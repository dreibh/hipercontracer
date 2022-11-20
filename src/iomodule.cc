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
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"

#include <iostream>  // FIXME!
#include <boost/interprocess/streams/bufferstream.hpp>

#ifdef __linux__
#include <linux/errqueue.h>
#include <linux/sockios.h>
#endif


template<class Endpoint>
Endpoint sockaddrToEndpoint(const sockaddr* address, const socklen_t socklen)
{
   if(socklen >= sizeof(sockaddr_in)) {
      if(address->sa_family == AF_INET) {
         return Endpoint( boost::asio::ip::address_v4(ntohl(((sockaddr_in*)address)->sin_addr.s_addr)),
                          ntohs(((sockaddr_in*)address)->sin_port) );
      }
      else if(address->sa_family == AF_INET6) {
         const unsigned char* aptr = ((sockaddr_in6*)address)->sin6_addr.s6_addr;
         boost::asio::ip::address_v6::bytes_type v6address;
         std::copy(aptr, aptr + v6address.size(), v6address.data());
         return Endpoint( boost::asio::ip::address_v6(v6address, ((sockaddr_in6*)address)->sin6_scope_id),
                          ntohs(((sockaddr_in6*)address)->sin6_port) );
      }
   }
   return Endpoint( boost::asio::ip::udp::endpoint() );
}



// ###### Constructor #######################################################
IOModuleBase::IOModuleBase(const std::string&                       name,
                           boost::asio::io_service&                 ioService,
                           std::map<unsigned short, ResultEntry*>&  resultsMap,
                           const boost::asio::ip::address&          sourceAddress,
                           const unsigned int                       packetSize,
                           std::function<void (const ResultEntry*)> newResultCallback)
   : Name(name),
     IOService(ioService),
     ResultsMap(resultsMap),
     SourceAddress(sourceAddress),
     PacketSize(packetSize),
     NewResultCallback(newResultCallback),
     MagicNumber( ((std::rand() & 0xffff) << 16) | (std::rand() & 0xffff) )
{
   Identifier = 0;
}


// ###### Destructor ########################################################
IOModuleBase::~IOModuleBase()
{
}


// ###### Constructor #######################################################
ICMPModule::ICMPModule(const std::string&                       name,
                       boost::asio::io_service&                 ioService,
                       std::map<unsigned short, ResultEntry*>&  resultsMap,
                       const boost::asio::ip::address&          sourceAddress,
                       const unsigned int                       packetSize,
                       std::function<void (const ResultEntry*)> newResultCallback)
   : IOModuleBase(name + "/ICMPPing", ioService, resultsMap, sourceAddress, packetSize, newResultCallback),
     PayloadSize( std::max((ssize_t)MIN_TRACESERVICE_HEADER_SIZE,
                           (ssize_t)PacketSize -
                              (ssize_t)((SourceAddress.is_v6() == true) ? 40 : 20) -
                              (ssize_t)sizeof(ICMPHeader)) ),
     ActualPacketSize( ((SourceAddress.is_v6() == true) ? 40 : 20) + sizeof(ICMPHeader) + PayloadSize ),
     ICMPSocket(IOService, (sourceAddress.is_v6() == true) ? boost::asio::ip::icmp::v6() : boost::asio::ip::icmp::v4())
{
   ExpectingReply = false;
}


// ###### Destructor ########################################################
ICMPModule::~ICMPModule()
{
}


