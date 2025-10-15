#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <array>
#include "../client_config.h"

using boost::asio::ip::tcp;
namespace CLIENT_PACKAGE {
     class StandardClient : public std::enable_shared_from_this<StandardClient> {
         public:
         StandardClient(){
             this->socketPtr=std::make_shared<tcp::socket>(io_context);
             this->socketPtr->open(tcp::v4());
             this->socketPtr->set_option(boost::asio::socket_base::reuse_address(true));
             tcp::endpoint local_endpoint(boost::asio::ip::make_address(CLIENT_IP_ADDRESS), CLIENT_PORT);
             this->socketPtr->bind(local_endpoint);
             tcp::endpoint server_endpoint(boost::asio::ip::make_address(SERVER_IP_ADDRESS), SERVER_PORT);
             this->socketPtr->connect(server_endpoint);
         };
         void SendOrder(const std::string& ORDER_TYPE, const std::string& SYMBOL, LD price_level, LD QTY){
             std::string tcp_message = ORDER_TYPE + "|" + SYMBOL + "|" + std::to_string(price_level) + "|" +std::to_string(QTY);
             boost::asio::write(*this->socketPtr, boost::asio::buffer(tcp_message));
         }
         void start(){
            this->OrderConfirmationRecievedListener();
            this->io_context.run();
         }
         private:
         void OrderConfirmationRecievedListener(){
            auto self_ptr = shared_from_this();
            this->socketPtr->async_read_some(boost::asio::buffer(data), [this, self_ptr](boost::system::error_code ec, size_t bytes_read) {
                if (!ec){
                    std::string msg(data.data(), bytes_read);
                    std::cout << msg << std::endl;
                }
                OrderConfirmationRecievedListener();
            });
         } 
         boost::asio::io_context io_context;
         std::shared_ptr<tcp::socket> socketPtr;  
         std::array<char, 1024> data;
     };
};