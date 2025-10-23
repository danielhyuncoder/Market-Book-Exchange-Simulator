#pragma once
#include <iostream>
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
};