// ###### Prepare ICMP socket ###############################################
bool ICMPModule::prepareSocket()
{
   // ====== Choose identifier ==============================================
   Identifier = ::getpid();   // Identifier is the process ID
   // NOTE: Assuming 16-bit PID, and one PID per thread!

   // ====== Bind ICMP socket to given source address =======================
   boost::system::error_code errorCode;
   ICMPSocket.bind(boost::asio::ip::icmp::endpoint(SourceAddress, 0), errorCode);
   if(errorCode !=  boost::system::errc::success) {
      HPCT_LOG(error) << getName() << ": Unable to bind ICMP socket to source address "
                      << SourceAddress << "!";
      return false;
   }

   // ====== Set filter (not required, but much more efficient) =============
   if(SourceAddress.is_v6()) {
      struct icmp6_filter filter;
      ICMP6_FILTER_SETBLOCKALL(&filter);
      ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED, &filter);
      if(setsockopt(ICMPSocket.native_handle(), IPPROTO_ICMPV6, ICMP6_FILTER,
                    &filter, sizeof(struct icmp6_filter)) < 0) {
         HPCT_LOG(warning) << "Unable to set ICMP6_FILTER!";
      }
   }
   return true;
}


// ###### Expect next ICMP message ##########################################
void ICMPModule::expectNextReply()
{
   if(ExpectingReply == false) {
      ICMPSocket.async_receive_from(boost::asio::buffer(MessageBuffer),
                                    ReplyEndpoint,
                                    std::bind(&ICMPModule::handleResponse, this,
                                             std::placeholders::_1,
                                             std::placeholders::_2));
      ExpectingReply = true;
   }
}


// ###### Cancel socket operations ##########################################
void ICMPModule::cancelSocket()
{
   ICMPSocket.cancel();
}


// ###### Send one ICMP request to given destination ########################
ResultEntry* ICMPModule::sendRequest(const DestinationInfo& destination,
                                     const unsigned int     ttl,
                                     const unsigned int     round,
                                     uint16_t&              seqNumber,
                                     uint32_t&              targetChecksum)
{
   // ====== Set TTL ========================================================
   const boost::asio::ip::unicast::hops option(ttl);
   ICMPSocket.set_option(option);

   // ====== Create an ICMP header for an echo request ======================
   seqNumber++;

   ICMPHeader echoRequest;
   echoRequest.type((SourceAddress.is_v6() == true) ?
      ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
   echoRequest.code(0);
   echoRequest.identifier(Identifier);
   echoRequest.seqNumber(seqNumber);

   TraceServiceHeader tsHeader(PayloadSize);
   tsHeader.magicNumber(MagicNumber);
   tsHeader.sendTTL(ttl);
   tsHeader.round((unsigned char)round);
   tsHeader.checksumTweak(0);
   const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();
   tsHeader.sendTimeStamp(sendTime);

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
   assert((targetChecksum & ~0xffff) == 0);

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
         sent = ICMPSocket.send_to(request_buffer.data(),
                                   boost::asio::ip::icmp::endpoint(destination.address(), 0));
      }
   }
   catch(boost::system::system_error const& e) {
      sent = -1;
   }

   // ====== Create ResultEntry on success ==================================
   if(sent > 0) {
      expectNextReply();   // Ensure to receive response!

      ResultEntry* resultEntry =
         new ResultEntry(round, seqNumber, ttl, ActualPacketSize,
                         (uint16_t)targetChecksum, sendTime,
                         destination, Unknown);
      assert(resultEntry != nullptr);

      std::pair<std::map<unsigned short, ResultEntry*>::iterator, bool> result =
         ResultsMap.insert(std::pair<unsigned short, ResultEntry*>(seqNumber, resultEntry));
      assert(result.second == true);

      return resultEntry;
   }
   HPCT_LOG(warning) << getName() << ": sendRequest() - send_to("
                     << SourceAddress << "->" << destination << ") failed!";
   return nullptr;
}


