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
#include <deque>
#include <map> 
#include <future>
#include "conversions.hpp"
#include "server_classes.hpp"

#include <fstream>
#include <vector>

namespace SERVER_PACKAGE{
    class MatchingSession;
};

namespace ORDER_BOOK_PACKAGE {
    using BID_SPREAD_TYPE=std::map<LL, std::deque<ORDER>, std::greater<LL>>;
    using ASK_SPREAD_TYPE=std::map<LL, std::deque<ORDER>>;
    
    class alignas(64) ORDER_BOOK {
        public:
        std::atomic<LL> seq_len;
        ORDER_BOOK() : seq_len(0){
            
        }
        void SEND_ORDER(ORDER order){
            //this->seq_len.fetch_add(1);
            //this->killed_orders[order.del_id]=false;
            this->killed_orders[order.order_id]=false;
            if (order.order_type == ORDER_TYPE::BUY){
                this->bids[order.price_level].push_back(std::move(order));
            } else {
                this->asks[order.price_level].push_back(std::move(order));
            }
        
        }
        void KILL_ORDER(ORDER& order){
            //this->seq_len.fetch_add(1);
            if (this->killed_orders.find(order.del_id)==this->killed_orders.end()) {
                if (auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(order.session_id)) {
                    sp->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(INVALID_ORDER_ID, 4));
                }
                return;
            }
            this->killed_orders[order.del_id]=true;
        }
        void MODIFY_ORDER(ORDER order){
            //this->seq_len.fetch_add(-1);
            this->KILL_ORDER(order);
            order.del_id=0;
            this->SEND_ORDER(order);
        }
        void NOTIFY_FILL(ORDER& order, LL fill_qty, LL remaining_qty, LL fill_price){
            auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(order.session_id);
            if (!sp) return;
            if (remaining_qty==0){
                sp->writeToClient(CONVERSION_PACKAGE::ENCODE_FULL_FILL(order, fill_qty, fill_price));
                this->killed_orders.erase(order.order_id);
            } else {
                sp->writeToClient(CONVERSION_PACKAGE::ENCODE_PARTIAL_FILL(order, fill_qty, remaining_qty, fill_price));
            }
        }
        void SEND_FILLED(ORDER& order){
           // order.ptr->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes(INVALID_ORDER_ID, 4));
        }
        void UPDATE_BOOK(){
            if (this->bids.empty()||this->asks.empty()) return;
            while (true){
               this->REMOVE_ORDERS<BID_SPREAD_TYPE>(this->bids);
               this->REMOVE_ORDERS<ASK_SPREAD_TYPE>(this->asks);
               if (this->bids.empty()||this->asks.empty()) break;
               // implement matching logic
               ORDER& bid_order = this->bids.begin()->second.front();
               ORDER& ask_order = this->asks.begin()->second.front();
               if (bid_order.price_level >= ask_order.price_level){
                    std::deque<ORDER>& bid_orders = this->bids.begin()->second;
                    std::deque<ORDER>& ask_orders = this->asks.begin()->second;
                    LL fill_qty = std::min(bid_order.qty, ask_order.qty);
                    LL fill_price = ask_order.price_level;
                    bid_order.qty -= fill_qty;
                    ask_order.qty -= fill_qty;
                    this->NOTIFY_FILL(bid_order, fill_qty, bid_order.qty, fill_price);
                    this->NOTIFY_FILL(ask_order, fill_qty, ask_order.qty, fill_price);
                    if (bid_order.qty==0) bid_orders.pop_front();
                    if (ask_order.qty==0) ask_orders.pop_front();
                    if (bid_orders.empty()) { LL k=this->bids.begin()->first; this->bids.erase(k); }
                    if (ask_orders.empty()) { LL k=this->asks.begin()->first; this->asks.erase(k); }
                } else break;
              

            }
        }
        OB_SNAPSHOT CREATE_SNAPSHOT(){
            OB_SNAPSHOT snapshot;
            int l = 0;
            for (auto& orders : this->bids){
                if (l>=SNAPSHOT_LEN) break;
                for (ORDER& order : orders.second){
                    if (l>=SNAPSHOT_LEN) break;
                    auto k_it = this->killed_orders.find(order.order_id);
                    if (k_it != this->killed_orders.end() && k_it->second) continue;
                    snapshot.buy_side[l++]=order;
                }
            }
            l=0;
            for (auto& orders : this->asks){
                if (l>=SNAPSHOT_LEN) break;
                for (ORDER& order : orders.second){
                    if (l>=SNAPSHOT_LEN) break;
                    auto k_it = this->killed_orders.find(order.order_id);
                    if (k_it != this->killed_orders.end() && k_it->second) continue;
                    snapshot.sell_side[l++]=order;
                }
            }
            return std::move(snapshot);
        }
        private:
        BID_SPREAD_TYPE bids;
        ASK_SPREAD_TYPE asks;
        std::unordered_map<LL, bool> killed_orders;
        
