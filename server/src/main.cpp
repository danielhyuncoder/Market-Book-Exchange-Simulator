#pragma once
#include <boost/asio.hpp>
#include "../include/server_classes.hpp"
#include <iostream>

int main() {
    

    std::unique_ptr<SERVER_PACKAGE::ServerHandler> ptr = std::make_unique<SERVER_PACKAGE::ServerHandler>();
}