// ###### Handle incoming ICMP message ######################################
void ICMPModule::handleResponse(const boost::system::error_code& errorCode,
                                std::size_t                      length)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      if(!errorCode) {

         // ====== Obtain reception time ====================================
         std::chrono::system_clock::time_point receiveTime;
#ifdef __linux__
         struct timeval tv;
         if(ioctl(ICMPSocket.native_handle(), SIOCGSTAMP, &tv) == 0) {
            // Got reception time from kernel via SIOCGSTAMP
            receiveTime = std::chrono::system_clock::time_point(
                             std::chrono::seconds(tv.tv_sec) +
                             std::chrono::microseconds(tv.tv_usec));
         }
         else {
#endif
            // Fallback: SIOCGSTAMP did not return a result
            receiveTime = std::chrono::system_clock::now();
#ifdef __linux__
         }
#endif

         // ====== Handle ICMP header =======================================
         boost::interprocess::bufferstream is(MessageBuffer, length);
         ExpectingReply = false;   // Need to call expectNextReply() to get next message!

         ICMPHeader icmpHeader;
         if(SourceAddress.is_v6()) {
            is >> icmpHeader;
            if(is) {
               if(icmpHeader.type() == ICMPHeader::IPv6EchoReply) {
                  if(icmpHeader.identifier() == Identifier) {
                     TraceServiceHeader tsHeader;
                     is >> tsHeader;
                     if(is) {
                        if(tsHeader.magicNumber() == MagicNumber) {
                           recordResult(receiveTime, icmpHeader, icmpHeader.seqNumber());
                        }
                     }
                  }
               }
               else if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
                        (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ) {
                  IPv6Header innerIPv6Header;
                  ICMPHeader innerICMPHeader;
                  TraceServiceHeader tsHeader;
                  is >> innerIPv6Header >> innerICMPHeader >> tsHeader;
                  if(is) {
                     if(tsHeader.magicNumber() == MagicNumber) {
                        recordResult(receiveTime, icmpHeader, innerICMPHeader.seqNumber());
                     }
                  }
               }
            }
         }
         else {
            IPv4Header ipv4Header;
            is >> ipv4Header;
            if(is) {
               is >> icmpHeader;
               if(is) {
                  if(icmpHeader.type() == ICMPHeader::IPv4EchoReply) {
                     if(icmpHeader.identifier() == Identifier) {
                        TraceServiceHeader tsHeader;
                        is >> tsHeader;
                        if(is) {
                           if(tsHeader.magicNumber() == MagicNumber) {
                              recordResult(receiveTime, icmpHeader, icmpHeader.seqNumber());
                           }
                        }
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
                              // Unfortunately, ICMPv4 does not return the full TraceServiceHeader here!
                              recordResult(receiveTime, icmpHeader, innerICMPHeader.seqNumber());
                           }
                        }
                     }
                  }
               }
            }
         }
      }

      expectNextReply();
   }
}


