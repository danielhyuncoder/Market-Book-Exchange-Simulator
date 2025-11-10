#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

namespace SUBSCRIBER_PACKAGE {
    class Subscriber {
         public:
         Subscriber() : multicast_ep();

         private:
         boost::asio::io_context io_context;
         udp::endpoint multicast_ep;
    };
};