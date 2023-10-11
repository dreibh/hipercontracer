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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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

#include "iomodule-udp.h"
#include "tools.h"
#include "logger.h"
#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "internet16.h"
#include "udpheader.h"
#include "traceserviceheader.h"

#include <boost/interprocess/streams/bufferstream.hpp>

#ifdef __linux__
#include <linux/errqueue.h>
#endif


REGISTER_IOMODULE(ProtocolType::PT_UDP, "UDP", UDPModule);


// ###### Constructor #######################################################
UDPModule::UDPModule(boost::asio::io_service&                 ioService,
                     std::map<unsigned short, ResultEntry*>&  resultsMap,
                     const boost::asio::ip::address&          sourceAddress,
                     std::function<void (const ResultEntry*)> newResultCallback,
                     const unsigned int                       packetSize,
                     const uint16_t                           destinationPort)
   : ICMPModule(ioService, resultsMap, sourceAddress,
                newResultCallback, packetSize),
     DestinationPort(destinationPort),
     RawUDPSocket(IOService, (sourceAddress.is_v6() == true) ? raw_udp::v6() :
                                                               raw_udp::v4() )
{
   // Overhead: IPv4 Header (20)/IPv6 Header (40) + UDP Header (8)
   PayloadSize      = std::max((ssize_t)MIN_TRACESERVICE_HEADER_SIZE,
                               (ssize_t)packetSize -
                                  (ssize_t)((SourceAddress.is_v6() == true) ? 40 : 20) - 8);
   ActualPacketSize = ((SourceAddress.is_v6() == true) ? 40 : 20) + 8 + PayloadSize;
}


// ###### Destructor ########################################################
UDPModule::~UDPModule()
{
}


