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

#include "../server_config.h"
#include "conversions.hpp"
#include "enums.hpp"
#include "order_book.hpp"

using boost::asio::ip::tcp;

namespace SERVER_PACKAGE {
    class MatchingSession : public std::enable_shared_from_this<MatchingSession> {
      public:
      MatchingSession(tcp::socket socket)
        : socket_(std::move(socket)) {}

      void start(ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book) {
        readListener(market_book);
      }
      void writeToClient(std::string msg){
         std::lock_guard<std::mutex> guard(this->mtx);
         boost::asio::write(this->socket_, boost::asio::buffer(msg));
      }
      private:
      bool simpleReject(ORDER& current_order){
          if (current_order.price_level<=0) {
              boost::asio::write(socket_, boost::asio::buffer("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(INVALID_PRICE, 4)));return true;
          } 
          if (current_order.qty<=0){
              boost::asio::write(socket_, boost::asio::buffer("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(INVALID_QUANTITY, 4)));return true;
          }
          return false;
      }
      void readListener(ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book) {
        auto self = shared_from_this(); 
        socket_.async_read_some(
            boost::asio::buffer(data_, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES),
            [this, self, &market_book](boost::system::error_code ec, size_t bytes_read) {
                if (!ec) {
                    std::string msg(data_.data(), bytes_read);
                    ORDER current_order;
                    current_order.ptr=shared_from_this().get();
                    if (msg[0]=='O'){
                      current_order=CONVERSION_PACKAGE::DECODE_SEND_ORDER(msg);
                      bool s = simpleReject(current_order);
                      if (s) {readListener(market_book);return;}
                      this->writeToClient("A"+msg.substr(1, (int)msg.length()-1));
                    } else if (msg[0]=='U'){
                      current_order=CONVERSION_PACKAGE::DECODE_MODIFY_ORDER(msg);
                      bool s = simpleReject(current_order);
                      if (s) {readListener(market_book);return;}
                    } else if (msg[0]=='X'){
                      current_order=CONVERSION_PACKAGE::DECODE_KILL_ORDER(msg);
                    } else {
                      this->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(MALFORMED_REQUEST, 4));readListener(market_book); return;
                    }
                    current_order.order_id=market_book.assign_order_id();
                    market_book.market_orders->push(std::move(current_order));
  
                }
                readListener(market_book);
            });
      }
      tcp::socket socket_;
      std::array<char, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES> data_;
      std::mutex mtx;
    };

    class MatchingEngine {
         public:
         MatchingEngine(boost::asio::io_context& io_context_ref, ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book) : acceptor(io_context_ref, tcp::endpoint(tcp::v4(), SERVER_PORT)){
            this->engineListener(market_book);
         }
         private:
         void engineListener( ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book){
            this->acceptor.async_accept(
            [this, &market_book](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto ptr = std::make_shared<MatchingSession>(std::move(socket));
                    ptr->start(market_book);
                }
                this->engineListener(market_book); 
            });
         }
         
         tcp::acceptor acceptor;
    };

    class ServerHandler {
         public:
         ServerHandler(){
             ORDER_BOOK_PACKAGE::MARKET_BOOK market_book;
             MatchingEngine engine(this->io_context, market_book);
             for (int i =0;i<NUM_SERVER_THREADS;i++){
                this->threads.emplace_back([&]{
                   io_context.run();
                });
             }
             for (int i =0;i<NUM_MARKET_BOOK_THREADS;i++){
                this->threads.emplace_back([&]{
                   market_book.market_listener();
                });
             }
             for (auto& thread : this->threads) {
                thread.join();
             }
         }
         private:
         std::vector<std::thread> threads;
         boost::asio::io_context io_context;
         

    };
};