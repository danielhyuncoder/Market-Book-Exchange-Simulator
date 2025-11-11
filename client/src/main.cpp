#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include "../include/client_classes.hpp"
int main() {

    std::shared_ptr<CLIENT_PACKAGE::StandardClient> client = std::make_shared<CLIENT_PACKAGE::StandardClient>();
    std::string order = "B";
    std::string symbol = "APPL";
    LL price_level = 54;
    LL qty = 243;
    client->SendOrder(order, symbol, price_level, qty);
    //client->SendModify(order, symbol, 23, price_level, qty);
    client->start(); 
}