        template <typename spread_t>
        void REMOVE_ORDERS(spread_t& spread){
            while (!spread.empty()){
                auto k_it = this->killed_orders.find(spread.begin()->second.front().order_id);
                if (k_it == this->killed_orders.end() || !k_it->second) break;
                std::deque<ORDER>& orders = spread.begin()->second;
                ORDER& top_order = orders.front();
                if (auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(top_order.session_id)) {
                    sp->writeToClient(CONVERSION_PACKAGE::ENCODE_KILL_CONFIRM(top_order));
                }
                this->killed_orders.erase(top_order.order_id);
                orders.pop_front();
                LL it=spread.begin()->first;
                if (orders.empty()) spread.erase(it);
            }
        }
        

    };
    struct alignas(64) ORDER_BOOK_SHARD {
        ORDER_BOOK_SHARD(boost::asio::io_context& ctx,LL port) : snapshot_broadcaster(ctx, port),port(port) {

        }
        ORDER_BOOK_SHARD(ORDER_BOOK_SHARD&&) = default; 
        std::shared_mutex mtx;
        std::unordered_map<LL, ORDER_BOOK> priv_mp;
        SERVER_PACKAGE::OB_MCAST_FEED snapshot_broadcaster;
        LL port =0;
    };
    class MARKET_BOOK {
        
        public:
        std::unique_ptr<boost::lockfree::queue<ORDER>> market_orders = std::make_unique<boost::lockfree::queue<ORDER>>(QUEUE_SIZE);
        void market_listener() {
            
            while (true) {
                ORDER order;
                if (!this->market_orders->pop(order)) continue;
                std::string str = "";
                for (int i =0;i<SYMBOL_BYTES;i++){
                    str+=order.symbol[i];
                }
          
                LL order_hash = this->shard_hash_func(str);

                LL symbol_hash=order_hash;
                
                order_hash %= NUM_SHARDS;
                std::unique_lock<std::shared_mutex> guard(this->shards[order_hash]->mtx);
                if (this->shards[order_hash]->priv_mp.find(symbol_hash)==this->shards[order_hash]->priv_mp.end()){
                    if (auto sp = SERVER_PACKAGE::SessionRegistry::instance().lock(order.session_id)) {
                        sp->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(SYMBOL_NOT_FOUND, 4));
                    }
                    
                    continue;
                }

                this->shards[order_hash]->priv_mp[symbol_hash].seq_len.fetch_add(1);
                if (order.request_type == REQUEST_TYPE::SEND_ORDER) {
                    this->shards[order_hash]->priv_mp[symbol_hash].SEND_ORDER(std::move(order));
                } else if (order.request_type == REQUEST_TYPE::MODIFY_ORDER){
                    this->shards[order_hash]->priv_mp[symbol_hash].MODIFY_ORDER(std::move(order));
                } else {
                    this->shards[order_hash]->priv_mp[symbol_hash].KILL_ORDER(order);
                }
                this->shards[order_hash]->priv_mp[symbol_hash].UPDATE_BOOK();
                LL seq_len=this->shards[order_hash]->priv_mp[symbol_hash].seq_len.load();
                if ((seq_len % SNAPSHOT_FREQUENCY) == 0){
                    std::string sym = "";
                    for (int i =0;i<4;i++) sym += order.symbol[i];
                    std::future<void> res = std::async(std::launch::async, &SERVER_PACKAGE::OB_MCAST_FEED::SEND_BROADCAST, &this->shards[order_hash]->snapshot_broadcaster, this->shards[order_hash]->priv_mp[symbol_hash].CREATE_SNAPSHOT(), std::move(sym),seq_len);
                }
                
            }
        }
        LL assign_order_id() {
            return current_id.fetch_add(1);
        }
        LL get_order_id() {
            return current_id.load();
        }
        MARKET_BOOK(boost::asio::io_context& ctx, std::vector<std::string> symbols) : current_id(1) {
           for (int i =0;i<NUM_SHARDS;i++){
               this->shards.push_back(std::make_shared<ORDER_BOOK_SHARD>(ctx, (long long)(30001+i)));
           }
           for (std::string& symbol : symbols){
               LL hash = shard_hash_func(symbol);

               this->shards[hash%NUM_SHARDS]->priv_mp[hash];
               if (PRINT_SYMBOL_HASHES) std::cout << symbol << " PORT -> " << (30001+(hash%NUM_SHARDS)) << std::endl;
           }

           
        }
        private:
        std::vector<std::shared_ptr<ORDER_BOOK_SHARD>> shards;
        std::atomic<LL> current_id;
        std::hash<std::string> shard_hash_func;
    };
};