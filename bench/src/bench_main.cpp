#include <benchmark/benchmark.h>
#include "order_book.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <atomic>

using namespace ORDER_BOOK_PACKAGE;

static ORDER make_order(LL id, ORDER_TYPE side, LL price, LL qty) {
    ORDER o;
    o.order_id = id;
    o.order_type = side;
    o.price_level = price;
    o.qty = qty;
    o.request_type = REQUEST_TYPE::SEND_ORDER;
    return o;
}

static void BM_SendOrder_NoMatch(benchmark::State& state) {
    ORDER_BOOK book;
    LL id = 1;
    for (auto _ : state) {
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, 100, 10));
    }
}
BENCHMARK(BM_SendOrder_NoMatch);

static void BM_SendOrder_FullMatch(benchmark::State& state) {
    ORDER_BOOK book;
    LL id = 1;
    for (auto _ : state) {
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::SELL, 100, 10));
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, 100, 10));
        book.UPDATE_BOOK();
    }
}
BENCHMARK(BM_SendOrder_FullMatch);

static void BM_CreateSnapshot(benchmark::State& state) {
    ORDER_BOOK book;
    for (LL i = 0; i < SNAPSHOT_LEN; i++) {
        book.SEND_ORDER(make_order(i, ORDER_TYPE::BUY, 100 - i, 10));
        book.SEND_ORDER(make_order(i + 1000, ORDER_TYPE::SELL, 200 + i, 10));
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.CREATE_SNAPSHOT());
    }
}
BENCHMARK(BM_CreateSnapshot);

static void BM_SendOrder_AtDepth(benchmark::State& state) {
    ORDER_BOOK book;
    LL depth = state.range(0);
    LL id = 1;
    for (LL i = 0; i < depth; i++) {
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, i, 10));
    }
    for (auto _ : state) {
        LL price = id % depth;
        book.SEND_ORDER(make_order(id++, ORDER_TYPE::BUY, price, 10));
    }
}
BENCHMARK(BM_SendOrder_AtDepth)->Arg(100)->Arg(1000)->Arg(10000)->Arg(100000);

// MARKET_BOOK benchmarks

static std::string make_symbol(int i) {
    char buf[5];
    snprintf(buf, sizeof(buf), "S%03d", i % 1000);   // 4 chars, matches SYMBOL_BYTES
    return std::string(buf);
}

using ORDER_BOOK_PACKAGE::hash_symbol;


static std::vector<std::string> make_one_symbol_per_shard() {
    std::vector<std::string> result(NUM_SHARDS);
    std::vector<bool> found(NUM_SHARDS, false);
    int found_count = 0;
    for (int i = 0; found_count < NUM_SHARDS; i++) {
        std::string sym = make_symbol(i);
        char bytes[SYMBOL_BYTES] = {0};
        std::memcpy(bytes, sym.data(), SYMBOL_BYTES);
        LL symbol_hash; size_t shard_index;
        hash_symbol(bytes, symbol_hash, shard_index);
        if (!found[shard_index]) {
            found[shard_index] = true;
            result[shard_index] = sym;
            found_count++;
        }
    }
    return result;
}

struct ShardDrainer {
    MARKET_BOOK* mb;
    std::atomic<bool> stop{false};
    std::thread worker;

    ShardDrainer(MARKET_BOOK* mb_, size_t shard_lo, size_t shard_hi) : mb(mb_) {
        worker = std::thread([this, shard_lo, shard_hi] {
            while (!stop.load(std::memory_order_relaxed)) {
                bool drained_any = false;
                for (size_t s = shard_lo; s < shard_hi; ++s) {
                    if (mb->drain_one(s)) drained_any = true;
                }
                if (!drained_any) std::this_thread::yield();
            }
            // Final sweep so anything pushed right before stop isn't lost.
            for (size_t s = shard_lo; s < shard_hi; ++s) {
                while (mb->drain_one(s)) {}
            }
        });
    }
    ~ShardDrainer() {
        stop.store(true, std::memory_order_relaxed);
        if (worker.joinable()) worker.join();
    }
};


static void BM_SubmitOrder_SameSymbol(benchmark::State& state) {
    static boost::asio::io_context ctx;
    static std::unique_ptr<MARKET_BOOK> mb;
    static std::unique_ptr<ShardDrainer> drainer;

    if (state.thread_index() == 0) {
        mb = std::make_unique<MARKET_BOOK>(ctx, std::vector<std::string>{"APPL"});
        LL symbol_hash; size_t shard_index;
        hash_symbol("APPL", symbol_hash, shard_index);
        drainer = std::make_unique<ShardDrainer>(mb.get(), shard_index, shard_index + 1);
    }

    LL id = state.thread_index() * 10000000LL;
    for (auto _ : state) {
        ORDER o = make_order(id++, ORDER_TYPE::BUY, 100, 10);
        std::memcpy(o.symbol, "APPL", SYMBOL_BYTES);   // every thread targets the SAME shard's queue
        mb->submit_order(std::move(o));
    }

    if (state.thread_index() == 0) {
        drainer.reset();   // stop + join before the next configuration reuses these statics
    }
}
BENCHMARK(BM_SubmitOrder_SameSymbol)->ThreadRange(1, 16);

static void BM_SubmitOrder_Spread(benchmark::State& state) {
    static boost::asio::io_context ctx;
    static std::unique_ptr<MARKET_BOOK> mb;
    static std::unique_ptr<ShardDrainer> drainer;
    static std::vector<std::string> shard_symbols;

    if (state.thread_index() == 0) {
        shard_symbols = make_one_symbol_per_shard();
        mb = std::make_unique<MARKET_BOOK>(ctx, shard_symbols);
        drainer = std::make_unique<ShardDrainer>(mb.get(), 0, NUM_SHARDS);
    }

    // guaranteed to be the symbol that maps to shard == thread_index()
    std::string sym = shard_symbols[state.thread_index() % NUM_SHARDS];
    LL id = state.thread_index() * 10000000LL;
    for (auto _ : state) {
        ORDER o = make_order(id++, ORDER_TYPE::BUY, 100, 10);
        std::memcpy(o.symbol, sym.data(), SYMBOL_BYTES);
        mb->submit_order(std::move(o));
    }

    if (state.thread_index() == 0) {
        drainer.reset();
    }
}
BENCHMARK(BM_SubmitOrder_Spread)->ThreadRange(1, 16);