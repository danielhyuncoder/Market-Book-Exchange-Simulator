#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include "../include/conversions.hpp"
using boost::asio::ip::udp;

int main() {
    try {
        boost::asio::io_context io_context;

        // Multicast settings
        const short multicast_port = 30001;
        const std::string multicast_address = "239.192.37.42"; // Example multicast address

        // Create the socket and bind to the multicast port
        udp::socket socket(io_context);
        udp::endpoint listen_endpoint(udp::v4(), multicast_port);
        socket.open(listen_endpoint.protocol());
        socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        socket.bind(listen_endpoint);

        // Join the multicast group
        boost::asio::ip::address multicast_addr = boost::asio::ip::make_address(multicast_address);
        socket.set_option(boost::asio::ip::multicast::join_group(multicast_addr));

        std::cout << "Listening on multicast " << multicast_address << ":" << multicast_port << std::endl;

        std::array<char, 1024> recv_buffer;

        while (true) {
            udp::endpoint sender_endpoint;
            size_t len = socket.receive_from(boost::asio::buffer(recv_buffer), sender_endpoint);
            std::string message(recv_buffer.data(), len);

            std::cout << "Received from " << sender_endpoint.address().to_string() 
                      << ":" << sender_endpoint.port() << " - " << message << std::endl;
            CONVERSION_PACKAGE::get_snapshot(message);
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}