#pragma once
#include <iostream>
#include "enums.hpp"
#include "../server_config.h"

class CONVERSION_PACKAGE {
    public:
    static LL byte_conversion(std::string& message, int begin, int end){
       int cnt = 0;
       LL ans=0;
       for (int i = end;i>begin;i--){
            ans |= static_cast<LL>(static_cast<unsigned char>(message[i]) << (8*cnt++));
       }
       return ans;
    }
    static ORDER DECODE_SEND_ORDER(std::string message){
        ORDER order;
        order.order_type = message[1]=='B' ? ORDER_TYPE::BUY : ORDER_TYPE::SELL;
        order.request_type = REQUEST_TYPE::SEND_ORDER;
        for (int i = 2;i<=SYMBOL_BYTES;i++){
            order.symbol[i] =message[i];
        }
        order.qty=byte_conversion(message, SYMBOL_BYTES+1, SYMBOL_BYTES+QTY_BYTES+1);
        order.price_level=byte_conversion(message, SYMBOL_BYTES+QTY_BYTES+1, SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+1);

        if (CONVERSION_LOGS) std::cout << "SEND_ORDER_CONVERSION: " <<  order.qty  << " : " << order.price_level << std::endl;
        return std::move(order);
    };
    static ORDER DECODE_KILL_ORDER(std::string message){
        ORDER order;
        order.order_type = message[1]=='B' ? ORDER_TYPE::BUY : ORDER_TYPE::SELL;
        order.request_type = REQUEST_TYPE::KILL_ORDER;
        for (int i = 2;i<=SYMBOL_BYTES;i++){
            order.symbol[i] =message[i];
        }
        order.del_id=byte_conversion(message, SYMBOL_BYTES+1, SYMBOL_BYTES+ID_BYTES+1);
        if (CONVERSION_LOGS) std::cout << "KILL_ORDER_CONVERSION: " << order.del_id << std::endl;
        return std::move(order);
    };
    static ORDER DECODE_MODIFY_ORDER(std::string message){
        ORDER order;
        order.order_type = message[1]=='B' ? ORDER_TYPE::BUY : ORDER_TYPE::SELL;
        order.request_type = REQUEST_TYPE::MODIFY_ORDER;
        for (int i = 2;i<=SYMBOL_BYTES;i++){
            order.symbol[i] =message[i];
        }
        order.qty=byte_conversion(message, SYMBOL_BYTES+1, SYMBOL_BYTES+QTY_BYTES+1);
        order.price_level=byte_conversion(message, SYMBOL_BYTES+QTY_BYTES+1, SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+1);
        order.del_id=byte_conversion(message, SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+1,  SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES+1);
        if (CONVERSION_LOGS) std::cout << "MODIFY_ORDER_CONVERSION: " << order.qty  << " : " << order.price_level << " : " << order.del_id << std::endl;
        return std::move(order);
    };
};