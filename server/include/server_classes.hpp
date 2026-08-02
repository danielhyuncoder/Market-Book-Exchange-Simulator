#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <deque>
#include "../server_config.h"
#include "conversions.hpp"
#include "enums.hpp"
#include <boost/lockfree/queue.hpp>

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

namespace ORDER_BOOK_PACKAGE{
    class MARKET_BOOK;
};


namespace SERVER_PACKAGE {
    class SessionRegistry {
        public:
        static SessionRegistry& instance() {
            static SessionRegistry reg;
            return reg;
        }
        void add(LL id, std::shared_ptr<MatchingSession> sp) {
            std::lock_guard<std::mutex> lock(mtx_);
            sessions_[id] = sp;
        }
        void remove(LL id) {
            std::lock_guard<std::mutex> lock(mtx_);
            sessions_.erase(id);
        }
        std::shared_ptr<MatchingSession> lock(LL id) {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = sessions_.find(id);
            if (it == sessions_.end()) return nullptr;
            return it->second.lock();
        }
        private:
        std::mutex mtx_;
        std::unordered_map<LL, std::weak_ptr<MatchingSession>> sessions_;
    };
    class MatchingSession : public std::enable_shared_from_this<MatchingSession> {
      public:
      MatchingSession(tcp::socket socket, LL session_id)
        : socket_(std::move(socket)), session_id_(session_id) {}

      void start(ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book) {
        readListener(market_book);
      }
      void writeToClient(std::string msg){
        boost::asio::write(this->socket_, boost::asio::buffer(msg));

      }
      LL id() const { return session_id_; }
      private:

      bool simpleReject(ORDER& current_order){
          if (current_order.price_level<=0) {
              this->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(INVALID_PRICE, 4));return true;
          } 
          if (current_order.qty<=0){
              this->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(INVALID_QUANTITY, 4));return true;
          }
          return false;
      }
      void readListener(ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book);
      tcp::socket socket_;
      std::array<char, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES> data_;
      std::mutex mtx;
      LL session_id_;
    };

    class MatchingEngine {
         public:
         MatchingEngine(boost::asio::io_context& io_context_ref, ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book) : acceptor(io_context_ref, tcp::endpoint(tcp::v4(), SERVER_PORT)){
            this->engineListener(market_book, io_context_ref);
         }
         private:
         void engineListener( ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book, boost::asio::io_context& io_context_ref ){
            this->acceptor.async_accept(
            [this, &market_book, &io_context_ref](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    LL sid = next_session_id_.fetch_add(1);
                    auto ptr = std::make_shared<MatchingSession>(std::move(socket), sid);
                    SessionRegistry::instance().add(sid, ptr);
                    ptr->start(market_book);
                }
                this->engineListener(market_book, io_context_ref); 
            });
         }
         std::atomic<LL> next_session_id_{1};
         tcp::acceptor acceptor;
    };
    class OB_MCAST_FEED{
         public:
         OB_MCAST_FEED(boost::asio::io_context& io_context_ref, const LL port) : snapshots(SNAPSHOT_QUEUE_SIZE), multicast_ep(boost::asio::ip::make_address(MULTICAST_IP) ,port) {
             this->sock_ptr = std::make_shared<udp::socket>(io_context_ref, udp::v4());
         }
         void SEND_BROADCAST(OB_SNAPSHOT snapshot, std::string symbol, LL seq_len){
            this->sock_ptr->send_to(boost::asio::buffer(CONVERSION_PACKAGE::SNAPSHOT_TO_BYTES(snapshot, std::move(symbol), seq_len), 4 + 8 + (2*(24 * SNAPSHOT_LEN))), multicast_ep);
         }
         private:
         
         udp::endpoint multicast_ep;
         boost::lockfree::queue<OB_SNAPSHOT> snapshots;
         std::shared_ptr<udp::socket> sock_ptr;
    };

    class ServerHandler {
         public:
         ServerHandler();
         private:
         std::vector<std::thread> threads;
         boost::asio::io_context io_context;
         

    };

};