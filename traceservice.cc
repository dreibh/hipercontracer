// NorNet Trace Service
// Copyright (C) 2015 by Thomas Dreibholz
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact: dreibh@simula.no


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <set>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"


std::set<boost::asio::ip::address> sourceArray;
std::set<boost::asio::ip::address> destinationArray;


void addAddress(std::set<boost::asio::ip::address>& array,
                const std::string&                  addressString)
{
   boost::system::error_code errorCode;
   boost::asio::ip::address address = boost::asio::ip::address::from_string(addressString, errorCode);
   if(errorCode != boost::system::errc::success) {
      std::cerr << "ERROR: Bad address " << addressString << "!" << std::endl;
      ::exit(1);
   }
   array.insert(address);
}


class Traceroute : public boost::noncopyable
{
   public:
   Traceroute(const boost::asio::ip::address& source,
              const boost::asio::ip::address& destination,
              const unsigned int              packetsPerHop = 3,
              const unsigned int              maxTTL        = 35);
   ~Traceroute();
   void run();

   inline bool isIPv6() const {
      return(SourceAddress.is_v6());
   }

   private:
   const unsigned int              PacketsPerHop;
   const unsigned int              MaxTTL;
   boost::asio::io_service         IO;
   boost::asio::ip::address        SourceAddress;
   boost::asio::ip::icmp::endpoint SourceEndpoint;
   boost::asio::ip::address        DestinationAddress;
   boost::asio::ip::icmp::endpoint DestinationEndpoint;
   boost::asio::ip::icmp::socket   ICMPSocket;
};


Traceroute::Traceroute(const boost::asio::ip::address& source,
                       const boost::asio::ip::address& destination,
                       const unsigned int              packetsPerHop,
                       const unsigned int              maxTTL)
   : PacketsPerHop(packetsPerHop),
     MaxTTL(maxTTL),
     IO(),
     SourceAddress(source),
     SourceEndpoint(SourceAddress, 0),
     DestinationAddress(destination),
     DestinationEndpoint(DestinationAddress, 0),
     ICMPSocket(IO, (isIPv6() == true) ? boost::asio::ip::icmp::v6() : boost::asio::ip::icmp::v4())
{
}


Traceroute::~Traceroute()
{
}


void Traceroute::run()
{
   unsigned int   identifier = ::getpid();
   unsigned short seqNumber  = 0;

   ICMPSocket.non_blocking(true);

   puts("s1");
   for(unsigned int ttl = 1; ttl <= MaxTTL; ttl++) {
      const boost::asio::ip::unicast::hops option(ttl);
      ICMPSocket.set_option(option);

      // Create an ICMP header for an echo request.
      ICMPHeader echoRequest;
      echoRequest.type((isIPv6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
      echoRequest.code(0);
      echoRequest.identifier(identifier);
      echoRequest.seqNumber(++seqNumber);
      const std::string body("");
      computeInternet16(echoRequest, body.begin(), body.end());

      // Encode the request packet.
      boost::asio::streambuf request_buffer;
      std::ostream os(&request_buffer);
      os << echoRequest << body;

      // Send the request.
      size_t s = ICMPSocket.send_to(request_buffer.data(), DestinationEndpoint);
//       printf("s=%d\n",(int)s);
   }

   ICMPSocket.non_blocking(false);

   for(unsigned int ttl = 1; ttl <= MaxTTL; ttl++) {

             // Recieve some data and parse it.
            std::vector<boost::uint8_t> data(1500,0);
            const std::size_t nr(
               ICMPSocket.receive(
                  boost::asio::buffer(data) ) );
            printf("nr=%d\n",(int)nr);
            if( nr < 16 )
            {
               throw std::runtime_error("To few bytes returned.");
            }
            boost::asio::ip::address address;
            if(isIPv6()) {
               boost::asio::ip::address_v6::bytes_type v6address;
               for(size_t i = 0;i < 16; i++) {
                  v6address[i] = data[16 + i];
               }
               address = boost::asio::ip::address_v6(v6address, 0);
            }
            else {
               boost::asio::ip::address_v4::bytes_type v4address;
               for(size_t i = 0;i < 4; i++) {
                  v4address[i] = data[12 + i];
               }
               address = boost::asio::ip::address_v4(v4address);
            }
            std::cout << address.to_string() << '\n';
            if( address == DestinationAddress )
            {
               break;
            }
   }
}


int main(int argc, char** argv)
{
   for(int i = 1; i < argc; i++) {
      if(strncmp(argv[i], "-source=", 8) == 0) {
         addAddress(sourceArray, (const char*)&argv[i][8]);
      }
      else if(strncmp(argv[i], "-destination=", 13) == 0) {
         addAddress(destinationArray, (const char*)&argv[i][13]);
      }
      else {
         std::cerr << "Usage: " << argv[0] << " -source=source ... -destination=destination ..." << std::endl;
      }
   }
   if( (sourceArray.size() < 1) ||
       (destinationArray.size() < 1) ) {
      std::cerr << "ERROR: At least one source and destination are needed!" << std::endl;
      ::exit(1);
   }

   for(std::set<boost::asio::ip::address>::iterator sourceIterator = sourceArray.begin(); sourceIterator != sourceArray.end(); sourceIterator++) {
      for(std::set<boost::asio::ip::address>::iterator destinationIterator = destinationArray.begin(); destinationIterator != destinationArray.end(); destinationIterator++) {
         Traceroute t(*sourceIterator, *destinationIterator);
         t.run();
      }
   }
}
