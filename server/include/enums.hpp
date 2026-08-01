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
    std::weak_ptr<SERVER_PACKAGE::MatchingSession> ptr;
};

struct alignas(64) OB_SNAPSHOT {
    ORDER buy_side[SNAPSHOT_LEN];
    ORDER sell_side[SNAPSHOT_LEN];
    LL seq_len=0;
};