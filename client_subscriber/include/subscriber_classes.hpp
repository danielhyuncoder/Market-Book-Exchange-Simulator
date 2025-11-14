#include <iostream>
#include <boost/asio.hpp>
#include "../subscriber_config.h"
using boost::asio::ip::udp;

namespace SUBSCRIBER_PACKAGE {
    class Subscriber {
         public:
         Subscriber() : socket(io_context), multicast_ep(udp::v4(), MULTICAST_PORT) {
            socket.open(multicast_ep.protocol());
            socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
            socket.bind(multicast_ep);

            boost::asio::ip::address multicast_addr = boost::asio::ip::make_address(MULTICAST_IP);
            socket.set_option(boost::asio::ip::multicast::join_group(multicast_addr));
            this->snapshotListener();
         };
         

         private:
         boost::asio::io_context io_context;
         udp::socket socket;
         udp::endpoint multicast_ep;
         std::array<char, 4 + 8 + (2*(24 * SNAPSHOT_LEN))> recv_buffer;
         void snapshotListener(){
            while (true) {
                udp::endpoint sender_endpoint;
                size_t len = socket.receive_from(boost::asio::buffer(recv_buffer), sender_endpoint);
                std::string message(recv_buffer.data(), len);

                std::cout << "Received snapshot from " << sender_endpoint.address().to_string() 
                      << ":" << sender_endpoint.port() << std::endl;

                CONVERSION_PACKAGE::get_snapshot(message);
                
            }
         }
    };
};