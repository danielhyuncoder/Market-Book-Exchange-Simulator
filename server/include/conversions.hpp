#pragma once
#include <iostream>
#include "enums.hpp"
#include "../server_config.h"

class CONVERSION_PACKAGE {
    public:
    static ORDER DECODE_SEND_ORDER(std::string message){
        ORDER order;
        order.order_type = message[1]=='B' ? ORDER_TYPE::BUY : ORDER_TYPE::SELL;
        order.request_type = REQUEST_TYPE::SEND_ORDER;
        for (int i = 2;i<=SYMBOL_BYTES;i++){
            order.symbol +=message[i];
        }
        int cnt = 1;
        for (int i = SYMBOL_BYTES+QTY_BYTES+1;i>SYMBOL_BYTES+1;i--){
            order.qty |= static_cast<LL>(static_cast<unsigned char>(message[i]) << (8*cnt++));
        }
        cnt=1;
         for (int i = SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+1;i>SYMBOL_BYTES+QTY_BYTES+1;i--){
            order.price_level |= static_cast<LL>(static_cast<unsigned char>(message[i]) << (8*cnt++));
        }
        return std::move(order);
    };
};