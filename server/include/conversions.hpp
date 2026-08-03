#pragma once
#include <iostream>
#include "enums.hpp"
#include "../server_config.h"



class CONVERSION_PACKAGE {
    public:
    static LL byte_conversion(std::string& message, int begin, int end){
       int cnt = 0;
       LL ans=0;
       for (int i = end;i>=begin;i--){
            ans |= static_cast<LL>(static_cast<unsigned char>(message[i]) << (8*cnt++));
       }
       return ans;
    }
    static ORDER DECODE_SEND_ORDER(std::string message){
        ORDER order;
        order.order_type = message[1]=='B' ? ORDER_TYPE::BUY : ORDER_TYPE::SELL;
        order.request_type = REQUEST_TYPE::SEND_ORDER;
        for (int i = 2;i<SYMBOL_BYTES+2;i++){
            order.symbol[i-2] =message[i];
        }
        order.qty=byte_conversion(message, SYMBOL_BYTES+2, SYMBOL_BYTES+QTY_BYTES+1);
        order.price_level=byte_conversion(message, SYMBOL_BYTES+QTY_BYTES+2, SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+1);

        if (CONVERSION_LOGS) std::cout << "SEND_ORDER_CONVERSION: " <<  order.qty  << " : " << order.price_level << std::endl;
        return std::move(order);
    };
    static ORDER DECODE_KILL_ORDER(std::string message){
        ORDER order;
        order.order_type = message[1]=='B' ? ORDER_TYPE::BUY : ORDER_TYPE::SELL;
        order.request_type = REQUEST_TYPE::KILL_ORDER;
        for (int i = 2;i<SYMBOL_BYTES+2;i++){
            order.symbol[i-2] =message[i];
        }
        order.del_id=byte_conversion(message, SYMBOL_BYTES+2, SYMBOL_BYTES+ID_BYTES+1);
        if (CONVERSION_LOGS) std::cout << "KILL_ORDER_CONVERSION: " << order.del_id << std::endl;
        return std::move(order);
    };
    static ORDER DECODE_MODIFY_ORDER(std::string message){
        ORDER order;
        order.order_type = message[1]=='B' ? ORDER_TYPE::BUY : ORDER_TYPE::SELL;
        order.request_type = REQUEST_TYPE::MODIFY_ORDER;
        for (int i = 2;i<SYMBOL_BYTES+2;i++){
            order.symbol[i-2] =message[i];
        }
        order.qty=byte_conversion(message, SYMBOL_BYTES+2, SYMBOL_BYTES+QTY_BYTES+1);
        order.price_level=byte_conversion(message, SYMBOL_BYTES+QTY_BYTES+2, SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+1);
        order.del_id=byte_conversion(message, SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+2,  SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES+1);
        if (CONVERSION_LOGS) std::cout << "MODIFY_ORDER_CONVERSION: " << order.qty  << " : " << order.price_level << " : " << order.del_id << std::endl;
        return std::move(order);
    };
    template<typename N>
    static constexpr std::string number_to_bytes(N number, const int num_bytes){
        std::string msg="";
        while (number>0){
            N record = number & 255;
            msg+=static_cast<unsigned char>(record);
            number = number >> 8;
        }
        while (msg.length()<num_bytes) {
            msg += static_cast<unsigned char>(0);
        }
        std::reverse(msg.begin(), msg.end());
        return std::move(msg);
    };

    template<typename N>
    static std::string order_to_bytes(ORDER& order){
        std::string msg="";
        msg+=number_to_bytes<N>(order.order_id, ID_BYTES);
        msg+=number_to_bytes<N>(order.price_level, PRICE_BYTES);
        msg+=number_to_bytes<N>(order.qty, QTY_BYTES);
        return std::move(msg);
    };
    static std::string SNAPSHOT_TO_BYTES(OB_SNAPSHOT snapshot, std::string symbol, LL seq_len){
        // Overall: Symbol (4 bytes), Seq_len (8 bytes)
        // Each Order (BUY): ID, Price, quantity (24 * 10) total = 240 bytes
        // Each Order (SELL): ID, Price, quantity (24 * 10) total = 240 bytes
        symbol += number_to_bytes<LL>(seq_len, 8);
        for (int i =0;i<SNAPSHOT_LEN;i++){
            symbol+=order_to_bytes<LL>(snapshot.buy_side[i]);
        }
        for (int i =0;i<SNAPSHOT_LEN;i++){
            symbol+=order_to_bytes<LL>(snapshot.sell_side[i]);
        }
        return std::move(symbol);
    }

};