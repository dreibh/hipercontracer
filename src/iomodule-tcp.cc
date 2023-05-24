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

#include "iomodule-tcp.h"
#include "tools.h"
#include "logger.h"
#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "internet16.h"
#include "tcpheader.h"
#include "traceserviceheader.h"

#include <boost/interprocess/streams/bufferstream.hpp>

#ifdef __linux__
#include <linux/errqueue.h>
#endif


REGISTER_IOMODULE(ProtocolType::PT_TCP, "TCP", TCPModule);


// ###### Constructor #######################################################
TCPModule::TCPModule(boost::asio::io_service&                 ioService,
                     std::map<unsigned short, ResultEntry*>&  resultsMap,
                     const boost::asio::ip::address&          sourceAddress,
                     std::function<void (const ResultEntry*)> newResultCallback,
                     const unsigned int                       packetSize,
                     const uint16_t                           destinationPort)
   : ICMPModule(ioService, resultsMap, sourceAddress,
                newResultCallback, packetSize),
     DestinationPort(destinationPort),
     RawTCPSocket(IOService, (sourceAddress.is_v6() == true) ? raw_tcp::v6() :
                                                               raw_tcp::v4() )
{
   // Overhead: IPv4 Header (20)/IPv6 Header (40) + TCP Header (20)
   PayloadSize      = std::max((ssize_t)MIN_TRACESERVICE_HEADER_SIZE,
                               (ssize_t)packetSize -
                                  (ssize_t)((SourceAddress.is_v6() == true) ? 40 : 20) - 20);
   ActualPacketSize = ((SourceAddress.is_v6() == true) ? 40 : 20) + 20 + PayloadSize;
}


// ###### Destructor ########################################################
TCPModule::~TCPModule()
{
}


// ###### Prepare TCP socket ################################################
bool TCPModule::prepareSocket()
{
   // ====== Prepare ICMP socket and create TCP socket ======================
   if(!ICMPModule::prepareSocket()) {
      return false;
   }

   // ====== Configure sockets (timestamping, etc.) =========================
   if(!configureSocket(TCPSocket.native_handle(), SourceAddress)) {
      return false;
   }
   if(!configureSocket(RawTCPSocket.native_handle(), SourceAddress)) {
      return false;
   }
   int on = 1;
   if(setsockopt(RawTCPSocket.native_handle(),
                 (SourceAddress.is_v6() == true) ? IPPROTO_IPV6 : IPPROTO_IP,
                 (SourceAddress.is_v6() == true) ? IPV6_HDRINCL : IP_HDRINCL,
                 &on, sizeof(on)) < 0) {
      HPCT_LOG(error) << "Unable to enable IP_HDRINCL/IPV6_HDRINCL option on socket: "
                        << strerror(errno);
      return false;
   }

   // ====== Await incoming message or error ================================
   expectNextReply(RawTCPSocket.native_handle(), true);
   expectNextReply(RawTCPSocket.native_handle(), false);

   return true;
}


// // ###### Expect next message ############################################
void TCPModule::expectNextReply(const int  socketDescriptor,
                                const bool readFromErrorQueue)
{
   if(socketDescriptor == TCPSocket.native_handle()) {
      TCPSocket.async_wait(
         (readFromErrorQueue == true) ?
            boost::asio::ip::tcp::socket::wait_error :
            boost::asio::ip::tcp::socket::wait_read,
         std::bind(&ICMPModule::handleResponse, this,
                   std::placeholders::_1,
                   TCPSocket.native_handle(), readFromErrorQueue)
      );
   }
   else if(socketDescriptor == RawTCPSocket.native_handle()) {
      RawTCPSocket.async_wait(
         (readFromErrorQueue == true) ?
            boost::asio::ip::tcp::socket::wait_error :
            boost::asio::ip::tcp::socket::wait_read,
         std::bind(&ICMPModule::handleResponse, this,
                   std::placeholders::_1,
                   RawTCPSocket.native_handle(), readFromErrorQueue)
      );
   }
   else {
      ICMPModule::expectNextReply(socketDescriptor, readFromErrorQueue);
   }
}


