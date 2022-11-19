// =================================================================
//          #     #                 #     #
//          ##    #   ####   #####  ##    #  ######   #####
//          # #   #  #    #  #    # # #   #  #          #
//          #  #  #  #    #  #    # #  #  #  #####      #
//          #   # #  #    #  #####  #   # #  #          #
//          #    ##  #    #  #   #  #    ##  #          #
//          #     #   ####   #    # #     #  ######     #
//
//       ---   The NorNet Testbed for Multi-Homed Systems  ---
//                       https://www.nntb.no
// =================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2022 by Thomas Dreibholz
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

#include "iomodule.h"
#include "tools.h"
#include "logger.h"
#include "icmpheader.h"
// #include "ipv4header.h"
// #include "ipv6header.h"
#include "traceserviceheader.h"

// #include <netinet/in.h>
// #include <netinet/ip.h>

// #include <functional>
// #include <boost/format.hpp>
// #include <boost/version.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
// #if BOOST_VERSION >= 106600
// #include <boost/uuid/detail/sha1.hpp>
// #else
// #include <boost/uuid/sha1.hpp>
// #endif

// #ifdef __linux__
// #include <linux/sockios.h>
// #endif


// ###### Constructor #######################################################
IOModuleBase::IOModuleBase(const std::string&              name,
                           boost::asio::io_service&        ioService,
                           const boost::asio::ip::address& sourceAddress,
                           const unsigned int              packetSize)
   : Name(name),
     IOService(ioService),
     SourceAddress(sourceAddress),
     PacketSize(packetSize),
     MagicNumber( ((std::rand() & 0xffff) << 16) | (std::rand() & 0xffff) )
{
}


// ###### Destructor ########################################################
IOModuleBase::~IOModuleBase()
{
}


// ###### Constructor #######################################################
ICMPModule::ICMPModule(const std::string&              name,
                       boost::asio::io_service&        ioService,
                       const boost::asio::ip::address& sourceAddress,
                       const unsigned int              packetSize)
   : IOModuleBase(name + "/ICMPPing", ioService, sourceAddress, packetSize),
     PayloadSize( std::max((ssize_t)MIN_TRACESERVICE_HEADER_SIZE,
                           (ssize_t)PacketSize -
                              (ssize_t)((SourceAddress.is_v6() == true) ? 40 : 20) -
                              (ssize_t)sizeof(ICMPHeader)) ),
     ActualPacketSize( ((SourceAddress.is_v6() == true) ? 40 : 20) + sizeof(ICMPHeader) + PayloadSize ),
     ICMPSocket(IOService, (sourceAddress.is_v6() == true) ? boost::asio::ip::icmp::v6() : boost::asio::ip::icmp::v4())
{
   Identifier = 0;
   SeqNumber  = (unsigned short)(std::rand() & 0xffff);
}


// ###### Destructor ########################################################
ICMPModule::~ICMPModule()
{
}


// ###### Send one ICMP request to given destination ########################
void ICMPModule::sendRequest(const DestinationInfo& destination,
                             const unsigned int     ttl,
                             const unsigned int     round,
                             uint32_t&              targetChecksum)
{
//    // ====== Compute payload size and packet size ===========================
//    const size_t payloadSize =
//       (size_t)std::max((ssize_t)MIN_TRACESERVICE_HEADER_SIZE,
//                        (ssize_t)PacketSize -
//                           (ssize_t)((SourceAddress.is_v6() == true) ? 40 : 20) -
//                           (ssize_t)sizeof(ICMPHeader));
//    const size_t actualPacketSize = ((SourceAddress.is_v6() == true) ? 40 : 20) +
//                                       sizeof(ICMPHeader) +
//                                       payloadSize;

   // ====== Set TTL ========================================================
   const boost::asio::ip::unicast::hops option(ttl);
   ICMPSocket.set_option(option);

   // ====== Create an ICMP header for an echo request ======================
   SeqNumber++;

   ICMPHeader echoRequest;
   echoRequest.type((SourceAddress.is_v6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
   echoRequest.code(0);
   echoRequest.identifier(Identifier);
   echoRequest.seqNumber(SeqNumber);

   TraceServiceHeader tsHeader(PayloadSize);
   tsHeader.magicNumber(MagicNumber);
   tsHeader.sendTTL(ttl);
   tsHeader.round((unsigned char)round);
   tsHeader.checksumTweak(0);
   const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();
   tsHeader.sendTimeStamp(makePacketTimeStamp(sendTime));

   // ====== Tweak checksum =================================================
   std::vector<unsigned char> tsHeaderContents = tsHeader.contents();
   // ------ No given target checksum ---------------------
   if(targetChecksum == ~0U) {
      computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
      targetChecksum = echoRequest.checksum();
   }
   // ------ Target checksum given ------------------------
   else {
      // Compute current checksum
      computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
      const uint16_t originalChecksum = echoRequest.checksum();

      // Compute value to tweak checksum to target value
      uint16_t diff = 0xffff - (targetChecksum - originalChecksum);
      if(originalChecksum > targetChecksum) {    // Handle necessary sum wrap!
         diff++;
      }
      tsHeader.checksumTweak(diff);

      // Compute new checksum (must be equal to target checksum!)
      tsHeaderContents = tsHeader.contents();
      computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
      assert(echoRequest.checksum() == targetChecksum);
   }

   // ====== Encode the request packet ======================================
   boost::asio::streambuf request_buffer;
   std::ostream os(&request_buffer);
   os << echoRequest << tsHeader;

   // ====== Send the request ===============================================
   std::size_t sent;
   try {
      int level;
      int option;
      int trafficClass = destination.trafficClass();
      if(destination.address().is_v6()) {
         level  = IPPROTO_IPV6;
         option = IPV6_TCLASS;
      }
      else {
         level  = IPPROTO_IP;
         option = IP_TOS;
      }

      if(setsockopt(ICMPSocket.native_handle(), level, option,
                    &trafficClass, sizeof(trafficClass)) < 0) {
         HPCT_LOG(warning) << "Unable to set Traffic Class!";
         sent = -1;
      }
      else {
         sent = ICMPSocket.send_to(request_buffer.data(), boost::asio::ip::icmp::endpoint(destination.address(), 0));
      }
   }
   catch(boost::system::system_error const& e) {
      sent = -1;
   }
   if(sent < 1) {
      HPCT_LOG(warning) << getName() << ": ICMPModule::sendICMPRequest() - ICMP send_to("
                        << SourceAddress << "->" << destination << ") failed!";
   }
   else {
      // ====== Record the request ==========================================
      OutstandingRequests++;

      assert((targetChecksum & ~0xffff) == 0);
      ResultEntry resultEntry(round, SeqNumber, ttl, actualPacketSize,
                              (uint16_t)targetChecksum, sendTime,
                              destination, Unknown);
      std::pair<std::map<unsigned short, ResultEntry>::iterator, bool> result = ResultsMap.insert(std::pair<unsigned short, ResultEntry>(SeqNumber,resultEntry));
      assert(result.second == true);
   }
}
