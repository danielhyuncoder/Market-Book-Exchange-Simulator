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
using boost::asio::ip::tcp;

namespace SERVER_PACKAGE {
    class MatchingSession : public std::enable_shared_from_this<MatchingSession> {
      public:
      MatchingSession(tcp::socket socket)
        : socket_(std::move(socket)) {}

      void start() {
        readListener();
      }

      private:

      void readListener() {
        auto self = shared_from_this(); 
        socket_.async_read_some(
            boost::asio::buffer(data_),
            [this, self](boost::system::error_code ec, size_t bytes_read) {
                if (!ec) {
                    std::string msg(data_.data(), bytes_read);
                    std::cout << msg << std::endl;
                    try{
                         auto remote = socket_.remote_endpoint();
                         CONVERSION_PACKAGE::DECODE_SEND_ORDER(msg);
                         boost::asio::write(socket_, boost::asio::buffer("ORDER CONFIRMED|"));

                    } catch (std::exception& err){
                       
                    }
  
                }
                readListener();
            });
      }
      tcp::socket socket_;
      std::array<char, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES> data_;
      
    };

    class MatchingEngine {
         public:
         MatchingEngine(boost::asio::io_context& io_context_ref) : acceptor(io_context_ref, tcp::endpoint(tcp::v4(), SERVER_PORT)){
            this->engineListener();
         }
         private:
         void engineListener(){
            this->acceptor.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto ptr = std::make_shared<MatchingSession>(std::move(socket));
                    ptr->start();
                }
                this->engineListener(); 
            });
         }

         tcp::acceptor acceptor;
    };

    class ServerHandler {
         public:
         ServerHandler(){
             MatchingEngine engine(this->io_context);
             
             for (int i =0;i<NUM_THREADS;i++){
                this->threads.emplace_back([&]{
                   io_context.run();
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