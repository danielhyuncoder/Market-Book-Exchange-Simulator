#pragma once
#include <iostream>
#include "../client_config.h"


class CONVERSION_PACKAGE {
    public:
    static std::string number_to_bytes(LL number, const int num_bytes){
        std::string msg="";
        while (number>0){
            LL record = number & 255;
            msg+=static_cast<unsigned char>(record);
            number = number >> 8;
        }
        while (msg.length()<num_bytes) {
            msg += static_cast<unsigned char>(0);
        }
        std::reverse(msg.begin(), msg.end());
        return msg;
    };
};