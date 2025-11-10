#include "server_classes.hpp"
#include "order_book.hpp"
#include "json.hpp"
using json = nlohmann::json;
void SERVER_PACKAGE::MatchingSession::readListener(ORDER_BOOK_PACKAGE::MARKET_BOOK& market_book) {
  
        auto self = shared_from_this(); 
        socket_.async_read_some(
            boost::asio::buffer(data_, 2+SYMBOL_BYTES+QTY_BYTES+PRICE_BYTES+ID_BYTES), 
            [this, self, &market_book](boost::system::error_code ec, size_t bytes_read) {
                if (!ec) {
                    std::string msg(data_.data(), bytes_read);
                    ORDER current_order;
                    current_order.ptr=shared_from_this().get();
                    if (msg[0]=='O'){
                      current_order=CONVERSION_PACKAGE::DECODE_SEND_ORDER(msg);
                      bool s = simpleReject(current_order);
                      if (s) {readListener(market_book);return;}
                      this->writeToClient("A"+msg.substr(1, (int)msg.length()-1));
                    } else if (msg[0]=='U'){
                      current_order=CONVERSION_PACKAGE::DECODE_MODIFY_ORDER(msg);
                      bool s = simpleReject(current_order);
                      if (s) {readListener(market_book);return;}
                    } else if (msg[0]=='X'){
                      current_order=CONVERSION_PACKAGE::DECODE_KILL_ORDER(msg);
                    } else {
                      this->writeToClient("J"+CONVERSION_PACKAGE::number_to_bytes<LL>(MALFORMED_REQUEST, 4));readListener(market_book); return;
                    }
                    current_order.order_id=market_book.assign_order_id();
                    market_book.market_orders->push(std::move(current_order));
  
                }
                readListener(market_book);
            });
}
SERVER_PACKAGE::ServerHandler::ServerHandler(){
    std::ifstream pFile(SYMBOL_PATH);
    if (!pFile.is_open()) {
        std::cerr << "Failed to open symbols.json" << std::endl;
        return;
    }
    json j;
    pFile>>j;
    pFile.close();
    std::vector<std::string> symbols;
    for (auto& symbol : j["symbols"]) {
        symbols.push_back(symbol["symbol_name"]);
    }

    ORDER_BOOK_PACKAGE::MARKET_BOOK market_book(this->io_context, std::move(symbols));
    MatchingEngine engine(this->io_context, market_book);
    
    for (int i =0;i<NUM_SERVER_THREADS;i++){
        this->threads.emplace_back([this]{

            io_context.run();
            
        });
    }
    for (int i =0;i<NUM_MARKET_BOOK_THREADS;i++){
        this->threads.emplace_back([this, &market_book]{
             market_book.market_listener();
        });
    }
    
    for (auto& thread : this->threads) {
        thread.join();
    }
    
}