// market book debugger
#include <iostream>
#include "order_book.hpp"

int main() {
    std::cout << "start\n" << std::flush;

    boost::asio::io_context ctx;
    std::cout << "io_context created\n" << std::flush;

    std::vector<std::string> symbols = {"S000", "S001"};
    std::cout << "about to construct MARKET_BOOK\n" << std::flush;

    ORDER_BOOK_PACKAGE::MARKET_BOOK mb(ctx, symbols);
    std::cout << "MARKET_BOOK constructed OK\n" << std::flush;

    return 0;
}