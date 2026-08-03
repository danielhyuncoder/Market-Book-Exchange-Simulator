#pragma once
#include <iostream>
#include "../subscriber_config.h"
#include "enums.hpp"
#include <vector>

class CONVERSION_PACKAGE {
    public:
 
    template<typename N>
    static LL byte_conversion(const std::string& message, int begin, int end) {
       LL ans = 0;
       for (int i = begin; i < end; ++i) {
           ans = (ans << 8) | static_cast<unsigned char>(message[i]);
       }
       return ans;
    }

    static CLIENT_SNAPSHOT get_snapshot(const std::string& message) {
       CLIENT_SNAPSHOT client_snapshot;

       client_snapshot.symbol = message.substr(0, SYMBOL_BYTES);

       int offset = SYMBOL_BYTES;
       client_snapshot.seq_len = byte_conversion<LL>(message, offset, offset + 8);
       offset += 8;

       if (PRINT_SNAPSHOT)
        std::cout << client_snapshot.symbol << std::endl
                  << "SEQ_LEN: " << client_snapshot.seq_len << std::endl
                  << "BIDS:" << std::endl;

       constexpr int ORDER_BYTES = ID_BYTES + PRICE_BYTES + QTY_BYTES;

       for (int i = 0; i < SNAPSHOT_LEN; i++) {
           CLIENT_ORDER& order = client_snapshot.buy_side[i];
           order.order_id = byte_conversion<LL>(message, offset, offset + ID_BYTES);
           offset += ID_BYTES;

           order.price_level = byte_conversion<LL>(message, offset, offset + PRICE_BYTES);
           offset += PRICE_BYTES;

           order.qty = byte_conversion<LL>(message, offset, offset + QTY_BYTES);
           offset += QTY_BYTES;

           if (PRINT_SNAPSHOT) std::cout << order.order_id << ", " << order.price_level << ", " << order.qty << std::endl;
       }

       if (PRINT_SNAPSHOT) std::cout << "ASKS:" << std::endl;

   
       for (int i = 0; i < SNAPSHOT_LEN; i++) {
          CLIENT_ORDER& order = client_snapshot.sell_side[i];
          order.order_id = byte_conversion<LL>(message, offset, offset + ID_BYTES);
          offset += ID_BYTES;

          order.price_level = byte_conversion<LL>(message, offset, offset + PRICE_BYTES);
          offset += PRICE_BYTES;

          order.qty = byte_conversion<LL>(message, offset, offset + QTY_BYTES);
          offset += QTY_BYTES;

          if (PRINT_SNAPSHOT) std::cout << order.order_id << ", " << order.price_level << ", " << order.qty << std::endl;
       }
       if (PRINT_SNAPSHOT) std::cout << std::endl;
       return client_snapshot;
}


};