#pragma once
#include <iostream>
#include <memory>
#include "enums.hpp"
#include <boost/lockfree/queue.hpp>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <array>
#include <functional>
#include <map>
#include <future>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include "conversions.hpp"
#include "server_classes.hpp"
#include <shared_mutex>
#include <fstream>
#include <vector>

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace SERVER_PACKAGE{
    class MatchingSession;
};

namespace ORDER_BOOK_PACKAGE {

    inline int clz64(uint64_t v) {
#if defined(_MSC_VER)
        unsigned long idx;
        _BitScanReverse64(&idx, v);      // idx is already the bit index of the MSB
        return static_cast<int>(idx);
#else
        return 63 - __builtin_clzll(v);  // clzll counts leading zeros, convert to bit index
#endif
    }
    inline int ctz64(uint64_t v) {
#if defined(_MSC_VER)
        unsigned long idx;
        _BitScanForward64(&idx, v);
        return static_cast<int>(idx);
#else
        return __builtin_ctzll(v);
#endif
    }


    static constexpr uint32_t PRICE_WINDOW_LOG2       = 16;                 // 65536 price ticks of coverage
    static constexpr uint32_t PRICE_WINDOW            = 1u << PRICE_WINDOW_LOG2;
    static constexpr uint32_t PRICE_MASK              = PRICE_WINDOW - 1;
    static constexpr uint32_t L0_WORDS                = PRICE_WINDOW / 64;  // 1024
    static constexpr uint32_t L1_WORDS                = (L0_WORDS + 63) / 64; // 16
    static constexpr uint32_t ORDER_POOL_INITIAL_CAP  = 1u << 14;           // 16384 resting orders to start
    static constexpr uint32_t ORDER_POOL_GROWTH_CHUNK = 1u << 14;           // grow by 16384 slots at a time
    static constexpr uint32_t ORDER_POOL_MAX_CAP      = 1u << 24;           // ~16.7M hard ceiling
    static constexpr uint32_t NIL                     = 0xFFFFFFFFu;


    struct OrderNode {
        ORDER order;
        uint32_t prev = NIL;
        uint32_t next = NIL;
    };

    class OrderPool {
        public:
        OrderPool() {
            grow(ORDER_POOL_INITIAL_CAP);
        }
        uint32_t acquire(ORDER order) {
            if (free_head == NIL) {
                if (nodes.size() >= ORDER_POOL_MAX_CAP) {
                    throw std::runtime_error("ORDER_POOL exhausted - raise ORDER_POOL_MAX_CAP");
                }
                uint32_t chunk = std::min<uint32_t>(ORDER_POOL_GROWTH_CHUNK,
                                                     ORDER_POOL_MAX_CAP - static_cast<uint32_t>(nodes.size()));
                grow(chunk);
            }
            uint32_t idx = free_head;
            free_head = nodes[idx].next;
            nodes[idx].order = std::move(order);
            nodes[idx].prev = NIL;
            nodes[idx].next = NIL;
            return idx;
        }
        void release(uint32_t idx) {
            nodes[idx].next = free_head;
            free_head = idx;
        }
        OrderNode& operator[](uint32_t idx) { return nodes[idx]; }

        private:
        // Appends `count` new nodes to the end of `nodes` and links them onto
        // the freelist. Existing indices are untouched by the vector growth -
        // std::vector may reallocate its backing buffer, but callers only ever
        // hold indices (never raw pointers/references across an acquire()),
        // so nothing already resting in the book is invalidated.
        void grow(uint32_t count) {
            if (count == 0) return;
            size_t old_size = nodes.size();
            nodes.resize(old_size + count);
            for (size_t i = old_size; i + 1 < nodes.size(); ++i) nodes[i].next = static_cast<uint32_t>(i + 1);
            nodes.back().next = free_head; // splice new run onto whatever was already free (NIL if none)
            free_head = static_cast<uint32_t>(old_size);
        }