// ###### Prepare UDP socket ################################################
bool UDPModule::prepareSocket()
{
   // ====== Prepare ICMP socket and create UDP socket ======================
   if(!ICMPModule::prepareSocket()) {
      return false;
   }

   // ====== Configure sockets (timestamping, etc.) =========================
   if(!configureSocket(UDPSocket.native_handle(), SourceAddress)) {
      return false;
   }
   if(!configureSocket(RawUDPSocket.native_handle(), SourceAddress)) {
      return false;
   }
   const int on = 1;
   if(SourceAddress.is_v6() == true) {
#ifdef __FreeBSD__
      abort();   // FIXME! Check IPV6_HDRINCL!!!
#else
      if(setsockopt(RawUDPSocket.native_handle(), IPPROTO_IPV6, IPV6_HDRINCL, &on, sizeof(on)) < 0) {
         HPCT_LOG(error) << "Unable to enable IPV6_HDRINCL option on socket: "
                         << strerror(errno);
         return false;
      }
#endif
   }
   else {
      if(setsockopt(RawUDPSocket.native_handle(), IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
         HPCT_LOG(error) << "Unable to enable IP_HDRINCL option on socket: "
                         << strerror(errno);
         return false;
      }
   }

   // ====== Await incoming message or error ================================
   expectNextReply(UDPSocket.native_handle(), true);
   expectNextReply(UDPSocket.native_handle(), false);
   expectNextReply(RawUDPSocket.native_handle(), true);
   expectNextReply(RawUDPSocket.native_handle(), false);

   return true;
}


// // ###### Expect next message ############################################
void UDPModule::expectNextReply(const int  socketDescriptor,
                                const bool readFromErrorQueue)
{
   if(socketDescriptor == UDPSocket.native_handle()) {
      UDPSocket.async_wait(
         (readFromErrorQueue == true) ?
            boost::asio::ip::udp::socket::wait_error :
            boost::asio::ip::udp::socket::wait_read,
         std::bind(&ICMPModule::handleResponse, this,
                   std::placeholders::_1,
                   UDPSocket.native_handle(), readFromErrorQueue)
      );
   }
   else if(socketDescriptor == RawUDPSocket.native_handle()) {
      RawUDPSocket.async_wait(
         (readFromErrorQueue == true) ?
            boost::asio::ip::udp::socket::wait_error :
            boost::asio::ip::udp::socket::wait_read,
         std::bind(&ICMPModule::handleResponse, this,
                   std::placeholders::_1,
                   RawUDPSocket.native_handle(), readFromErrorQueue)
      );
   }
   else {
      ICMPModule::expectNextReply(socketDescriptor, readFromErrorQueue);
   }
}


// ###### Cancel socket operations ##########################################
void UDPModule::cancelSocket()
{
   UDPSocket.cancel();
   RawUDPSocket.cancel();
   ICMPModule::cancelSocket();
}


// ###### Send one UDP request to given destination ########################
unsigned int UDPModule::sendRequest(const DestinationInfo& destination,
                                    const unsigned int     fromTTL,
                                    const unsigned int     toTTL,
                                    const unsigned int     fromRound,
                                    const unsigned int     toRound,
                                    uint16_t&              seqNumber,
                                    uint32_t*              targetChecksumArray)
{
   // NOTE:
   // - RawUDPSocket is used for sending the raw UDP packet
   // - UDPSocket is used for receiving the response. This socket is already
   //   bound to the ANY address and source port.
   // - If SourceAddress is the ANY address: find the actual source address
   const raw_udp::endpoint remoteEndpoint(destination.address(),
                                          SourceAddress.is_v6() ? 0 : DestinationPort);
   const raw_udp::endpoint localEndpoint((UDPSocketEndpoint.address().is_unspecified() ?
                                            findSourceForDestination(destination.address()) :
                                            UDPSocketEndpoint.address()),
                                         UDPSocketEndpoint.port());

   // ====== Prepare TraceService header ====================================
   TraceServiceHeader tsHeader(PayloadSize);
   tsHeader.magicNumber(MagicNumber);

   // ====== Prepare UDP header =============================================
   UDPHeader udpHeader;
   udpHeader.sourcePort(localEndpoint.port());
   udpHeader.destinationPort(DestinationPort);
   udpHeader.length(8 + PayloadSize);

   // ====== Prepare IP header ==============================================
   IPv6Header       ipv6Header;
   IPv4Header       ipv4Header;
   IPv6PseudoHeader ipv6PseudoHeader;
   IPv4PseudoHeader ipv4PseudoHeader;
   if(SourceAddress.is_v6()) {
      ipv6Header.version(6);
      ipv6Header.trafficClass(destination.trafficClass());
      ipv6Header.flowLabel(0);
      ipv6Header.payloadLength(8 + PayloadSize);
      ipv6Header.nextHeader(IPPROTO_UDP);
      ipv6Header.sourceAddress(localEndpoint.address().to_v6());
      ipv6Header.destinationAddress(destination.address().to_v6());

      ipv6PseudoHeader = IPv6PseudoHeader(ipv6Header, udpHeader.length());
   }
   else {
      ipv4Header.version(4);
      ipv4Header.typeOfService(destination.trafficClass());
      ipv4Header.headerLength(20);
      ipv4Header.totalLength(ActualPacketSize);
      ipv4Header.fragmentOffset(0);
      ipv4Header.protocol(IPPROTO_UDP);
      ipv4Header.sourceAddress(localEndpoint.address().to_v4());
      ipv4Header.destinationAddress(destination.address().to_v4());

      ipv4PseudoHeader = IPv4PseudoHeader(ipv4Header, udpHeader.length());
   }

   // ====== Message scatter/gather array ===================================
   const std::array<boost::asio::const_buffer, 3> buffer {
      SourceAddress.is_v6() ? boost::asio::buffer(ipv6Header.data(), ipv6Header.size()) :
                              boost::asio::buffer(ipv4Header.data(), ipv4Header.size()),
      boost::asio::buffer(udpHeader.data(), udpHeader.size()),
      boost::asio::buffer(tsHeader.data(),  tsHeader.size())
   };

   // ====== Prepare ResultEntry array ======================================
   const unsigned int        entries      = (1 + (toRound -fromRound)) * (1 + (fromTTL -toTTL));
   unsigned int              currentEntry = 0;
   ResultEntry*              resultEntryArray[entries];
   boost::system::error_code errorCodeArray[entries];
   std::size_t               sentArray[entries];
   for(unsigned int i = 0; i < entries; i++) {
      resultEntryArray[i] = new ResultEntry;
   }

   // ====== Sender loop ====================================================
   assert(fromRound <= toRound);
   assert(fromTTL >= toTTL);
   unsigned int messagesSent = 0;
   // ------ BEGIN OF TIMING-CRITICAL PART ----------------------------------
   for(unsigned int round = fromRound; round <= toRound; round++) {
      for(int ttl = (int)fromTTL; ttl >= (int)toTTL; ttl--) {
         assert(currentEntry < entries);
         seqNumber++;   // New sequence number!

         // ====== Update IP header =========================================
         if(SourceAddress.is_v6()) {
            ipv6Header.hopLimit(ttl);
         }
         else {
            ipv4Header.timeToLive(ttl);
            // NOTE: Using IPv4 Identification for the sequence number!
            ipv4Header.identification(seqNumber);
            ipv4Header.headerChecksum(0);
         }

         // ====== Update UDP header ========================================
         udpHeader.checksum(0);

         // ====== Update TraceService header ===============================
         tsHeader.seqNumber(seqNumber);
         tsHeader.sendTTL(ttl);
         tsHeader.round((unsigned char)round);
         const ResultTimePoint sendTime = nowInUTC<ResultTimePoint>();
         tsHeader.sendTimeStamp(sendTime);

         // ====== Compute checksums ========================================
         uint32_t udpChecksum = 0;
         udpHeader.computeInternet16(udpChecksum);
         if(SourceAddress.is_v6()) {
            ipv6PseudoHeader.computeInternet16(udpChecksum);
         }
         else {
            ipv4PseudoHeader.computeInternet16(udpChecksum);

            uint32_t ipv4HeaderChecksum = 0;
            ipv4Header.computeInternet16(ipv4HeaderChecksum);
            ipv4Header.headerChecksum(finishInternet16(ipv4HeaderChecksum));
         }
         tsHeader.computeInternet16(udpChecksum);
         udpHeader.checksum(finishInternet16(udpChecksum));

         // ====== Send the request =========================================
         sentArray[currentEntry] =
            RawUDPSocket.send_to(buffer, remoteEndpoint, 0, errorCodeArray[currentEntry]);

         // ====== Store message information ================================
         resultEntryArray[currentEntry]->initialise(
            TimeStampSeqID,
            round, seqNumber, ttl, ActualPacketSize,
            (uint16_t)targetChecksumArray[round], sendTime,
            localEndpoint.address(), destination, Unknown
         );
         if( (!errorCodeArray[currentEntry]) && (sentArray[currentEntry] > 0) ) {
            TimeStampSeqID++;
            messagesSent++;
         }

         currentEntry++;
      }
   }
   // ------ END OF TIMING-CRITICAL PART ------------------------------------
   assert(currentEntry == entries);

   // ====== Check results ==================================================
   for(unsigned int i = 0; i < entries; i++) {
      std::pair<std::map<unsigned short, ResultEntry*>::iterator, bool> result =
         ResultsMap.insert(std::pair<unsigned short, ResultEntry*>(
                              resultEntryArray[i]->seqNumber(),
                              resultEntryArray[i]));
      assert(result.second == true);
      if( (errorCodeArray[i]) || (sentArray[i] <= 0) ) {
         resultEntryArray[i]->failedToSend(errorCodeArray[i]);
         HPCT_LOG(debug) << getName() << ": sendRequest() - send_to("
                         << localEndpoint.address() << "->" << destination << ") failed: "
                         << errorCodeArray[i].message();
      }
   }

   return messagesSent;
}


// ###### Handle payload response (i.e. not from error queue) ###############
void UDPModule::handlePayloadResponse(const int     socketDescriptor,
                                      ReceivedData& receivedData)
{
   boost::interprocess::bufferstream is(receivedData.MessageBuffer,
                                        receivedData.MessageLength);

   // ====== UDP response ===================================================
   if(socketDescriptor == UDPSocket.native_handle()) {
      // ------ TraceServiceHeader ------------------------------------------
      TraceServiceHeader tsHeader;
      is >> tsHeader;
      if( (is) && (tsHeader.magicNumber() == MagicNumber) ) {
         recordResult(receivedData, 0, 0, tsHeader.seqNumber(),
                      ((SourceAddress.is_v6()) ? 40 + 8 : 20 + 8 ) + receivedData.MessageLength);
      }
   }

   // ====== ICMP error response ============================================
   else if(socketDescriptor == ICMPSocket.native_handle()) {

      // ------ IPv6 --------------------------------------------------------
      ICMPHeader icmpHeader;
      if(SourceAddress.is_v6()) {
         is >> icmpHeader;
         if(is) {

            // ------ IPv6 -> ICMPv6[Error] ---------------------------------
            if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
                (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ) {
               // ------ IPv6 -> ICMPv6[Error] -> IPv6 ----------------------
               IPv6Header innerIPv6Header;
               is >> innerIPv6Header;
               if( (is) && (innerIPv6Header.nextHeader() == IPPROTO_UDP) ) {
                  // NOTE: Addresses will be checked by recordResult()!
                  // ------ IPv6 -> ICMPv6[Error] -> IPv6 -> UDP ------------
                  UDPHeader udpHeader;
                  is >> udpHeader;
                  if( (is) &&
                     (udpHeader.sourcePort()      == UDPSocketEndpoint.port()) &&
                     (udpHeader.destinationPort() == DestinationPort) ) {
                     receivedData.Source      = boost::asio::ip::udp::endpoint(innerIPv6Header.sourceAddress(),      udpHeader.sourcePort());
                     receivedData.Destination = boost::asio::ip::udp::endpoint(innerIPv6Header.destinationAddress(), udpHeader.destinationPort());
                     // ------ TraceServiceHeader ---------------------------
                     TraceServiceHeader tsHeader;
                     is >> tsHeader;
                     if( (is) && (tsHeader.magicNumber() == MagicNumber) ) {
                        recordResult(receivedData,
                                     icmpHeader.type(), icmpHeader.code(),
                                     tsHeader.seqNumber(),
                                     receivedData.MessageLength);
                     }
                  }
               }
            }

         }
      }

      // ------ IPv4 --------------------------------------------------------
      else {
         // NOTE: For IPv4, also the IPv4 header of the message is included!
         IPv4Header ipv4Header;
         is >> ipv4Header;
         if( (is) && (ipv4Header.protocol() == IPPROTO_ICMP) ) {
            is >> icmpHeader;
            if(is) {

               // ------ IPv4 -> ICMP[Error] --------------------------------
               if( (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ||
                   (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
                  // ------ IPv4 -> ICMP[Error] -> IPv4 ---------------------
                  IPv4Header innerIPv4Header;
                  is >> innerIPv4Header;
                  if( (is) && (innerIPv4Header.protocol() == IPPROTO_UDP) ) {
                     // NOTE: Addresses will be checked by recordResult()!
                     // ------ IPv4 -> ICMP[Error] -> IPv4 -> UDP ------------
                     UDPHeader udpHeader;
                     is >> udpHeader;
                     if( (is) &&
                         (udpHeader.sourcePort()      == UDPSocketEndpoint.port()) &&
                         (udpHeader.destinationPort() == DestinationPort) ) {
                        receivedData.Source      = boost::asio::ip::udp::endpoint(innerIPv4Header.sourceAddress(),      udpHeader.sourcePort());
                        receivedData.Destination = boost::asio::ip::udp::endpoint(innerIPv4Header.destinationAddress(), udpHeader.destinationPort());
                        // Unfortunately, ICMPv4 does not return the full
                        // TraceServiceHeader here! So, the sequence number
                        // has to be used to identify the outgoing request!
                        recordResult(receivedData,
                                     icmpHeader.type(), icmpHeader.code(),
                                     innerIPv4Header.identification(),
                                     receivedData.MessageLength);
                     }
                  }
               }

            }
         }
      }

   }
}


// ###### Handle error response (i.e. from error queue) #####################
void UDPModule::handleErrorResponse(const int          socketDescriptor,
                                    ReceivedData&      receivedData,
                                    sock_extended_err* socketError)
{
   // Nothing to do here!
}
