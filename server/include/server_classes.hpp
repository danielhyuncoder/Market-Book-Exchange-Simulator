#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <atomic>

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

      private:

      void readListener(ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book) {
        auto self = shared_from_this(); 
        socket_.async_read_some(
            boost::asio::buffer(data_, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES),
            [this, self, &market_book](boost::system::error_code ec, size_t bytes_read) {
                if (!ec) {
                    std::string msg(data_.data(), bytes_read);
                    ORDER current_order;
                    if (msg[0]=='S'){
                      current_order=CONVERSION_PACKAGE::DECODE_SEND_ORDER(msg);
                    } else if (msg[0]=='M'){
                      current_order=CONVERSION_PACKAGE::DECODE_MODIFY_ORDER(msg);
                    } else {
                      current_order=CONVERSION_PACKAGE::DECODE_KILL_ORDER(msg);
                    }
                    if (msg[0]=='S'||msg[0]=='M'){
                      
                      current_order.order_id=market_book.assign_order_id();
                      boost::asio::write(socket_, boost::asio::buffer("ORDER CONFIRMED|"+std::to_string(current_order.order_id)));
                    }
                    market_book.market_orders->push(std::move(current_order));
                   
                   

  
                }
                readListener(market_book);
            });
      }
      tcp::socket socket_;
      std::array<char, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES> data_;
      
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