// ###### Cancel socket operations ##########################################
void TCPModule::cancelSocket()
{
   TCPSocket.cancel();
   RawTCPSocket.cancel();
   ICMPModule::cancelSocket();
}


// ###### Send one TCP request to given destination ########################
unsigned int TCPModule::sendRequest(const DestinationInfo& destination,
                                    const unsigned int     fromTTL,
                                    const unsigned int     toTTL,
                                    const unsigned int     fromRound,
                                    const unsigned int     toRound,
                                    uint16_t&              seqNumber,
                                    uint32_t*              targetChecksumArray)
{
   // NOTE:
   // - RawTCPSocket is used for sending the raw TCP packet
   // - TCPSocket is used for receiving the response. This socket is already
   //   bound to the ANY address and source port.
   // - If SourceAddress is the ANY address: find the actual source address
   const raw_tcp::endpoint remoteEndpoint(destination.address(),
                                          SourceAddress.is_v6() ? 0 : DestinationPort);
   const raw_tcp::endpoint localEndpoint((TCPSocketEndpoint.address().is_unspecified() ?
                                            findSourceForDestination(destination.address()) :
                                            TCPSocketEndpoint.address()),
                                         TCPSocketEndpoint.port());

   // ====== Prepare TraceService header ====================================
   TraceServiceHeader tsHeader(PayloadSize);
   tsHeader.magicNumber(MagicNumber);

   // ====== Prepare TCP header =============================================
   TCPHeader tcpHeader;
   tcpHeader.sourcePort(localEndpoint.port());
   tcpHeader.destinationPort(DestinationPort);
   // TCP sequence number: (Seq Number)(Seq Number)!
   tcpHeader.seqNumber(((uint32_t)seqNumber << 16) | seqNumber);
   tcpHeader.ackNumber(0);
   tcpHeader.dataOffset(20);
   tcpHeader.flags(TCPFlags::TF_SYN);
   tcpHeader.window(4096);
   tcpHeader.urgentPointer(0);

   // ====== Prepare IP header ==============================================
   IPv6Header       ipv6Header;
   IPv4Header       ipv4Header;
   IPv6PseudoHeader ipv6PseudoHeader;
   IPv4PseudoHeader ipv4PseudoHeader;
   if(SourceAddress.is_v6()) {
      ipv6Header.version(6);
      ipv6Header.trafficClass(destination.trafficClass());
      ipv6Header.flowLabel(0);
      ipv6Header.payloadLength(tcpHeader.dataOffset() + PayloadSize);
      ipv6Header.nextHeader(IPPROTO_TCP);
      ipv6Header.sourceAddress(localEndpoint.address().to_v6());
      ipv6Header.destinationAddress(destination.address().to_v6());

      ipv6PseudoHeader = IPv6PseudoHeader(ipv6Header, tcpHeader.dataOffset() + PayloadSize);
   }
   else {
      ipv4Header.version(4);
      ipv4Header.typeOfService(destination.trafficClass());
      ipv4Header.headerLength(20);
      ipv4Header.totalLength(ActualPacketSize);
      ipv4Header.fragmentOffset(0);
      ipv4Header.protocol(IPPROTO_TCP);
      ipv4Header.sourceAddress(localEndpoint.address().to_v4());
      ipv4Header.destinationAddress(destination.address().to_v4());

      ipv4PseudoHeader = IPv4PseudoHeader(ipv4Header, tcpHeader.dataOffset() + PayloadSize);
   }

   // ====== Message scatter/gather array ===================================
   const std::array<boost::asio::const_buffer, 3> buffer {
      SourceAddress.is_v6() ? boost::asio::buffer(ipv6Header.data(), ipv6Header.size()) :
                              boost::asio::buffer(ipv4Header.data(), ipv4Header.size()),
      boost::asio::buffer(tcpHeader.data(), tcpHeader.size()),
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

         // ====== Update TCP header ========================================
         tcpHeader.checksum(0);

         // ====== Update TraceService header ===============================
         tsHeader.seqNumber(seqNumber);
         tsHeader.sendTTL(ttl);
         tsHeader.round((unsigned char)round);
         const ResultTimePoint sendTime = ResultClock::now();
         tsHeader.sendTimeStamp(sendTime);

         // ====== Compute checksums ========================================
         uint32_t tcpChecksum = 0;
         tcpHeader.computeInternet16(tcpChecksum);
         if(SourceAddress.is_v6()) {
            ipv6PseudoHeader.computeInternet16(tcpChecksum);
         }
         else {
            ipv4PseudoHeader.computeInternet16(tcpChecksum);

            uint32_t ipv4HeaderChecksum = 0;
            ipv4Header.computeInternet16(ipv4HeaderChecksum);
            ipv4Header.headerChecksum(finishInternet16(ipv4HeaderChecksum));
         }
         tsHeader.computeInternet16(tcpChecksum);
         tcpHeader.checksum(finishInternet16(tcpChecksum));

         // ====== Send the request =========================================
         sentArray[currentEntry] =
            RawTCPSocket.send_to(buffer, remoteEndpoint, 0, errorCodeArray[currentEntry]);

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
void TCPModule::handlePayloadResponse(const int     socketDescriptor,
                                      ReceivedData& receivedData)
{
   boost::interprocess::bufferstream is(receivedData.MessageBuffer,
                                        receivedData.MessageLength);

   // ====== TCP response ===================================================
   if(socketDescriptor == RawTCPSocket.native_handle()) {

      // ------ IPv6 --------------------------------------------------------
      if(SourceAddress.is_v6()) {
         TCPHeader tcpHeader;
         is >> tcpHeader;
         if( (is) &&
               (tcpHeader.destinationPort() == TCPSocketEndpoint.port()) &&
               (tcpHeader.sourcePort()      == DestinationPort) ) {

            // For SYN+ACK: AckNumber = SeqNumber + 1!
            if( (tcpHeader.flags() & (TCPFlags::TF_SYN|TCPFlags::TF_ACK|TCPFlags::TF_RST)) == (TCPFlags::TF_SYN|TCPFlags::TF_ACK) ) {
               const uint32_t a  = tcpHeader.ackNumber() - 1;
               const uint16_t a1 = (uint16_t)((a & 0xffff0000) >> 16);
               const uint16_t a2 = ((uint16_t)(a & 0xffff));
               if(a1 == a2) {
                  // Addressing information is already checked by kernel!
                  recordResult(receivedData, 0, 0, a1);
               }
            }
            else if(tcpHeader.flags() & TCPFlags::TF_RST) {
               // For RST: AckNumber = SeqNumber + PayloadSize!
               const uint32_t r  = tcpHeader.ackNumber() - PayloadSize - 1;
               const uint16_t r1 = (uint16_t)((r & 0xffff0000) >> 16);
               const uint16_t r2 = ((uint16_t)(r & 0xffff));
               if(r1 == r2) {
                  // Addressing information is already checked by kernel!
                  recordResult(receivedData, 0, 0, r1);
               }
            }
         }
      }

      // ------ IPv4 --------------------------------------------------------
      else {
         IPv4Header ipv4Header;
         is >> ipv4Header;
         if( (is) && (ipv4Header.protocol() == IPPROTO_TCP) ) {
            TCPHeader tcpHeader;
            is >> tcpHeader;
            if( (is) &&
                (tcpHeader.destinationPort() == TCPSocketEndpoint.port()) &&
                (tcpHeader.sourcePort()      == DestinationPort) ) {

               // For SYN+ACK: AckNumber = SeqNumber + 1!
               if( (tcpHeader.flags() & (TCPFlags::TF_SYN|TCPFlags::TF_ACK|TCPFlags::TF_RST)) == (TCPFlags::TF_SYN|TCPFlags::TF_ACK) ) {
                  const uint32_t a  = tcpHeader.ackNumber() - 1;
                  const uint16_t a1 = (uint16_t)((a & 0xffff0000) >> 16);
                  const uint16_t a2 = ((uint16_t)(a & 0xffff));
                  if(a1 == a2) {
                     puts("SYN+ACK");
                     // Addressing information may need update!
                     receivedData.Destination = boost::asio::ip::udp::endpoint(ipv4Header.sourceAddress(),      tcpHeader.sourcePort());
                     receivedData.Source      = boost::asio::ip::udp::endpoint(ipv4Header.destinationAddress(), tcpHeader.destinationPort());
                     recordResult(receivedData, 0, 0, a1);
                  }
               }
               else if(tcpHeader.flags() & TCPFlags::TF_RST) {
                  // For RST: AckNumber = SeqNumber + PayloadSize!
                  const uint32_t r  = tcpHeader.ackNumber() - PayloadSize - 1;
                  const uint16_t r1 = (uint16_t)((r & 0xffff0000) >> 16);
                  const uint16_t r2 = ((uint16_t)(r & 0xffff));
                  if(r1 == r2) {
                     puts("RST");
                     // Addressing information may need update!
                     receivedData.Destination = boost::asio::ip::udp::endpoint(ipv4Header.sourceAddress(),      tcpHeader.sourcePort());
                     receivedData.Source      = boost::asio::ip::udp::endpoint(ipv4Header.destinationAddress(), tcpHeader.destinationPort());
                     recordResult(receivedData, 0, 0, r1);
                  }
               }
            }
         }
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
               if( (is) && (innerIPv6Header.nextHeader() == IPPROTO_TCP) ) {
                  // NOTE: Addresses will be checked by recordResult()!
                  // ------ IPv6 -> ICMPv6[Error] -> IPv6 -> TCP ------------
                  TCPHeader tcpHeader;
                  is >> tcpHeader;
                  if( (is) &&
                     (tcpHeader.sourcePort()      == TCPSocketEndpoint.port()) &&
                     (tcpHeader.destinationPort() == DestinationPort) ) {
                     receivedData.Source      = boost::asio::ip::udp::endpoint(innerIPv6Header.sourceAddress(),      tcpHeader.sourcePort());
                     receivedData.Destination = boost::asio::ip::udp::endpoint(innerIPv6Header.destinationAddress(), tcpHeader.destinationPort());
                     // ------ TraceServiceHeader ---------------------------
                     TraceServiceHeader tsHeader;
                     is >> tsHeader;
                     if( (is) && (tsHeader.magicNumber() == MagicNumber) ) {
                        recordResult(receivedData,
                                     icmpHeader.type(), icmpHeader.code(),
                                     tsHeader.seqNumber());
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
                  if( (is) && (innerIPv4Header.protocol() == IPPROTO_TCP) ) {
                     // NOTE: Addresses will be checked by recordResult()!
                    // ------ IPv4 -> ICMP[Error] -> IPv4 -> TCP ------------
                    TCPHeader tcpHeader;
                    is >> tcpHeader;
                    if( (is) &&
                        (tcpHeader.sourcePort()      == TCPSocketEndpoint.port()) &&
                        (tcpHeader.destinationPort() == DestinationPort) ) {
                       receivedData.Source      = boost::asio::ip::udp::endpoint(innerIPv4Header.sourceAddress(),      tcpHeader.sourcePort());
                       receivedData.Destination = boost::asio::ip::udp::endpoint(innerIPv4Header.destinationAddress(), tcpHeader.destinationPort());
                       // Unfortunately, ICMPv4 does not return the full
                       // TraceServiceHeader here! So, the sequence number
                       // has to be used to identify the outgoing request!
                       recordResult(receivedData,
                                    icmpHeader.type(), icmpHeader.code(),
                                    innerIPv4Header.identification());
                     }
                  }
               }

            }
         }
      }

   }
}


// ###### Handle error response (i.e. from error queue) #####################
void TCPModule::handleErrorResponse(const int          socketDescriptor,
                                    ReceivedData&      receivedData,
                                    sock_extended_err* socketError)
{
   // Nothing to do here!
}