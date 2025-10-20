#pragma once
#include <iostream>

#define LL long long
#define LD long double

enum class ORDER_TYPE{ 
    SELL=0,BUY=1
};

enum class REQUEST_TYPE { 
    SEND_ORDER=0,
    MODIFY_ORDER=1,
    KILL_ORDER=2,
};

struct ORDER alignas(64) {
    LL price_level =0, qty =0;
    std::string symbol ="";
    ORDER_TYPE order_type;
    REQUEST_TYPE request_type;
    LL order_id;
};