        std::vector<OrderNode> nodes;
        uint32_t free_head = NIL;
    };

   
    struct PriceLevel {
        uint32_t head = NIL;
        uint32_t tail = NIL;
        uint32_t count = 0;
        LL level_price = 0; // real price of a slot

        bool empty() const { return count == 0; }
    };

    inline void level_push_back(PriceLevel& lvl, OrderPool& pool, ORDER order) {
        uint32_t idx = pool.acquire(std::move(order));
        if (lvl.tail == NIL) {
            lvl.head = lvl.tail = idx;
        } else {
            pool[lvl.tail].next = idx;
            pool[idx].prev = lvl.tail;
            lvl.tail = idx;
        }
        ++lvl.count;
    }
    inline ORDER& level_front(PriceLevel& lvl, OrderPool& pool) {
        return pool[lvl.head].order;
    }
    inline void level_pop_front(PriceLevel& lvl, OrderPool& pool) {
        uint32_t idx = lvl.head;
        uint32_t nxt = pool[idx].next;
        lvl.head = nxt;
        if (nxt != NIL) pool[nxt].prev = NIL; else lvl.tail = NIL;
        pool.release(idx);
        --lvl.count;
    }

 
    class PriceBitmap {
        public:
        void set(uint32_t idx) {
            l0[idx >> 6] |= (1ULL << (idx & 63));
            l1[idx >> 12] |= (1ULL << ((idx >> 6) & 63));
        }
        void clear(uint32_t idx) {
            uint64_t& w = l0[idx >> 6];
            w &= ~(1ULL << (idx & 63));
            if (w == 0) l1[idx >> 12] &= ~(1ULL << ((idx >> 6) & 63));
        }
        bool find_highest(uint32_t& out) const {
            for (int w1 = (int)L1_WORDS - 1; w1 >= 0; --w1) {
                if (l1[w1]) {
                    int b1 = clz64(l1[w1]);
                    uint32_t word_idx = w1 * 64 + b1;
                    int b0 = clz64(l0[word_idx]);
                    out = word_idx * 64 + b0;
                    return true;
                }
            }
            return false;
        }
        bool find_lowest(uint32_t& out) const {
            for (uint32_t w1 = 0; w1 < L1_WORDS; ++w1) {
                if (l1[w1]) {
                    int b1 = ctz64(l1[w1]);
                    uint32_t word_idx = w1 * 64 + b1;
                    int b0 = ctz64(l0[word_idx]);
                    out = word_idx * 64 + b0;
                    return true;
                }
            }
            return false;
        }
        // Visitor returns true to keep going, false to stop early.
        // Only used for snapshotting (bounded by SNAPSHOT_LEN), not on the hot path.
        template <typename F>
        void for_each_descending(F&& fn) const {
            for (int w1 = (int)L1_WORDS - 1; w1 >= 0; --w1) {
                uint64_t bits1 = l1[w1];
                while (bits1) {
                    int b1 = clz64(bits1);
                    uint32_t word_idx = w1 * 64 + b1;
                    uint64_t bits0 = l0[word_idx];
                    while (bits0) {
                        int b0 = clz64(bits0);
                        if (!fn(word_idx * 64 + b0)) return;
                        bits0 &= ~(1ULL << b0);
                    }
                    bits1 &= ~(1ULL << b1);
                }
            }
        }
        template <typename F>
        void for_each_ascending(F&& fn) const {
            for (uint32_t w1 = 0; w1 < L1_WORDS; ++w1) {
                uint64_t bits1 = l1[w1];
                while (bits1) {
                    int b1 = ctz64(bits1);
                    uint32_t word_idx = w1 * 64 + b1;
                    uint64_t bits0 = l0[word_idx];
                    while (bits0) {
                        int b0 = ctz64(bits0);
                        if (!fn(word_idx * 64 + b0)) return;
                        bits0 &= ~(1ULL << b0);
                    }
                    bits1 &= ~(1ULL << b1);
                }
            }
        }
        private:
        std::array<uint64_t, L0_WORDS> l0{};
        std::array<uint64_t, L1_WORDS> l1{};
    };

