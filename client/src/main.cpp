#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include "../include/client_classes.hpp"

int main() {
    std::shared_ptr<CLIENT_PACKAGE::StandardClient> client = std::make_shared<CLIENT_PACKAGE::StandardClient>();
    std::string order = "BUY";
    std::string symbol = "APPL";
    LD price_level = 0.2;
    LD qty = 203;
    client->SendOrder(order, symbol, price_level, qty);
    client->start();
}
