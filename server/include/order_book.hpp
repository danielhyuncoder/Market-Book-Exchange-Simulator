#pragma once
#include <iostream>
#include <memory>
#include "enums.hpp"
#include <boost/lockfree/queue.hpp>
#include <unordered_map>

namespace ORDER_BOOK_PACKAGE {
    class MARKET_BOOK {
        public:
        boost::lockfree::queue<ORDER> market_orders;
        void market_listener(){
            while (true) {
                ORDER order;
                if (!market_orders.pop(order)) continue;
                
                std::cout << "PROCESSED ORDER " std::endl;
            }
        }
        private:
        std::unordered_map<std::string, ORDER_BOOK> orderbooks; 
    };
    class ORDER_BOOK {

    };
};