    // ---------------------------------------------------------------------
    // Flat open-addressing table for the killed-order flags. Replaces
    // std::unordered_map<LL,bool>, which is node-based and heap-allocates on
    // every insert - that allocation was the dominant cost in SEND_ORDER
    // even after the price-level logic became array-based. This is a single
    // contiguous array: fibonacci hashing, linear probing, tombstones for
    // deletion, doubling growth. No allocation on the hot path once warmed up.
    //
    // ASSUMPTION: order_id/del_id are always >= 0 (true for the sequential
    // ids handed out by MARKET_BOOK::assign_order_id(), which starts at 1).
    // Two keys are reserved as sentinels: -1 marks an empty slot, -2 marks a
    // deleted one. If your ids can be negative, change these sentinels to
    // values outside your id range.
    // ---------------------------------------------------------------------
    class KillFlagTable {
        public:
        KillFlagTable() { rehash(1u << 12); } // 4096 slots to start

        // Insert-or-assign, mirrors killed_orders[key] = value
        void set(LL key, bool value) {
            maybe_grow();
            uint32_t idx = locate_for_insert(key);
            LL prev_key = slots[idx].key;
            if (prev_key != key) {
                if (prev_key == TOMBSTONE_KEY) --tombstone_count;
                ++live_count;
                slots[idx].key = key;
            }
            slots[idx].value = value;
        }

        // Returns a pointer to the flag if present, else nullptr - use in
        // place of `find(key) == end()` / `it->second`.
        bool* find(LL key) {
            uint32_t idx = locate_for_find(key);
            return (idx == INVALID) ? nullptr : &slots[idx].value;
        }

        void erase(LL key) {
            uint32_t idx = locate_for_find(key);
            if (idx == INVALID) return;
            slots[idx].key = TOMBSTONE_KEY;
            --live_count;
            ++tombstone_count;
        }

        private:
        static constexpr LL EMPTY_KEY     = -1;
        static constexpr LL TOMBSTONE_KEY = -2;
        static constexpr uint32_t INVALID = 0xFFFFFFFFu;

        struct Slot { LL key = EMPTY_KEY; bool value = false; };

        std::vector<Slot> slots;
        uint32_t mask = 0;
        uint32_t live_count = 0;
        uint32_t tombstone_count = 0;

        static uint32_t hash_key(LL key) {
            uint64_t h = static_cast<uint64_t>(key) * 0x9E3779B97F4A7C15ULL; // fibonacci hashing
            return static_cast<uint32_t>(h >> 32);
        }

        uint32_t locate_for_find(LL key) const {
            uint32_t idx = hash_key(key) & mask;
            for (;;) {
                const Slot& s = slots[idx];
                if (s.key == key) return idx;
                if (s.key == EMPTY_KEY) return INVALID; // empty slot => not present, probe chain ends
                idx = (idx + 1) & mask;
            }
        }

        // Finds the slot to write `key` into: the existing slot with that
        // key if present (to update in place), otherwise the first empty or
        // tombstone slot along the probe sequence (to insert fresh, reusing
        // tombstones so probe chains don't grow unbounded from churn).
        uint32_t locate_for_insert(LL key) {
            uint32_t idx = hash_key(key) & mask;
            uint32_t first_tombstone = INVALID;
            for (;;) {
                Slot& s = slots[idx];
                if (s.key == key) return idx;
                if (s.key == EMPTY_KEY) return (first_tombstone != INVALID) ? first_tombstone : idx;
                if (s.key == TOMBSTONE_KEY && first_tombstone == INVALID) first_tombstone = idx;
                idx = (idx + 1) & mask;
            }
        }

