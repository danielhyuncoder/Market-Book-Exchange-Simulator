#pragma once
#include <iostream>
#include "../subscriber_config.h"
#include "enums.hpp"
#include <vector>

class CONVERSION_PACKAGE {
    public:
    // Decodes big-endian bytes back into a 64-bit number
template<typename N>
static LL byte_conversion(const std::string& message, int begin, int end) {
    LL ans = 0;
    for (int i = begin; i < end; ++i) {
        ans = (ans << 8) | static_cast<unsigned char>(message[i]);
    }
    return ans;
}

// Main decoder
static CLIENT_SNAPSHOT get_snapshot(const std::string& message) {
    CLIENT_SNAPSHOT client_snapshot;

    // --- 1️⃣ Symbol (first 4 bytes)
    client_snapshot.symbol = message.substr(0, SYMBOL_BYTES);

    // --- 2️⃣ Sequence number (next 8 bytes)
    int offset = SYMBOL_BYTES;
    client_snapshot.seq_len = byte_conversion<LL>(message, offset, offset + 8);
    offset += 8;

    if (PRINT_SNAPSHOT)
        std::cout << client_snapshot.symbol << std::endl
                  << "SEQ_LEN: " << client_snapshot.seq_len << std::endl
                  << "ASKS:" << std::endl;

    // Each order: ID, Price, Qty
    constexpr int ORDER_BYTES = ID_BYTES + PRICE_BYTES + QTY_BYTES;

    // --- 3️⃣ SELL side (first SNAPSHOT_LEN orders)
    for (int i = 0; i < SNAPSHOT_LEN; i++) {
        auto& o = client_snapshot.sell_side[i];
        o.order_id = byte_conversion<LL>(message, offset, offset + ID_BYTES);
        offset += ID_BYTES;

        o.price_level = byte_conversion<LL>(message, offset, offset + PRICE_BYTES);
        offset += PRICE_BYTES;

        o.qty = byte_conversion<LL>(message, offset, offset + QTY_BYTES);
        offset += QTY_BYTES;

        if (PRINT_SNAPSHOT)
            std::cout << o.order_id << ", " << o.price_level << ", " << o.qty << std::endl;
    }

    if (PRINT_SNAPSHOT) std::cout << "BIDS:" << std::endl;

    // --- 4️⃣ BUY side (next SNAPSHOT_LEN orders)
    for (int i = 0; i < SNAPSHOT_LEN; i++) {
        auto& o = client_snapshot.buy_side[i];
        o.order_id = byte_conversion<LL>(message, offset, offset + ID_BYTES);
        offset += ID_BYTES;

        o.price_level = byte_conversion<LL>(message, offset, offset + PRICE_BYTES);
        offset += PRICE_BYTES;

        o.qty = byte_conversion<LL>(message, offset, offset + QTY_BYTES);
        offset += QTY_BYTES;

        if (PRINT_SNAPSHOT)
            std::cout << o.order_id << ", " << o.price_level << ", " << o.qty << std::endl;
    }

    return client_snapshot;
}


};