// ###### Record result from response message ###############################
void ICMPModule::recordResult(const std::chrono::system_clock::time_point& receiveTime,
                              const ICMPHeader&                            icmpHeader,
                              const unsigned short                         seqNumber)
{
   // ====== Find corresponding request =====================================
   std::map<unsigned short, ResultEntry*>::iterator found = ResultsMap.find(seqNumber);
   if(found == ResultsMap.end()) {
      return;
   }
   ResultEntry* resultEntry = found->second;

   // ====== Get status =====================================================
   if(resultEntry->status() == Unknown) {
      resultEntry->setReceiveTime(receiveTime);
      // Just set address, keep traffic class and identifier settings:
      resultEntry->setDestinationAddress(ReplyEndpoint.address());

      HopStatus status = Unknown;
      if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
          (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ) {
         status = TimeExceeded;
      }
      else if( (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ||
               (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
         if(SourceAddress.is_v6()) {
            switch(icmpHeader.code()) {
               case ICMP6_DST_UNREACH_ADMIN:
                  status = UnreachableProhibited;
               break;
               case ICMP6_DST_UNREACH_BEYONDSCOPE:
                  status = UnreachableScope;
               break;
               case ICMP6_DST_UNREACH_NOROUTE:
                  status = UnreachableNetwork;
               break;
               case ICMP6_DST_UNREACH_ADDR:
                  status = UnreachableHost;
               break;
               case ICMP6_DST_UNREACH_NOPORT:
                  status = UnreachablePort;
               break;
               default:
                  status = UnreachableUnknown;
               break;
            }
         }
         else {
            switch(icmpHeader.code()) {
               case ICMP_UNREACH_FILTER_PROHIB:
                  status = UnreachableProhibited;
               break;
               case ICMP_UNREACH_NET:
               case ICMP_UNREACH_NET_UNKNOWN:
                  status = UnreachableNetwork;
               break;
               case ICMP_UNREACH_HOST:
               case ICMP_UNREACH_HOST_UNKNOWN:
                  status = UnreachableHost;
               break;
               case ICMP_UNREACH_PORT:
                  status = UnreachablePort;
               break;
               default:
                  status = UnreachableUnknown;
               break;
            }
         }
      }
      else if( (icmpHeader.type() == ICMPHeader::IPv6EchoReply) ||
               (icmpHeader.type() == ICMPHeader::IPv4EchoReply) ) {
         status  = Success;
      }
      resultEntry->setStatus(status);

      NewResultCallback(resultEntry);
   }
}





// ###### Constructor #######################################################
UDPModule::UDPModule(const std::string&                       name,
                     boost::asio::io_service&                 ioService,
                     std::map<unsigned short, ResultEntry*>&  resultsMap,
                     const boost::asio::ip::address&          sourceAddress,
                     const unsigned int                       packetSize,
                     std::function<void (const ResultEntry*)> newResultCallback)
   : IOModuleBase(name + "/UDPPing", ioService, resultsMap, sourceAddress, packetSize, newResultCallback),
     PayloadSize( std::max((ssize_t)MIN_TRACESERVICE_HEADER_SIZE,
                           (ssize_t)PacketSize -
                              (ssize_t)((SourceAddress.is_v6() == true) ? 40 : 20) -
                              (ssize_t)8) ),
     ActualPacketSize( ((SourceAddress.is_v6() == true) ? 40 : 20) + 8 + PayloadSize ),
     UDPSocket(IOService)
{
   ExpectingReply = false;
   ExpectingError = false;
}


// ###### Destructor ########################################################
UDPModule::~UDPModule()
{
}


// ###### Prepare UDP socket ################################################
bool UDPModule::prepareSocket()
{
   // ====== Choose identifier ==============================================
   Identifier = ::getpid();   // Identifier is the process ID
   // NOTE: Assuming 16-bit PID, and one PID per thread!

   // ====== Bind UDP socket to given source address ========================
   boost::system::error_code errorCode;
   boost::asio::ip::udp::endpoint sourceEndpoint(SourceAddress, 0);
   UDPSocket.open((SourceAddress.is_v6() == true) ? boost::asio::ip::udp::v6() : boost::asio::ip::udp::v4());
   UDPSocket.bind(sourceEndpoint, errorCode);
   if(errorCode !=  boost::system::errc::success) {
      HPCT_LOG(error) << getName() << ": Unable to bind UDP socket to source address "
                      << sourceEndpoint << "!";
      return false;
   }

   // ====== Enable RECVERR/IPV6_RECVERR option =============================
   const int on = 1;
   if(setsockopt(UDPSocket.native_handle(),
                 (SourceAddress.is_v6() == true) ? SOL_IPV6: SOL_IP,
                 (SourceAddress.is_v6() == true) ? IPV6_RECVERR : IP_RECVERR,
                 &on, sizeof(on)) < 0) {
      HPCT_LOG(error) << "Unable to enable RECVERR/IPV6_RECVERR option on UDP socket";
      return false;
   }

   // ====== Enable SO_TIMESTAMP option =====================================
   if(setsockopt(UDPSocket.native_handle(), SOL_SOCKET, SO_TIMESTAMP,
                 &on, sizeof(on)) < 0) {
      HPCT_LOG(error) << "Unable to enable RECVERR/IPV6_RECVERR option on UDP socket";
      return false;
   }

   return true;
}


// ###### Expect next UDP message ##########################################
void UDPModule::expectNextReply()
{
   if(ExpectingReply == false) {
      UDPSocket.async_wait(boost::asio::ip::tcp::socket::wait_read,
                           std::bind(&UDPModule::handleResponse, this,
                                     std::placeholders::_1, false));
      ExpectingReply = true;
   }
   if(ExpectingError == false) {
      UDPSocket.async_wait(boost::asio::ip::tcp::socket::wait_error,
                           std::bind(&UDPModule::handleResponse, this,
                                     std::placeholders::_1, true));
      ExpectingError = true;
   }
}


// ###### Cancel socket operations ##########################################
void UDPModule::cancelSocket()
{
   UDPSocket.cancel();
}


// ###### Send one UDP request to given destination ########################
ResultEntry* UDPModule::sendRequest(const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum)
{
   // ====== Set TTL ========================================================
   const boost::asio::ip::unicast::hops option(ttl);
   UDPSocket.set_option(option);

   // ====== Create request =================================================
   seqNumber++;

   TraceServiceHeader tsHeader(PayloadSize);
   tsHeader.magicNumber(MagicNumber);
   tsHeader.sendTTL(ttl);
   tsHeader.round((unsigned char)round);
   tsHeader.checksumTweak(seqNumber);   // FIXME!
   const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();
   tsHeader.sendTimeStamp(sendTime);

   // ====== Encode the request packet ======================================
   boost::asio::streambuf request_buffer;
   std::ostream os(&request_buffer);
   os << tsHeader;

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

      if(setsockopt(UDPSocket.native_handle(), level, option,
                    &trafficClass, sizeof(trafficClass)) < 0) {
         HPCT_LOG(warning) << "Unable to set Traffic Class!";
         sent = -1;
      }
      else {
         sent = UDPSocket.send_to(request_buffer.data(),
                                  boost::asio::ip::udp::endpoint(destination.address(), 7));   //FIXME!!!
      }
   }
   catch(boost::system::system_error const& e) {
      sent = -1;
   }

   // ====== Create ResultEntry on success ==================================
   if(sent > 0) {
      expectNextReply();   // Ensure to receive response!

      ResultEntry* resultEntry =
         new ResultEntry(round, seqNumber, ttl, ActualPacketSize,
                         (uint16_t)targetChecksum, sendTime,
                         destination, Unknown);
      assert(resultEntry != nullptr);

      std::pair<std::map<unsigned short, ResultEntry*>::iterator, bool> result =
         ResultsMap.insert(std::pair<unsigned short, ResultEntry*>(seqNumber, resultEntry));
      assert(result.second == true);

      return resultEntry;
   }
   HPCT_LOG(warning) << getName() << ": sendRequest() - send_to("
                     << SourceAddress << "->" << destination << ") failed!";
   return nullptr;
}


// ###### Handle incoming UDP message #######################################
void UDPModule::handleResponse(const boost::system::error_code& errorCode,
                               const bool                       readFromErrorQueue)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      // ====== Ensure to request further messages later ====================
      if(!readFromErrorQueue) {
         ExpectingReply = false;   // Need to call expectNextReply() to get next message!
      }
      else {
         ExpectingError = false;   // Need to call expectNextReply() to get next error!
      }

      // ====== Read all messages ===========================================
      if(!errorCode) {
         while(true) {
            // ====== Read message ==========================================
            iovec                                 iov;
            msghdr                                msg;
            sock_extended_err*                    socketError = nullptr;
            sockaddr_storage                      replyAddress;
            std::chrono::system_clock::time_point receiveTime;

            iov.iov_base       = &MessageBuffer;
            iov.iov_len        = sizeof(MessageBuffer);
            msg.msg_name       = (sockaddr*)&replyAddress;
            msg.msg_namelen    = sizeof(replyAddress);
            msg.msg_iov        = &iov;
            msg.msg_iovlen     = 1;
            msg.msg_flags      = 0;
            msg.msg_control    = ControlBuffer;
            msg.msg_controllen = sizeof(ControlBuffer);

            const ssize_t length = recvmsg(UDPSocket.native_handle(), &msg,
                                           (readFromErrorQueue == true) ? MSG_ERRQUEUE|MSG_DONTWAIT : MSG_DONTWAIT);
            if(length <= 0) {
               break;
            }

            // ====== Handle control data ===================================
            for(cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
               if(cmsg->cmsg_level == SOL_SOCKET) {
                  if(cmsg->cmsg_type == SO_TIMESTAMP) {
                     puts("TIMESTAMP!");
                     const timeval* tv = (const timeval*)CMSG_DATA(cmsg);
                     receiveTime = std::chrono::system_clock::time_point(
                                      std::chrono::seconds(tv->tv_sec) +
                                      std::chrono::microseconds(tv->tv_usec));
                  }
               }
               else if(cmsg->cmsg_level == SOL_IPV6) {
                  if(cmsg->cmsg_type == IPV6_HOPLIMIT) {
                     puts("IPV6_HOPLIMIT");
                  }
                  else if(cmsg->cmsg_type == IPV6_RECVERR) {
                     socketError = (sock_extended_err*)CMSG_DATA(cmsg);
                     if( (socketError->ee_origin != SO_EE_ORIGIN_ICMP6) &&
                         (socketError->ee_origin != SO_EE_ORIGIN_LOCAL) ) {
                        socketError = nullptr;   // Unexpected content!
                     }
                     else puts("IPV6_RECVERR!");
                  }
               }
               else if(cmsg->cmsg_level == SOL_IP) {
                  if(cmsg->cmsg_type == IP_RECVERR) {
                     socketError = (sock_extended_err*)CMSG_DATA(cmsg);
                     if( (socketError->ee_origin != SO_EE_ORIGIN_ICMP) &&
                         (socketError->ee_origin != SO_EE_ORIGIN_LOCAL) ) {
                        socketError = nullptr;   // Unexpected content!
                     }
                     else puts("IP_RECVERR!");
                  }
               }
            }

            // ====== Ensure to have the reception time =====================
            // Ideally, it was provided by SO_TIMESTAMP.
            if(receiveTime == std::chrono::system_clock::time_point()) {
               abort();   // FIXME!!!
#ifdef __linux__
               // ------ Linux: get reception time via SIOCGSTAMP -----------
               timeval tv;
               if(ioctl(UDPSocket.native_handle(), SIOCGSTAMP, &tv) == 0) {
                  // Got reception time from kernel via SIOCGSTAMP
                  receiveTime = std::chrono::system_clock::time_point(
                                 std::chrono::seconds(tv.tv_sec) +
                                 std::chrono::microseconds(tv.tv_usec));
               }
               // ------ Obtain current system time -------------------------
               else {
#endif
                  // Fallback: SIOCGSTAMP did not return a result
                  receiveTime = std::chrono::system_clock::now();
#ifdef __linux__
               }
#endif
            }

            // ====== Handle reply data =====================================
            if(!readFromErrorQueue) {
               // ====== Get reply address ==================================
               ReplyEndpoint = sockaddrToEndpoint<boost::asio::ip::udp::endpoint>(
                                  (sockaddr*)msg.msg_name, msg.msg_namelen);

               // ====== Handle TraceServiceHeader ==========================
               boost::interprocess::bufferstream is(MessageBuffer, length);

               TraceServiceHeader tsHeader;
               is >> tsHeader;
               if(is) {
                  if(tsHeader.magicNumber() == MagicNumber) {
                     printf("M  seq=%u\n", tsHeader.checksumTweak());
                     recordResult(receiveTime, ICMPHeader::IPv6EchoReply, 0,
                                  tsHeader.checksumTweak());   // FIXME!!!
                  }
               }
            }

            else {   // FIXME!
               // ====== Get reply address ==================================
               ReplyEndpoint = sockaddrToEndpoint<boost::asio::ip::udp::endpoint>(
                                  (sockaddr*)SO_EE_OFFENDER(socketError), sizeof(sockaddr_storage));

               boost::interprocess::bufferstream is(MessageBuffer, length);
               TraceServiceHeader tsHeader;
               is >> tsHeader;
               if(is) {
                  if(tsHeader.magicNumber() == MagicNumber) {
                     sockaddr* o = SO_EE_OFFENDER(socketError);
                     auto oo =
                     boost::asio::ip::udp::endpoint(
                        boost::asio::ip::address_v4( ntohl(((sockaddr_in*)o)->sin_addr.s_addr) ),
                        ntohs(((sockaddr_in*)o)->sin_port)
                     );

                     printf("el=%d   seq=%u\n", (int)length, tsHeader.checksumTweak());
                     std::cout << "EE=" << oo << "\n";

                     recordResult(receiveTime,
                                  socketError->ee_type, socketError->ee_code,
                                  tsHeader.checksumTweak());   // FIXME!!!
                  } else puts("E1");
               } else puts("E2");
            }
         }
      }
      expectNextReply();
   }
}