        void maybe_grow() {
            // Keep (live + tombstones) under ~60% load factor so probe
            // chains stay short (O(1) amortized). +1 accounts for the
            // insert about to happen.
            if ((static_cast<uint64_t>(live_count) + tombstone_count + 1) * 10 >= slots.size() * 6) {
                rehash(static_cast<uint32_t>(slots.size() * 2));
            }
        }

        void rehash(uint32_t new_capacity) {
            std::vector<Slot> old = std::move(slots);
            slots.assign(new_capacity, Slot{});
            mask = new_capacity - 1;
            live_count = 0;
            tombstone_count = 0;
            for (const Slot& s : old) {
                if (s.key != EMPTY_KEY && s.key != TOMBSTONE_KEY) {
                    uint32_t idx = locate_for_insert(s.key);
                    slots[idx].key = s.key;
                    slots[idx].value = s.value;
                    ++live_count;
                }
            }
        }
    };

    class alignas(64) ORDER_BOOK {
        public:
        std::atomic<LL> seq_len;

        ORDER_BOOK() : seq_len(0) {
            bids = std::make_unique<std::array<PriceLevel, PRICE_WINDOW>>();
            asks = std::make_unique<std::array<PriceLevel, PRICE_WINDOW>>();
        }

        void SEND_ORDER(ORDER order) {
            //this->seq_len.fetch_add(1);
            //this->killed_orders[order.del_id]=false;
            killed_orders.set(order.order_id, false);
            if (order.order_type == ORDER_TYPE::BUY) {
                insert(*bids, bid_bitmap, order);
            } else {
                insert(*asks, ask_bitmap, order);
            }
        }

        void KILL_ORDER(ORDER& order) {
            //this->seq_len.fetch_add(1);
            bool* kf = killed_orders.find(order.del_id);
            if (!kf) {
                if (auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(order.session_id)) {
                    sp->writeToClient("J" + CONVERSION_PACKAGE::number_to_bytes<LL>(INVALID_ORDER_ID, 4));
                }
                return;
            }
            *kf = true;
        }

        void MODIFY_ORDER(ORDER order) {
            //this->seq_len.fetch_add(-1);
            this->KILL_ORDER(order);
            order.del_id = 0;
            this->SEND_ORDER(std::move(order));
        }

        void NOTIFY_FILL(ORDER& order, LL fill_qty, LL remaining_qty, LL fill_price) {
            auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(order.session_id);
            if (!sp) return;
            if (remaining_qty == 0) {
                sp->writeToClient(CONVERSION_PACKAGE::ENCODE_FULL_FILL(order, fill_qty, fill_price));
                this->killed_orders.erase(order.order_id);
            } else {
                sp->writeToClient(CONVERSION_PACKAGE::ENCODE_PARTIAL_FILL(order, fill_qty, remaining_qty, fill_price));
            }
        }

        void SEND_FILLED(ORDER& order){
           // order.ptr->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes(INVALID_ORDER_ID, 4));
        }
        void UPDATE_BOOK() {
            while (true) {
                drain_dead(*bids, bid_bitmap, /*best_is_high=*/true);
                drain_dead(*asks, ask_bitmap, /*best_is_high=*/false);

                uint32_t bidx, aidx;
                if (!bid_bitmap.find_highest(bidx)) break;
                if (!ask_bitmap.find_lowest(aidx)) break;

                PriceLevel& bl = (*bids)[bidx];
                PriceLevel& al = (*asks)[aidx];
                ORDER& bo = level_front(bl, pool);
                ORDER& ao = level_front(al, pool);
                if (bo.price_level < ao.price_level) break;

                LL fill_qty = std::min(bo.qty, ao.qty);
                LL fill_price = ao.price_level;
                bo.qty -= fill_qty;
                ao.qty -= fill_qty;
                this->NOTIFY_FILL(bo, fill_qty, bo.qty, fill_price);
                this->NOTIFY_FILL(ao, fill_qty, ao.qty, fill_price);

                if (bo.qty == 0) { level_pop_front(bl, pool); if (bl.empty()) bid_bitmap.clear(bidx); }
                if (ao.qty == 0) { level_pop_front(al, pool); if (al.empty()) ask_bitmap.clear(aidx); }
            }
        }

