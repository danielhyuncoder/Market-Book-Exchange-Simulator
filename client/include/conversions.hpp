#pragma once
#include <iostream>
#include <string>
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
    static void DECODE_ERROR(std::string& message){
       std::cout << "ERROR FROM SERVER -> CODE: ";
       std::cout << byte_conversion(message, 1, 4) << std::endl;
    }
    static void DECODE_ACK(std::string& message){
       std::cout << "ACK FROM SERVER -> " << std::endl;
       std::string s = "";s+=message[1];
       std::cout << ("TYPE: " + s) << std::endl;
       std::cout << ("SYMBOL: " + message.substr(2, 4)) << std::endl;
       std::cout << ("QUANTITY: " + std::to_string(byte_conversion(message, SYMBOL_BYTES+3, SYMBOL_BYTES+QTY_BYTES+1))) << std::endl;
       std::cout << ("PRICE LEVEL: " + std::to_string(byte_conversion(message, SYMBOL_BYTES+QTY_BYTES+3, SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+1))) << std::endl;
       std::cout << std::endl;
    }
};