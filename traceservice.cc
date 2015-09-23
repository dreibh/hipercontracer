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

#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>

#include <map>
#include <vector>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

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


enum HopStatus {
   Unknown                = 0,
   TimeExceeded           = 1,
   UnreachableScope       = 100,
   UnreachableNetwork     = 101,
   UnreachableHost        = 102,
   UnreachableProtocol    = 103,
   UnreachablePort        = 104,
   UnreachableProhibited  = 105,
   UnreachableUnknown     = 110,
   Timeout                = 200,
   Success                = 255
};

class HopResult {
   public:
   HopResult(const unsigned int             hop,
             boost::asio::ip::address       address,
             const HopStatus                status,
             const boost::posix_time::ptime sendTime);

   inline friend bool operator<(const HopResult& hopResult1, const HopResult& hopResult2) {
      return(hopResult1.Hop < hopResult2.Hop);
   }
   friend std::ostream& operator<<(std::ostream& os, const HopResult& hopResult);

   private:
   const unsigned int             Hop;
   boost::asio::ip::address       Address;
   const HopStatus                Status;
   const boost::posix_time::ptime SendTime;
};


HopResult::HopResult(const unsigned int             hop,
                     boost::asio::ip::address       address,
                     const HopStatus                status,
                     const boost::posix_time::ptime sendTime)
   : Hop(hop),
     Address(address),
     Status(status),
     SendTime(sendTime)
{
}

std::ostream& operator<<(std::ostream& os, const HopResult& hopResult)
{
   os << hopResult.Hop
      << "\t" << hopResult.Address
      << "\t" << hopResult.Status;
//       << "\t" << hopResult.RTT.total_microseconds();
   return(os);
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
   void recordResult(const boost::posix_time::ptime& receiveTime,
                     const ICMPHeader&               icmpHeader);

   const unsigned int                PacketsPerHop;
   const unsigned int                MaxTTL;
   boost::asio::io_service           IO;
   boost::asio::ip::address          SourceAddress;
   boost::asio::ip::icmp::endpoint   SourceEndpoint;
   boost::asio::ip::address          DestinationAddress;
   boost::asio::ip::icmp::endpoint   DestinationEndpoint;
   boost::asio::ip::icmp::socket     ICMPSocket;
   boost::asio::streambuf            ReplyBuffer;
   boost::asio::deadline_timer       TimeoutTimer;
   boost::asio::ip::icmp::endpoint   ReplyEndpoint;          // Store ICMP reply's source

   unsigned int                      Identifier;
   int                               LastHop;
   std::map<unsigned int, HopResult> HopResultsMap;

   char buf[65536];  // FIXME! ?????
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
   LastHop    = -1;
}


Traceroute::~Traceroute()
{
}