        OB_SNAPSHOT CREATE_SNAPSHOT() {
            OB_SNAPSHOT snapshot;
            int l = 0;
            bid_bitmap.for_each_descending([&](uint32_t idx) {
                PriceLevel& lvl = (*bids)[idx];
                uint32_t cur = lvl.head;
                while (cur != NIL && l < SNAPSHOT_LEN) {
                    OrderNode& n = pool[cur];
                    bool* kf = killed_orders.find(n.order.order_id);
                    if (!(kf && *kf)) snapshot.buy_side[l++] = n.order;
                    cur = n.next;
                }
                return l < SNAPSHOT_LEN;
            });
            l = 0;
            ask_bitmap.for_each_ascending([&](uint32_t idx) {
                PriceLevel& lvl = (*asks)[idx];
                uint32_t cur = lvl.head;
                while (cur != NIL && l < SNAPSHOT_LEN) {
                    OrderNode& n = pool[cur];
                    bool* kf = killed_orders.find(n.order.order_id);
                    if (!(kf && *kf)) snapshot.sell_side[l++] = n.order;
                    cur = n.next;
                }
                return l < SNAPSHOT_LEN;
            });
            return snapshot;
        }

        private:
        static uint32_t index_of(LL price) { return static_cast<uint32_t>(price) & PRICE_MASK; }

        void insert(std::array<PriceLevel, PRICE_WINDOW>& side, PriceBitmap& bmp, ORDER& order) {
            uint32_t idx = index_of(order.price_level);
            PriceLevel& lvl = side[idx];
            if (lvl.empty()) {
                lvl.level_price = order.price_level;
                bmp.set(idx);
            }
            level_push_back(lvl, pool, std::move(order));
        }

        // Lazily evict killed orders sitting at the front of the current best
        // level - same lazy-deletion strategy as the original REMOVE_ORDERS.
        void drain_dead(std::array<PriceLevel, PRICE_WINDOW>& side, PriceBitmap& bmp, bool best_is_high) {
            while (true) {
                uint32_t idx;
                bool has = best_is_high ? bmp.find_highest(idx) : bmp.find_lowest(idx);
                if (!has) return;
                PriceLevel& lvl = side[idx];
                ORDER& front = level_front(lvl, pool);
                bool* kf = killed_orders.find(front.order_id);
                if (!kf || !*kf) return; // top order alive, stop
                if (auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(front.session_id)) {
                    sp->writeToClient(CONVERSION_PACKAGE::ENCODE_KILL_CONFIRM(front));
                }
                killed_orders.erase(front.order_id);
                level_pop_front(lvl, pool);
                if (lvl.empty()) bmp.clear(idx);
            }
        }

        std::unique_ptr<std::array<PriceLevel, PRICE_WINDOW>> bids;
        std::unique_ptr<std::array<PriceLevel, PRICE_WINDOW>> asks;
        PriceBitmap bid_bitmap;
        PriceBitmap ask_bitmap;
        OrderPool pool;
        KillFlagTable killed_orders;
    };

    inline void hash_symbol(const char* symbol, LL& symbol_hash, size_t& shard_index) {
        uint32_t sym32 = 0;
        std::memcpy(&sym32, symbol, SYMBOL_BYTES);
        uint64_t mixed = static_cast<uint64_t>(sym32) * 0x9E3779B97F4A7C15ULL;
        symbol_hash = static_cast<LL>(mixed);
        shard_index = (mixed >> 48) & (NUM_SHARDS - 1);
    }

    struct alignas(64) ORDER_BOOK_SHARD {
        ORDER_BOOK_SHARD(boost::asio::io_context& ctx, LL port)
            : snapshot_broadcaster(ctx, port), port(port), queue(QUEUE_SIZE) {}
        ORDER_BOOK_SHARD(ORDER_BOOK_SHARD&&) = default;

