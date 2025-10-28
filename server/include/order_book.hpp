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
#include <queue>
#include <map> 
#include "conversions.hpp"

namespace SERVER_PACKAGE{
    class MatchingSession;
};

namespace ORDER_BOOK_PACKAGE {
    using BID_SPREAD_TYPE=std::map<LL, std::queue<ORDER>, std::greater<LL>>;
    using ASK_SPREAD_TYPE=std::map<LL, std::queue<ORDER>>;
    class alignas(64) ORDER_BOOK {
        public:
        ORDER_BOOK() : seq_len(0){
            
        }
        void SEND_ORDER(ORDER order){
            this->killed_orders[order.del_id]=false;
            if (order.order_type == ORDER_TYPE::BUY){
                this->bids[order.price_level].push(std::move(order));
            } else {
                this->asks[order.price_level].push(std::move(order));
            }
        
        }
        void KILL_ORDER(ORDER& order){
            if (this->killed_orders.find(order.del_id)==this->killed_orders.end()) {
                order.ptr->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes(INVALID_ORDER_ID, 4));
                return;
            }
            this->killed_orders[order.del_id]=true;
        }
        void MODIFY_ORDER(ORDER order){
            this->KILL_ORDER(order);
            order.del_id=0;
            this->SEND_ORDER(order);
        }
        void SEND_FILLED(ORDER& order){
            order.ptr->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes(INVALID_ORDER_ID, 4));
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
                  std::queue<ORDER>& bid_orders = this->bids.begin()->second;
                  std::queue<ORDER>& ask_orders = this->asks.begin()->second;
                  if (bid_order.qty>=ask_order.qty){
                     bid_order.qty-=ask_order.qty;
                     ask_orders.pop();
                     if (bid_order.qty==0) bid_orders.pop();
                  } else {
                     ask_order.qty-=bid_order.qty;
                     bid_orders.pop();
                     if (ask_order.qty==0) ask_orders.pop();
                  }

                  // price level erasure logic
                  if (bid_orders.empty()) {
                     LL it_bid = this->bids.begin()->first;
                     this->bids.erase(it_bid);
                  }
                  if (ask_orders.empty()) {
                     LL it_ask = this->asks.begin()->first;
                     this->asks.erase(it_ask);
                  }
               } else break;
              

            }
        }
        private:
        BID_SPREAD_TYPE bids;
        ASK_SPREAD_TYPE asks;
        std::unordered_map<LL, bool> killed_orders;
        std::atomic<LL> seq_len;
        template <typename spread_t>
        void REMOVE_ORDERS(spread_t& spread){
            while (!spread.empty() && this->killed_orders.find(spread.begin()->second.front().order_id)!=this->killed_orders.end()){
                std::queue<ORDER>& orders = spread.begin()->second;
                ORDER& top_order = orders.front();
                top_order.ptr->writeToClient("C"+CONVERSION_PACKAGE::number_to_bytes(top_order.qty, 8));
                this->killed_orders.erase(top_order.order_id);
                orders.pop();
                LL it=spread.begin()->first;
                if (orders.empty()) spread.erase(it);
            }
        }
        

    };
    struct alignas(64) ORDER_BOOK_SHARD {
        std::shared_mutex mtx;
        std::unordered_map<LL, ORDER_BOOK> priv_mp;
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
                std::unique_lock<std::shared_mutex> guard(this->shards[order_hash].mtx);
                if (this->shards[order_hash].priv_mp.find(symbol_hash)==this->shards[order_hash].priv_mp.end()){
                    // err logic.
                }
                if (order.request_type == REQUEST_TYPE::SEND_ORDER) {
                    this->shards[order_hash].priv_mp[symbol_hash].SEND_ORDER(std::move(order));
                } else if (order.request_type == REQUEST_TYPE::MODIFY_ORDER){
                    this->shards[order_hash].priv_mp[symbol_hash].MODIFY_ORDER(std::move(order));
                } else {
                    this->shards[order_hash].priv_mp[symbol_hash].KILL_ORDER(order);
                }
                this->shards[order_hash].priv_mp[symbol_hash].UPDATE_BOOK();
            }
        }
        LL assign_order_id() {
            return current_id.fetch_add(1);
        }
        LL get_order_id() {
            return current_id.load();
        }
        MARKET_BOOK() : current_id(0) {
           
        }
        private:
        std::array<ORDER_BOOK_SHARD, NUM_SHARDS> shards;
        std::atomic<LL> current_id;
        std::hash<std::string> shard_hash_func;
    };
};