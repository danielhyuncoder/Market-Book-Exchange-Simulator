#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <array>
#include "../client_config.h"
#include "./conversions.hpp"
#include "json.hpp"
#include <fstream>

using json = nlohmann::json;
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
         void SendOrder(const std::string& ORDER_TYPE, const std::string& SYMBOL, LL price_level, LL QTY){
             std::string tcp_message = "O" + ORDER_TYPE + SYMBOL +CONVERSION_PACKAGE::number_to_bytes<LL>(QTY, QTY_BYTES)+ CONVERSION_PACKAGE::number_to_bytes<LL>(price_level, PRICE_BYTES);
             CONVERSION_PACKAGE::pad_message(tcp_message);
             boost::asio::write(*this->socketPtr, boost::asio::buffer(tcp_message));
         }
         void SendModify(const std::string& ORDER_TYPE, const std::string& SYMBOL, LL order_id, LL price_level, LL QTY){
             std::string tcp_message = "U" + ORDER_TYPE+ SYMBOL + CONVERSION_PACKAGE::number_to_bytes<LL>(QTY, QTY_BYTES)+ CONVERSION_PACKAGE::number_to_bytes<LL>(price_level, PRICE_BYTES) + CONVERSION_PACKAGE::number_to_bytes<LL>(order_id, ID_BYTES);
             CONVERSION_PACKAGE::pad_message(tcp_message);
             boost::asio::write(*this->socketPtr, boost::asio::buffer(tcp_message));
         }
         void SendKill(const std::string& SYMBOL, LL order_id){
             std::string tcp_message = "X" + SYMBOL + CONVERSION_PACKAGE::number_to_bytes<LL>(order_id, ID_BYTES);
             CONVERSION_PACKAGE::pad_message(tcp_message);
             boost::asio::write(*this->socketPtr, boost::asio::buffer(tcp_message));
         }
         void start(){
            this->ClientTCPListener();
            this->io_context.run();
         }
         void ProcessOrdersFromJSON() {
            std::ifstream pFile(ORDERS_PATH);
            if (!pFile.is_open()) {
               std::cerr << "Failed to open orders.json" << std::endl;
               return;
            }
            json j;
            pFile>>j;
            pFile.close();
            for (auto& order : j["orders"]) {
               std::string type = order["type"];
               std::string symbol = order["symbol"];
            
               if (type=="KILL"){
                   LL order_id = order["order_id"];
                   this->SendKill(symbol, order_id);
                   continue;
               }
               LL price_level = order["price_level"];
               LL qty = order["quantity"];
               
               std::string order_type = order["order_type"];
               
               if (type=="SEND"){
                   this->SendOrder(order_type, symbol, price_level, qty);
                   continue;
               }
               LL order_id = order["order_id"];
               this->SendModify(order_type, symbol, order_id, price_level, qty);
            } 
         }
         private:
         void ClientTCPListener(){
            auto self_ptr = shared_from_this();
            this->socketPtr->async_read_some(boost::asio::buffer(data, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES), [this, self_ptr](boost::system::error_code ec, size_t bytes_read) {
                if (!ec){
                    std::string msg(data.data(), bytes_read);
                    if (msg[0]=='A') {
                        CONVERSION_PACKAGE::DECODE_ACK(msg);
                    } else CONVERSION_PACKAGE::DECODE_ERROR(msg);
                }
                ClientTCPListener();
            });
         } 
         boost::asio::io_context io_context;
         std::shared_ptr<tcp::socket> socketPtr;  
         std::array<char, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES> data;
     };
};