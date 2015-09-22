// Example source:
// http://web.archive.org/web/20150922093308/https://svn.boost.org/trac/boost/attachment/ticket/4529/program.cpp

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <boost/asio.hpp>
#include "icmp_header.h"  // from boost::asio examples on boost website
                          // (also pasted last in this file)

using boost::asio::ip::icmp;

class traceroute: public boost::noncopyable
{
   public:
      traceroute(const char* host):
         io(),
         resolver(io),
         socket(io, icmp::v4()),
         sequence_number(0)
      {
        icmp::resolver::query query(icmp::v4(), host, "");
        destination = *resolver.resolve(query);
      }
      ~traceroute() { socket.close(); }
      void trace()
      {
         for( int ttl(1); ttl < 31 ; ttl++)
         {
            const boost::asio::ip::unicast::hops option( ttl );
            socket.set_option(option);

            boost::asio::ip::unicast::hops op;
            socket.get_option(op);
            if( ttl !=  op.value() )
            {
               std::ostringstream o;
               o << "TTL not set properly. Should be "
                 << ttl << " but was set to "
                 << op.value() << '.';
               throw std::runtime_error(o.str());
            }

            // Create an ICMP header for an echo request.
            icmp_header echo_request;
            echo_request.type(icmp_header::echo_request);
            echo_request.code(0);
            echo_request.identifier(get_identifier());
            echo_request.sequence_number(++sequence_number);
            const std::string body("");
            compute_checksum(echo_request, body.begin(), body.end());

            // Encode the request packet.
            boost::asio::streambuf request_buffer;
            std::ostream os(&request_buffer);
            os << echo_request << body;

            // Send the request.
            socket.send_to(request_buffer.data(), destination);

            // Recieve some data and parse it.
            std::vector<boost::uint8_t> data(64,0);
            const std::size_t nr(
               socket.receive(
                  boost::asio::buffer(data) ) );
            if( nr < 16 )
            {
               throw std::runtime_error("To few bytes returned.");
            }
            std::ostringstream remote_ip;
            remote_ip
               << (int)data[12] << '.'
               << (int)data[13] << '.'
               << (int)data[14] << '.'
               << (int)data[15];
            std::cout << remote_ip.str() << '\n';
            if( boost::asio::ip::address_v4::from_string( remote_ip.str() )
                  == destination.address() )
            {
               break;
            }
         }
      }
   private:
      static const int port = 33434;
      boost::asio::io_service io;
      boost::asio::ip::icmp::resolver resolver;
      boost::asio::ip::icmp::socket socket ;
      unsigned short sequence_number;
      boost::asio::ip::icmp::endpoint destination;
      static unsigned short get_identifier()
      {
#if defined(BOOST_WINDOWS)
         return static_cast<unsigned short>(::GetCurrentProcessId());
#else
         return static_cast<unsigned short>(::getpid());
#endif
      }
};

int main( int argc, char** argv)
{
   try
   {
      if( argc != 2 )
      {
         throw std::invalid_argument("Usage: traceroute host");
      }
      traceroute T( argv[1] );
      T.trace();
   }
   catch( const std::exception& e )
   {
      std::cerr << e.what() << '\n';
   }
}