// ###### Record result from response message ###############################
void UDPModule::recordResult(const std::chrono::system_clock::time_point& receiveTime,
                             const unsigned int                           icmpType,
                             const unsigned int                           icmpCode,
                             const unsigned short                         seqNumber)
{
   // ====== Find corresponding request =====================================
   std::map<unsigned short, ResultEntry*>::iterator found = ResultsMap.find(seqNumber);
   if(found == ResultsMap.end()) {
      return;
   }
   ResultEntry* resultEntry = found->second;

   // ====== Get status =====================================================
   if(resultEntry->status() == Unknown) {
      resultEntry->setReceiveTime(receiveTime);
      // Just set address, keep traffic class and identifier settings:
      resultEntry->setDestinationAddress(ReplyEndpoint.address());

      HopStatus status = Unknown;
      if( (icmpType == ICMPHeader::IPv6TimeExceeded) ||
          (icmpType == ICMPHeader::IPv4TimeExceeded) ) {
         status = TimeExceeded;
      }
      else if( (icmpType == ICMPHeader::IPv6Unreachable) ||
               (icmpType == ICMPHeader::IPv4Unreachable) ) {
         if(SourceAddress.is_v6()) {
            switch(icmpCode) {
               case ICMP6_DST_UNREACH_ADMIN:
                  status = UnreachableProhibited;
               break;
               case ICMP6_DST_UNREACH_BEYONDSCOPE:
                  status = UnreachableScope;
               break;
               case ICMP6_DST_UNREACH_NOROUTE:
                  status = UnreachableNetwork;
               break;
               case ICMP6_DST_UNREACH_ADDR:
                  status = UnreachableHost;
               break;
               case ICMP6_DST_UNREACH_NOPORT:
                  status = UnreachablePort;
               break;
               default:
                  status = UnreachableUnknown;
               break;
            }
         }
         else {
            switch(icmpCode) {
               case ICMP_UNREACH_FILTER_PROHIB:
                  status = UnreachableProhibited;
               break;
               case ICMP_UNREACH_NET:
               case ICMP_UNREACH_NET_UNKNOWN:
                  status = UnreachableNetwork;
               break;
               case ICMP_UNREACH_HOST:
               case ICMP_UNREACH_HOST_UNKNOWN:
                  status = UnreachableHost;
               break;
               case ICMP_UNREACH_PORT:
                  status = UnreachablePort;
               break;
               default:
                  status = UnreachableUnknown;
               break;
            }
         }
      }
      else if( (icmpType == ICMPHeader::IPv6EchoReply) ||
               (icmpType == ICMPHeader::IPv4EchoReply) ) {
         status  = Success;
      }
      resultEntry->setStatus(status);

// // // //       HopStatus status = Unknown;
// // // //       if(icmpHeader == nullptr) {
// // // //          status  = Success;
// // // //       }
// // // //       resultEntry->setStatus(status);

      NewResultCallback(resultEntry);
   }
}
