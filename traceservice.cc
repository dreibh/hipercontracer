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


#include <boost/interprocess/streams/bufferstream.hpp> // ???????????

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
              const unsigned int              maxTTL        = 12);
   ~Traceroute();
   void run();

   inline bool isIPv6() const {
      return(SourceAddress.is_v6());
   }

   void handleTimeout();
   void handleMessage(std::size_t length);

   private:
   const unsigned int              PacketsPerHop;
   const unsigned int              MaxTTL;
   boost::asio::io_service         IO;
   boost::asio::ip::address        SourceAddress;
   boost::asio::ip::icmp::endpoint SourceEndpoint;
   boost::asio::ip::address        DestinationAddress;
   boost::asio::ip::icmp::endpoint DestinationEndpoint;
   boost::asio::ip::icmp::socket   ICMPSocket;
   boost::asio::streambuf          ReplyBuffer;
   boost::asio::deadline_timer     TimeoutTimer;
   boost::asio::ip::icmp::endpoint ReplyEndpoint;          // Store ICMP reply's source
   unsigned int                    Identifier;

   char buf[65536];
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
     ICMPSocket(IO, (isIPv6() == true) ? boost::asio::ip::icmp::v6() : boost::asio::ip::icmp::v4()),
     TimeoutTimer(IO)
{
   Identifier = 0;
}


Traceroute::~Traceroute()
{
}


void Traceroute::run()
{
   boost::system::error_code errorCode;
   unsigned short            seqNumber  = 0;

   Identifier = ::getpid();

   ICMPSocket.non_blocking(true);

   ICMPSocket.bind(SourceEndpoint, errorCode);
   if(errorCode !=  boost::system::errc::success) {
      std::cerr << "ERROR: Unable to bind to source address " << SourceAddress << "!" << std::endl;
      return;
   }

   puts("s1");
   for(unsigned int ttl = 1; ttl <= MaxTTL; ttl++) {
      const boost::asio::ip::unicast::hops option(ttl);
      ICMPSocket.set_option(option, errorCode);
      if(errorCode !=  boost::system::errc::success) {
         std::cerr << "ERROR: Unable to set TTL!" << std::endl;
         return;
      }

      // Create an ICMP header for an echo request.
      ICMPHeader echoRequest;
      echoRequest.type((isIPv6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
      echoRequest.code(0);
      echoRequest.identifier(Identifier);
      echoRequest.seqNumber(++seqNumber);
      const std::string body("----TEST!!!!----");
      computeInternet16(echoRequest, body.begin(), body.end());

      // Encode the request packet.
      boost::asio::streambuf request_buffer;
      std::ostream os(&request_buffer);
      os << echoRequest << body;

      // Send the request.
      size_t s = ICMPSocket.send_to(request_buffer.data(), DestinationEndpoint);
      if(s < 1) {  // FIXME!
         printf("s=%d\n",(int)s);
      }
   }

//    ICMPSocket.non_blocking(false);

puts("t1");
   TimeoutTimer.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(3));
   TimeoutTimer.async_wait(boost::bind(&Traceroute::handleTimeout, this));

puts("t2");
   ReplyBuffer.consume(ReplyBuffer.size());
   ICMPSocket.async_receive_from(boost::asio::buffer(buf),
//       ReplyBuffer.prepare(65536),
                                 ReplyEndpoint,
                            boost::bind(&Traceroute::handleMessage, this, _2));

puts("t3");
   IO.run();
}


void Traceroute::handleTimeout()
{
   std::cout << "Timeout" << std::endl;
}


void Traceroute::handleMessage(std::size_t length)
{
   const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
   ReplyBuffer.commit(length);

   boost::interprocess::bufferstream is(buf, length);
//    in.
printf("r=%d\n",(int)length);

   IPv4Header ipv4Header;
   IPv6Header ipv6Header;
   ICMPHeader icmpHeader;
   if(isIPv6()) {
      is >> icmpHeader >> ipv6Header;
      if(is) {

         std::cout << "A=" << ipv4Header.sourceAddress().to_string() << std::endl;
      }
   }
   else {
      is >> ipv4Header;
      if(is) {
         is >> icmpHeader;
         if(is) {
            if(ReplyEndpoint.address() != ipv4Header.sourceAddress()) {
               puts("ADDR MITMATCH!");
            }

            if(icmpHeader.type() == ICMPHeader::IPv4EchoReply) {

               if(icmpHeader.identifier() == Identifier) {
                  std::cout << "@@@@@@@ Good REPLY: from " << ReplyEndpoint.address().to_string()
                            << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;

         //          std::cout << "A=" << ipv4Header.sourceAddress().to_string() << std::endl;
               }
               else {
                  printf("wrong ID %d from %s\n", icmpHeader.identifier(),ReplyEndpoint.address().to_string().c_str());
                  std::cout << "BAD: from " << ReplyEndpoint.address().to_string()
                           << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;
                  std::cout << "src=" << ipv4Header.sourceAddress().to_string() << std::endl;
                  std::cout << "dst=" << ipv4Header.destinationAddress().to_string() << std::endl;
               }
            }
            else if(icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) {
               IPv4Header innerIPv4Header;
               ICMPHeader innerICMPHeader;
               puts("ttl");
               is >> innerIPv4Header >> innerICMPHeader;
               if(is) {
                  puts("ok-1");
                  printf(" T=%d\n",innerICMPHeader.type());
                  if(innerICMPHeader.type() == ICMPHeader::IPv4EchoRequest) {
                     puts("ok-2");
                     if(innerICMPHeader.identifier() == Identifier) {
                        puts("ok-3");
                        std::cout << "@@@@@@@ Good TTL EXCEEDED: from " << ReplyEndpoint.address().to_string()
                                 << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;
                    }
                  }
               }
            }

         } else puts("--- ICMP is bad!");
      } else puts("--- IP is bad!");

   }

   ICMPSocket.async_receive_from(boost::asio::buffer(buf), ReplyEndpoint,
                                 boost::bind(&Traceroute::handleMessage, this, _2));
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
