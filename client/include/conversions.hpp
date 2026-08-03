#pragma once
#include <iostream>
#include <string>
#include <array>
#include "../client_config.h"


class CONVERSION_PACKAGE {
    public:
    template<typename N>
    static std::string number_to_bytes(N number, const int num_bytes){
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
        return msg;
    };
    static void pad_message(std::string& str){
        while (str.length()<2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES){
            str+=' ';
        }
    }
    static LL byte_conversion(std::string& message, int begin, int end){
       int cnt = 0;
       LL ans=0;
       for (int i = end;i>=begin;i--){
            ans |= static_cast<LL>(static_cast<unsigned char>(message[i]) << (8*cnt++));
       }
       return ans;
    }
    static int RESPONSE_BODY_LENGTH(char type){
        switch(type){
            case 'A': return 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+2*ID_BYTES;
            case 'F': return 1+SYMBOL_BYTES+ID_BYTES+QTY_BYTES+PRICE_BYTES;
            case 'P': return 1+SYMBOL_BYTES+ID_BYTES+2*QTY_BYTES+PRICE_BYTES;
            case 'K': return SYMBOL_BYTES+ID_BYTES+QTY_BYTES;
            case 'J': return 4;
            default:  return 0;
        }
    }
    static void DECODE_ACK(std::string& body){
        int off = 2+SYMBOL_BYTES;
        LL qty = byte_conversion(body, off, off+QTY_BYTES-1); off+=QTY_BYTES;
        LL price = byte_conversion(body, off, off+PRICE_BYTES-1); off+=PRICE_BYTES;
        LL order_id = byte_conversion(body, off, off+ID_BYTES-1); off+=ID_BYTES;
        LL del_id = byte_conversion(body, off, off+ID_BYTES-1);
        std::cout << "ACK -> TYPE: " << body[0] << " SIDE: " << body[1]
               << " SYMBOL: " << body.substr(2, SYMBOL_BYTES)
               << " QTY: " << qty << " PRICE: " << price
               << " ORDER_ID: " << order_id << " DEL_ID: " << del_id << "\n\n";
    }
    static void DECODE_FULL_FILL(std::string& body){
        int off = 1+SYMBOL_BYTES;
        LL order_id = byte_conversion(body, off, off+ID_BYTES-1); off+=ID_BYTES;
        LL fill_qty = byte_conversion(body, off, off+QTY_BYTES-1); off+=QTY_BYTES;
        LL fill_price = byte_conversion(body, off, off+PRICE_BYTES-1);
        std::cout << "FULL FILL -> SIDE: " << body[0] << " SYMBOL: " << body.substr(1, SYMBOL_BYTES)
               << " ORDER_ID: " << order_id << " QTY: " << fill_qty << " PRICE: " << fill_price << "\n\n";
    }
    static void DECODE_PARTIAL_FILL(std::string& body){
        int off = 1+SYMBOL_BYTES;
        LL order_id = byte_conversion(body, off, off+ID_BYTES-1); off+=ID_BYTES;
        LL fill_qty = byte_conversion(body, off, off+QTY_BYTES-1); off+=QTY_BYTES;
        LL remaining = byte_conversion(body, off, off+QTY_BYTES-1); off+=QTY_BYTES;
        LL fill_price = byte_conversion(body, off, off+PRICE_BYTES-1);
        std::cout << "PARTIAL FILL -> SIDE: " << body[0] << " SYMBOL: " << body.substr(1, SYMBOL_BYTES)
               << " ORDER_ID: " << order_id << " FILLED: " << fill_qty
               << " REMAINING: " << remaining << " PRICE: " << fill_price << "\n\n";
    }
    static void DECODE_KILL_CONFIRM(std::string& body){
        int off = SYMBOL_BYTES;
        LL order_id = byte_conversion(body, off, off+ID_BYTES-1); off+=ID_BYTES;
        LL removed_qty = byte_conversion(body, off, off+QTY_BYTES-1);
        std::cout << "KILL CONFIRMED -> SYMBOL: " << body.substr(0, SYMBOL_BYTES)
               << " ORDER_ID: " << order_id << " QTY_REMOVED: " << removed_qty << "\n\n";
    }
    static void DECODE_ERROR(std::string& body){
        std::cout << "ERROR FROM SERVER -> CODE: " << byte_conversion(body, 0, 3) << "\n\n";
    }
};