        boost::lockfree::queue<ORDER> queue;
        std::unordered_map<LL, ORDER_BOOK> priv_mp;
        SERVER_PACKAGE::OB_MCAST_FEED snapshot_broadcaster;
        LL port = 0;
    };

    class MARKET_BOOK {
        public:
        void submit_order(ORDER order) {
            LL symbol_hash; size_t shard_index;
            hash_symbol(order.symbol, symbol_hash, shard_index);
            this->shards[shard_index]->queue.push(std::move(order));
        }

        bool drain_one(size_t shard_index) {
            ORDER order;
            if (!this->shards[shard_index]->queue.pop(order)) return false;
            LL symbol_hash; size_t sidx;
            hash_symbol(order.symbol, symbol_hash, sidx);
            process_one(sidx, order, symbol_hash);
            return true;
        }

        void market_listener(size_t shard_start, size_t shard_end) {
            while (true) {
                for (size_t s = shard_start; s < shard_end; ++s) drain_one(s);
            }
        }

        void process_one(size_t shard_index, ORDER& order, LL symbol_hash) {
            auto& shard = *this->shards[shard_index];
            
            if (shard.priv_mp.find(symbol_hash) == shard.priv_mp.end()) {
                if (auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(order.session_id)) {
                    sp->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(SYMBOL_NOT_FOUND, 4));
                }
                return;
            }
            
            ORDER_BOOK& book = shard.priv_mp[symbol_hash];

            book.seq_len.fetch_add(1);
            if (order.request_type == REQUEST_TYPE::SEND_ORDER) {
                book.SEND_ORDER(std::move(order));
            } else if (order.request_type == REQUEST_TYPE::MODIFY_ORDER) {
                book.MODIFY_ORDER(std::move(order));
            } else {
                book.KILL_ORDER(order);
            }
            book.UPDATE_BOOK();
            LL seq_len = book.seq_len.load();
            if ((seq_len % SNAPSHOT_FREQUENCY) == 0) {
                std::string sym = "";
                for (int i = 0; i < 4; i++) sym += order.symbol[i];

                 //std::future<void> res = std::async(std::launch::async, &SERVER_PACKAGE::OB_MCAST_FEED::SEND_BROADCAST, &shard.snapshot_broadcaster, book.CREATE_SNAPSHOT(), std::move(sym),seq_len);
                std::thread(&SERVER_PACKAGE::OB_MCAST_FEED::SEND_BROADCAST,
                            &shard.snapshot_broadcaster,
                            book.CREATE_SNAPSHOT(), std::move(sym), seq_len).detach();
            }
        }

        LL assign_order_id() { return current_id.fetch_add(1); }
        LL get_order_id() { return current_id.load(); }

        MARKET_BOOK(boost::asio::io_context& ctx, std::vector<std::string> symbols) : current_id(1) {
            for (int i = 0; i < NUM_SHARDS; i++) {
                this->shards.push_back(std::make_shared<ORDER_BOOK_SHARD>(ctx, (long long)(30001 + i)));
            }
            for (std::string& symbol : symbols) {
                char sym_bytes[SYMBOL_BYTES] = {0};
                std::memcpy(sym_bytes, symbol.data(), std::min<size_t>(SYMBOL_BYTES, symbol.size()));
                LL symbol_hash; size_t shard_index;
                hash_symbol(sym_bytes, symbol_hash, shard_index);
                this->shards[shard_index]->priv_mp[symbol_hash];
                if (PRINT_SYMBOL_HASHES) std::cout << symbol << " PORT -> " << (30001 + shard_index) << std::endl;
            }
        }

        private:
        std::vector<std::shared_ptr<ORDER_BOOK_SHARD>> shards;
        std::atomic<LL> current_id;
    };
};