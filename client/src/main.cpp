#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include "../include/client_classes.hpp"
int main() {

    std::shared_ptr<CLIENT_PACKAGE::StandardClient> client = std::make_shared<CLIENT_PACKAGE::StandardClient>();
    client->ProcessOrdersFromJSON();
    client->start(); 
}
