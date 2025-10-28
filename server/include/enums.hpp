#pragma once
#include <iostream>
#include <memory>
#include "../server_config.h"
#define LL long long
#define LD long double

namespace SERVER_PACKAGE {
    class MatchingSession;
};

enum class ORDER_TYPE{ 
    SELL=0,BUY=1
};

enum class REQUEST_TYPE { 
    SEND_ORDER=0,
    MODIFY_ORDER=1,
    KILL_ORDER=2,
};

struct alignas(64) ORDER {
    LL price_level =0, qty =0;
    char symbol[SYMBOL_BYTES];
    ORDER_TYPE order_type;
    REQUEST_TYPE request_type;
    LL order_id=0;
    LL del_id=0;
    SERVER_PACKAGE::MatchingSession* ptr;
};
