#define LL long long
#include <iostream>
#include "../subscriber_config.h"
struct CLIENT_ORDER {
     LL price_level = 0, qty = 0, order_id = 0;
};
struct CLIENT_SNAPSHOT {
    std::string symbol;
    CLIENT_ORDER buy_side[SNAPSHOT_LEN];
    CLIENT_ORDER sell_side[SNAPSHOT_LEN];
    LL seq_len=0;
};