void Traceroute::run()
{
   boost::system::error_code errorCode;
   unsigned short            seqNumber  = 0;

   Identifier = ::getpid();

   // ====== Bind ICMP socket to given source address =======================
   ICMPSocket.bind(SourceEndpoint, errorCode);
   if(errorCode !=  boost::system::errc::success) {
      std::cerr << "ERROR: Unable to bind to source address "
                << SourceAddress << "!" << std::endl;
      return;
   }

   // ====== Set filter (not required, but much more efficient) =============
   if(isIPv6()) {
      struct icmp6_filter filter;
      ICMP6_FILTER_SETBLOCKALL(&filter);
      ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED, &filter);
      if(setsockopt(ICMPSocket.native(), IPPROTO_ICMPV6, ICMP6_FILTER,
                    &filter, sizeof(struct icmp6_filter)) < 0) {
         std::cerr << "WARNING: Unable to set ICMP6_FILTER!" << std::endl;
      }
   }


   // ====== Send Echo Requests =============================================
   for(unsigned int ttl = MaxTTL; ttl > 0; ttl--) {
      // ------ Set TTL ----------------------------------------
      const boost::asio::ip::unicast::hops option(ttl);
      ICMPSocket.set_option(option, errorCode);
      if(errorCode !=  boost::system::errc::success) {
         std::cerr << "ERROR: Unable to set TTL!" << std::endl;
         return;
      }

      // ------ Create an ICMP header for an echo request ------
      ICMPHeader echoRequest;
      echoRequest.type((isIPv6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
      echoRequest.code(0);
      echoRequest.identifier(Identifier);
      echoRequest.seqNumber(++seqNumber);
      const boost::posix_time::ptime sendTime = boost::posix_time::microsec_clock::universal_time();
      const std::string body = boost::posix_time::to_iso_string(sendTime);
      computeInternet16(echoRequest, body.begin(), body.end());

      // ------ Encode the request packet ----------------------
      boost::asio::streambuf request_buffer;
      std::ostream os(&request_buffer);
      os << echoRequest << body;

      // ------ Send the request -------------------------------

      size_t s = ICMPSocket.send_to(request_buffer.data(), DestinationEndpoint);
      if(s < 1) {  // FIXME!
         printf("s=%d\n",(int)s);
      }

      // ------ Record the request -----------------------------
      HopResult hopResult(ttl, DestinationAddress, HopStatus::Unknown, sendTime);
      HopResultsMap.insert(std::pair<unsigned int, HopResult>(ttl,hopResult));
   }

   // ====== Set timeout timer ==============================================
   TimeoutTimer.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::seconds(3));
   TimeoutTimer.async_wait(boost::bind(&Traceroute::handleTimeout, this));

   // ====== Get handler for response messages ==============================
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


void Traceroute::recordResult(const boost::posix_time::ptime& receiveTime,
                              const ICMPHeader&               icmpHeader)
{
   // ====== Find corresponding request =====================================

   // ====== Get status =====================================================
   HopStatus status = HopStatus::Unknown;
   if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
       (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ) {
      status = HopStatus::TimeExceeded;
   }
   else if( (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ||
            (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
      if(isIPv6()) {
         switch(icmpHeader.code()) {
             case ICMP6_DST_UNREACH_ADMIN:
               status = HopStatus::UnreachableProhibited;
              break;
             case ICMP6_DST_UNREACH_BEYONDSCOPE:
               status = HopStatus::UnreachableScope;
              break;
             case ICMP6_DST_UNREACH_NOROUTE:
               status = HopStatus::UnreachableNetwork;
              break;
             case ICMP6_DST_UNREACH_ADDR:
               status = HopStatus::UnreachableHost;
              break;
             case ICMP6_DST_UNREACH_NOPORT:
               status = HopStatus::UnreachablePort;
              break;
             default:
               status = HopStatus::UnreachableUnknown;
              break;
         }
      }
      else {
         switch(icmpHeader.code()) {
             case ICMP_PKT_FILTERED:
               status = HopStatus::UnreachableProhibited;
              break;
             case ICMP_NET_UNREACH:
             case ICMP_NET_UNKNOWN:
               status = HopStatus::UnreachableNetwork;
              break;
             case ICMP_HOST_UNREACH:
             case ICMP_HOST_UNKNOWN:
               status = HopStatus::UnreachableHost;
              break;
             case ICMP_PORT_UNREACH:
               status = HopStatus::UnreachablePort;
              break;
             default:
               status = HopStatus::UnreachableUnknown;
              break;
         }
      }
   }
   else if( (icmpHeader.type() == ICMPHeader::IPv6EchoReply) ||
            (icmpHeader.type() == ICMPHeader::IPv4EchoReply) ) {
      status = HopStatus::Success;
   }

   // ====== Get hop ========================================================
   printf("STATUS: %d   body=%d\n",(int)status,0);
}


void Traceroute::handleMessage(std::size_t length)
{
   const boost::posix_time::ptime receiveTime = boost::posix_time::microsec_clock::universal_time();
//    ReplyBuffer.commit(length);

   boost::interprocess::bufferstream is(buf, length);

   printf("r=%d\n",(int)length);

   ICMPHeader icmpHeader;
   if(isIPv6()) {
      is >> icmpHeader;
      if(is) {
         if( (icmpHeader.type() == ICMPHeader::IPv6NeighborSolicitation)  ||
             (icmpHeader.type() == ICMPHeader::IPv6NeighborAdvertisement) ||
             (icmpHeader.type() == ICMPHeader::IPv6RouterAdvertisement)   ||
             (icmpHeader.type() == ICMPHeader::IPv6RouterAdvertisement) ) {
            // just ignore ...
         }
         else if(icmpHeader.type() == ICMPHeader::IPv6EchoReply) {
            puts("REPLY");
            if(icmpHeader.identifier() == Identifier) {
               std::cout << "@@@@@@@ Good REPLY: from " << ReplyEndpoint.address().to_string()
                           << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;
               recordResult(receiveTime, icmpHeader);
            }
         }
         else if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
                  (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ) {
            IPv6Header ipv6Header;
            is >> ipv6Header;
            if(is) {
               std::cout << "@@@@@@@ Good ICMP of router: from " << ReplyEndpoint.address().to_string()
                         << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;
               recordResult(receiveTime, icmpHeader);
            }
         }
         else          printf("  => T=%d\n",icmpHeader.type());

      } else puts("not ICMP!");
   }
   else {
      IPv4Header ipv4Header;
      is >> ipv4Header;
      if(is) {
         is >> icmpHeader;
         if(is) {
            if(icmpHeader.type() == ICMPHeader::IPv4EchoReply) {
               if(icmpHeader.identifier() == Identifier) {
                  std::cout << "@@@@@@@ Good REPLY: from " << ReplyEndpoint.address().to_string()
                            << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;
                  recordResult(receiveTime, icmpHeader);
               }
            }
            else if(icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) {
               IPv4Header innerIPv4Header;
               ICMPHeader innerICMPHeader;
               is >> innerIPv4Header >> innerICMPHeader;
               if(is) {
                  if( (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ||
                      (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
                     if(innerICMPHeader.identifier() == Identifier) {
                        std::cout << "@@@@@@@ Good ICMP of router: from " << ReplyEndpoint.address().to_string()
                                  << " ID=" << (int)icmpHeader.identifier() <<" t=" << (int)icmpHeader.type() << " c=" << (int)icmpHeader.code() << " #=" << (int)icmpHeader.seqNumber() << std::endl;
                        recordResult(receiveTime, icmpHeader);
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
