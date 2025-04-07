// ==========================================================================
//     _   _ _ ____            ____          _____
//    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
//    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
//    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
//    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
//
//       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
//                 https://www.nntb.no/~dreibh/hipercontracer/
// ==========================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2025 by Thomas Dreibholz
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

#include "iomodule-icmp.h"
#include "assure.h"
#include "tools.h"
#include "logger.h"
#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"

#include <boost/interprocess/streams/bufferstream.hpp>

#ifdef __linux__
#include <linux/errqueue.h>
#endif


// NOTE: The registration was moved to iomodule-base.cc, due to linking issues!
// REGISTER_IOMODULE(ProtocolType::PT_ICMP, "ICMP", ICMPModule);


// ###### Constructor #######################################################
ICMPModule::ICMPModule(boost::asio::io_context&                 ioContext,
                       std::map<unsigned short, ResultEntry*>&  resultsMap,
                       const boost::asio::ip::address&          sourceAddress,
                       const uint16_t                           sourcePort,
                       const uint16_t                           destinationPort,
                       std::function<void (const ResultEntry*)> newResultCallback,
                       const unsigned int                       packetSize)
   : IOModuleBase(ioContext, resultsMap, sourceAddress, sourcePort, destinationPort,
                  newResultCallback),
     ICMPSocket(IOContext, (sourceAddress.is_v6() == true) ? boost::asio::ip::icmp::v6() :
                                                             boost::asio::ip::icmp::v4() ),
     UDPSocket(IOContext, (sourceAddress.is_v6() == true) ? boost::asio::ip::udp::v6() :
                                                            boost::asio::ip::udp::v4() )
{
   // Overhead: IPv4 Header (20)/IPv6 Header (40) + ICMP Header (8)
   PayloadSize      = std::max((ssize_t)MIN_TRACESERVICE_HEADER_SIZE,
                               (ssize_t)packetSize -
                                  (ssize_t)((SourceAddress.is_v6() == true) ? 40 : 20) - 8);
   ActualPacketSize = ((SourceAddress.is_v6() == true) ? 40 : 20) + 8 + PayloadSize;
   ExpectingReply   = false;
   ExpectingError   = false;
}


// ###### Destructor ########################################################
ICMPModule::~ICMPModule()
{
}


// ###### Prepare ICMP socket ###############################################
bool ICMPModule::prepareSocket()
{
   // ====== Bind UDP socket to given source address ========================
   boost::system::error_code      errorCode;
   boost::asio::ip::udp::endpoint udpSourceEndpoint(SourceAddress, SourcePort);
   UDPSocket.bind(udpSourceEndpoint, errorCode);
   if(errorCode != boost::system::errc::success) {
      HPCT_LOG(error) << getName() << ": Unable to bind UDP socket to source address "
                      << udpSourceEndpoint << "!";
      return false;
   }
   UDPSocketEndpoint = UDPSocket.local_endpoint();

   // ====== Choose identifier ==============================================
   // On modern systems, the PID is 32 bits. Therefore, using the UDP
   // socket's local port number as ID:
   sockaddr* in = (sockaddr*)UDPSocketEndpoint.data();
   if(in->sa_family == AF_INET) {
      Identifier = ntohs(((sockaddr_in*)in)->sin_port);
   }
   else {
      assure(in->sa_family == AF_INET6);
      Identifier = ntohs(((sockaddr_in6*)in)->sin6_port);
   }

   // ====== Bind ICMP socket to given source address =======================
   ICMPSocket.bind(boost::asio::ip::icmp::endpoint(SourceAddress, 0), errorCode);
   if(errorCode !=  boost::system::errc::success) {
      HPCT_LOG(error) << getName() << ": Unable to bind ICMP socket to source address "
                      << SourceAddress << "!";
      return false;
   }

   // ====== Configure sockets (timestamping, etc.) =========================
   if(!configureSocket(ICMPSocket.native_handle(), SourceAddress)) {
      return false;
   }

   // ====== Set filter (not required, but more efficient) ==================
   if(SourceAddress.is_v6()) {
      icmp6_filter filter;
      ICMP6_FILTER_SETBLOCKALL(&filter);
      ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY,     &filter);
      ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED,  &filter);
      ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH,    &filter);
      if(setsockopt(ICMPSocket.native_handle(), IPPROTO_ICMPV6, ICMP6_FILTER,
                    &filter, sizeof(struct icmp6_filter)) < 0) {
         HPCT_LOG(warning) << "Unable to set ICMP6_FILTER!";
      }
   }
#if defined (ICMP_FILTER)
   else {
      icmp_filter filter;
      filter.data = ~( (1 << ICMP_ECHOREPLY)      |
                       (1 << ICMP_TIME_EXCEEDED)  |
                       (1 << ICMP_DEST_UNREACH) );
      if(setsockopt(ICMPSocket.native_handle(), IPPROTO_ICMP, ICMP_FILTER,
                    &filter, sizeof(filter)) < 0) {
         HPCT_LOG(warning) << "Unable to set ICMP_FILTER!";
      }
  }
#else
#if !defined(__FreeBSD__)
#warning No ICMP_FILTER!
#endif
#endif

   // ====== Await incoming message or error ================================
   expectNextReply(ICMPSocket.native_handle(), true);
   expectNextReply(ICMPSocket.native_handle(), false);

   return true;
}


// ###### Expect next ICMP message ##########################################
void ICMPModule::expectNextReply(const int  socketDescriptor,
                                 const bool readFromErrorQueue)
{
   if(socketDescriptor == ICMPSocket.native_handle()) {
      if(readFromErrorQueue == true) {
         assure(ExpectingError == false);
         ICMPSocket.async_wait(
            boost::asio::ip::icmp::socket::wait_error,
            std::bind(&ICMPModule::handleResponse, this,
                     std::placeholders::_1, ICMPSocket.native_handle(), true)
         );
         ExpectingError = true;
      }
      else {
         assure(ExpectingReply == false);
         ICMPSocket.async_wait(
            boost::asio::ip::icmp::socket::wait_read,
            std::bind(&ICMPModule::handleResponse, this,
                     std::placeholders::_1, ICMPSocket.native_handle(), false)
         );
         ExpectingReply = true;
      }
   }
}


// ###### Cancel socket operations ##########################################
void ICMPModule::cancelSocket()
{
   ICMPSocket.cancel();
}


// ###### Send one ICMP request to given destination ########################
unsigned int ICMPModule::sendRequest(const DestinationInfo& destination,
                                     const unsigned int     fromTTL,
                                     const unsigned int     toTTL,
                                     const unsigned int     fromRound,
                                     const unsigned int     toRound,
                                     uint16_t&              seqNumber,
                                     uint32_t*              targetChecksumArray)
{
   const boost::asio::ip::icmp::endpoint remoteEndpoint(destination.address(), 0);
   const boost::asio::ip::icmp::endpoint localEndpoint(SourceAddress.is_unspecified() ?
                                                          findSourceForDestination(destination.address()) :
                                                          SourceAddress,
                                                       0);

   // ====== Set TOS/Traffic Class ==========================================
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
      return 0;
   }

   // ====== Prepare TraceService header ====================================
   TraceServiceHeader tsHeader(PayloadSize);
   tsHeader.magicNumber(MagicNumber);
   tsHeader.checksumTweak(0);

   // ====== Prepare ICMP header ============================================
   ICMPHeader echoRequest;
   echoRequest.type((SourceAddress.is_v6() == true) ?
                       ICMP6_ECHO_REQUEST : ICMP_ECHO);
   echoRequest.code(0);
   echoRequest.identifier(Identifier);

   // ====== Message scatter/gather array ===================================
   const std::array<boost::asio::const_buffer, 2> buffer {
      boost::asio::buffer(echoRequest.data(), echoRequest.size()),
      boost::asio::buffer(tsHeader.data(),    tsHeader.size())
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
   assure(fromRound <= toRound);
   assure(fromTTL >= toTTL);
   unsigned int messagesSent = 0;
   int currentTTL            = -1;
   // ------ BEGIN OF TIMING-CRITICAL PART ----------------------------------
   for(unsigned int round = fromRound; round <= toRound; round++) {
      for(int ttl = (int)fromTTL; ttl >= (int)toTTL; ttl--) {
         assure(currentEntry < entries);
         seqNumber++;   // New sequence number!

         // ====== Set TTL ==================================================
         if(ttl != currentTTL) {
            // Only need to set option again if it differs from current TTL!
            const boost::asio::ip::unicast::hops hopsOption(ttl);
            ICMPSocket.set_option(hopsOption, errorCodeArray[currentEntry]);
            currentTTL = ttl;
         }

         // ====== Update ICMP header =======================================
         uint32_t icmpChecksum = 0;
         echoRequest.seqNumber(seqNumber);
         echoRequest.checksum(0);   // Reset the original checksum first!
         echoRequest.computeInternet16(icmpChecksum);

         // ====== Update TraceService header ===============================
         tsHeader.sendTTL(ttl);
         tsHeader.round((unsigned char)round);
         tsHeader.checksumTweak(0);
         const ResultTimePoint sendTime = nowInUTC<ResultTimePoint>();
         tsHeader.sendTimeStamp(sendTime);
         tsHeader.computeInternet16(icmpChecksum);
         // Update ICMP checksum:
         echoRequest.checksum(finishInternet16(icmpChecksum));

         // ------ No given target checksum ---------------------------------
         if(targetChecksumArray[round] == ~0U) {
            targetChecksumArray[round] = echoRequest.checksum();
         }
         // ------ Target checksum given ------------------------------------
         else {
            // RFC 1624: Checksum 0xffff == -0 cannot occur, since there is
            //           always at least one non-zero field in each packet!
            assure(targetChecksumArray[round] != 0xffff);

            const uint16_t originalChecksum = echoRequest.checksum();

            // Compute value to tweak checksum to target value
            uint16_t diff = 0xffff - (targetChecksumArray[round] - originalChecksum);
            if(originalChecksum > targetChecksumArray[round]) {    // Handle necessary sum wrap!
               diff++;
            }
            tsHeader.checksumTweak(diff);

            // Compute new checksum (must be equal to target checksum!)
            icmpChecksum = 0;
            echoRequest.checksum(0);   // Reset the original checksum first!
            echoRequest.computeInternet16(icmpChecksum);
            tsHeader.computeInternet16(icmpChecksum);
            echoRequest.checksum(finishInternet16(icmpChecksum));
            assure(echoRequest.checksum() == targetChecksumArray[round]);
         }
         assure((targetChecksumArray[round] & ~0xffff) == 0);

         // ====== Send the request =========================================
         sentArray[currentEntry] =
            ICMPSocket.send_to(buffer, remoteEndpoint, 0, errorCodeArray[currentEntry]);

         // ====== Store message information ================================
         resultEntryArray[currentEntry]->initialise(
            TimeStampSeqID,
            round, seqNumber, ttl, ActualPacketSize,
            (uint16_t)targetChecksumArray[round], 0, 0,
            sendTime,
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
   assure(currentEntry == entries);

   // ====== Check results ==================================================
   for(unsigned int i = 0; i < entries; i++) {
      std::pair<std::map<unsigned short, ResultEntry*>::iterator, bool> result =
         ResultsMap.insert(std::pair<unsigned short, ResultEntry*>(
                              resultEntryArray[i]->seqNumber(),
                              resultEntryArray[i]));
      assure(result.second == true);
      if( (errorCodeArray[i]) || (sentArray[i] <= 0) ) {
         resultEntryArray[i]->failedToSend(errorCodeArray[i]);
         HPCT_LOG(debug) << getName() << ": sendRequest() - send_to("
                         << SourceAddress << "->" << destination << ") failed: "
                         << errorCodeArray[i].message();
      }
   }

   return messagesSent;
}


// ###### Handle incoming message from regular or from error queue ##########
void ICMPModule::handleResponse(const boost::system::error_code& errorCode,
                                const int                        socketDescriptor,
                                const bool                       readFromErrorQueue)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      // ====== Ensure to request further messages later ====================
      if(socketDescriptor == ICMPSocket.native_handle()) {
         if(!readFromErrorQueue) {
            ExpectingReply = false;   // Need to call expectNextReply() to get next message!
         }
         else {
            ExpectingError = false;   // Need to call expectNextReply() to get next error!
         }
      }

      // ====== Read all messages ===========================================
      if(!errorCode) {
         while(true) {
            // ====== Read message and control data =========================
            iovec              iov;
            msghdr             msg;
            sockaddr_storage   replyAddress;

            iov.iov_base       = &MessageBuffer;
            iov.iov_len        = sizeof(MessageBuffer);
            msg.msg_name       = (sockaddr*)&replyAddress;
            msg.msg_namelen    = sizeof(replyAddress);
            msg.msg_iov        = &iov;
            msg.msg_iovlen     = 1;
            msg.msg_flags      = 0;
            msg.msg_control    = ControlBuffer;
            msg.msg_controllen = sizeof(ControlBuffer);

#if defined (MSG_ERRQUEUE)
            const ssize_t length =
               recvmsg(socketDescriptor, &msg,
                       (readFromErrorQueue == true) ? MSG_ERRQUEUE|MSG_DONTWAIT : MSG_DONTWAIT);
#else
            assure(readFromErrorQueue == false);
            const ssize_t length = recvmsg(socketDescriptor, &msg, MSG_DONTWAIT);
#endif
            // NOTE: length == 0 for control data without user data!
            if(length < 0) {
               break;
            }


            // ====== Handle control data ===================================
            ReceivedData receivedData;
            receivedData.ReplyEndpoint          = boost::asio::ip::udp::endpoint();
            receivedData.ApplicationReceiveTime = nowInUTC<ResultTimePoint>();
            receivedData.ReceiveSWSource        = TimeSourceType::TST_Unknown;
            receivedData.ReceiveSWTime          = ResultTimePoint();
            receivedData.ReceiveHWSource        = TimeSourceType::TST_Unknown;
            receivedData.ReceiveHWTime          = ResultTimePoint();
            receivedData.MessageBuffer          = (char*)&MessageBuffer;
            receivedData.MessageLength          = length;

            sock_extended_err* socketError          = nullptr;
            sock_extended_err* socketTXTimestamping = nullptr;
            scm_timestamping*  socketTimestamp      = nullptr;
            for(cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
               // printf("Level %u, Type %u\n", cmsg->cmsg_level, cmsg->cmsg_type);
               if(cmsg->cmsg_level == SOL_SOCKET) {
#if defined (SO_TIMESTAMPING)
                  if(cmsg->cmsg_type == SO_TIMESTAMPING) {
                     socketTimestamp     = (scm_timestamping*)CMSG_DATA(cmsg);
                     if(socketTimestamp->ts[2].tv_sec != 0) {
                        // Hardware timestamp (raw):
                        receivedData.ReceiveHWSource = TimeSourceType::TST_TIMESTAMPING_HW;
                        receivedData.ReceiveHWTime   = ResultTimePoint(
                                                          std::chrono::seconds(socketTimestamp->ts[2].tv_sec) +
                                                          std::chrono::nanoseconds(socketTimestamp->ts[2].tv_nsec));
                     }
                     if(socketTimestamp->ts[0].tv_sec != 0) {
                        // Software timestamp (system clock):
                        receivedData.ReceiveSWSource = TimeSourceType::TST_TIMESTAMPING_SW;
                        receivedData.ReceiveSWTime   = ResultTimePoint(
                                                          std::chrono::seconds(socketTimestamp->ts[0].tv_sec) +
                                                          std::chrono::nanoseconds(socketTimestamp->ts[0].tv_nsec));
                     }
                  }
                  else
#endif
#if defined (SO_TIMESTAMPNS)
                  if(cmsg->cmsg_type == SO_TIMESTAMPNS) {
                     const timespec* ts = (const timespec*)CMSG_DATA(cmsg);
                     receivedData.ReceiveSWSource = TimeSourceType::TST_TIMESTAMPNS;
                     receivedData.ReceiveSWTime   = ResultTimePoint(
                                                       std::chrono::seconds(ts->tv_sec) +
                                                       std::chrono::nanoseconds(ts->tv_nsec));
                  }
                  else if(cmsg->cmsg_type == SO_TIMESTAMP) {
#endif
#if defined (SO_TS_CLOCK)
                     const timespec* ts = (const timespec*)CMSG_DATA(cmsg);
                     receivedData.ReceiveSWSource = TimeSourceType::TST_TIMESTAMPNS;
                     receivedData.ReceiveSWTime   = ResultTimePoint(
                                                       std::chrono::seconds(ts->tv_sec) +
                                                       std::chrono::nanoseconds(ts->tv_nsec));
#else
                     const timeval* tv = (const timeval*)CMSG_DATA(cmsg);
                     receivedData.ReceiveSWSource = TimeSourceType::TST_TIMESTAMP;
                     receivedData.ReceiveSWTime   = ResultTimePoint(
                                                       std::chrono::seconds(tv->tv_sec) +
                                                       std::chrono::microseconds(tv->tv_usec));
#endif
#if defined (SO_TIMESTAMPNS)
                  }
#endif
               }

#if defined (MSG_ERRQUEUE)
               else if(cmsg->cmsg_level == SOL_IPV6) {
                  if(cmsg->cmsg_type == IPV6_RECVERR) {
                     socketError = (sock_extended_err*)CMSG_DATA(cmsg);
                     if(socketError->ee_origin ==  SO_EE_ORIGIN_TIMESTAMPING) {
                        socketTXTimestamping = socketError;
                     }
                     else if( (socketError->ee_origin != SO_EE_ORIGIN_ICMP6) &&
                              (socketError->ee_origin != SO_EE_ORIGIN_LOCAL) ) {
                        socketError = nullptr;   // Unexpected content!
                     }
                  }
               }
               else if(cmsg->cmsg_level == SOL_IP) {
                  if(cmsg->cmsg_type == IP_RECVERR) {
                     socketError = (sock_extended_err*)CMSG_DATA(cmsg);
                     if( (socketError->ee_origin == SO_EE_ORIGIN_TIMESTAMPING) &&
                         (socketError->ee_errno == ENOMSG) ) {
                        socketTXTimestamping = socketError;
                     }
                     else if( (socketError->ee_origin != SO_EE_ORIGIN_ICMP) &&
                              (socketError->ee_origin != SO_EE_ORIGIN_LOCAL) ) {
                        socketError = nullptr;   // Unexpected content!
                     }
                  }
               }
#endif
            }

            // ====== TX Timestamping information via error queue ===========
#if defined (SO_TIMESTAMPNS)
            if( (readFromErrorQueue) && (socketTXTimestamping != nullptr) ) {
               if(socketTimestamp != nullptr) {
                  updateSendTimeInResultEntry(socketTXTimestamping, socketTimestamp);
               }
               // This is just the timestamp -> nothing more to do here!
               continue;
            }
#endif

#if defined (SIOCGSTAMPNS) || defined (SIOCGSTAMP)
            // ====== No timestamping, yet? Try SIOCGSTAMPNS/SIOCGSTAMP =====
            if(receivedData.ReceiveSWSource == TimeSourceType::TST_Unknown) {
               // NOTE: Assuming SIOCGSTAMPNS/SIOCGSTAMP deliver software time stamps!

               // ------ Linux: get reception time via SIOCGSTAMPNS ---------
               timespec ts;
               timeval  tv;
               if(ioctl(socketDescriptor, SIOCGSTAMPNS, &ts) == 0) {
                  // Got reception time from kernel via SIOCGSTAMPNS
                  receivedData.ReceiveSWSource = TimeSourceType::TST_SIOCGSTAMPNS;
                  receivedData.ReceiveSWTime   = ResultTimePoint(
                                                    std::chrono::seconds(ts.tv_sec) +
                                                    std::chrono::nanoseconds(ts.tv_nsec));
               }
               // ------ Linux: get reception time via SIOCGSTAMP -----------
               else if(ioctl(socketDescriptor, SIOCGSTAMP, &tv) == 0) {
                  // Got reception time from kernel via SIOCGSTAMP
                  receivedData.ReceiveSWSource = TimeSourceType::TST_SIOCGSTAMP;
                  receivedData.ReceiveSWTime   = ResultTimePoint(
                                                    std::chrono::seconds(tv.tv_sec) +
                                                    std::chrono::microseconds(tv.tv_usec));
               }
            }
#endif

            // ====== Get reply address =====================================
            // Using UDP endpoint as generic container to store address:port!
            receivedData.ReplyEndpoint =
               sockaddrToEndpoint<boost::asio::ip::udp::endpoint>(
                  (sockaddr*)msg.msg_name, msg.msg_namelen);


            // ====== Handle reply data =====================================
            if(!readFromErrorQueue) {
               handlePayloadResponse(socketDescriptor, receivedData);
            }

            else {
               handleErrorResponse(socketDescriptor, receivedData, socketError);
            }
         }
      }

      expectNextReply(socketDescriptor, readFromErrorQueue);
   }
}


// ###### Update ResultEntry with send timestamp information ################
void ICMPModule::updateSendTimeInResultEntry(const sock_extended_err* socketError,
                                             const scm_timestamping*  socketTimestamp)
{
#if defined (SO_TIMESTAMPING)
   for(std::map<unsigned short, ResultEntry*>::iterator iterator = ResultsMap.begin();
       iterator != ResultsMap.end(); iterator++) {
      ResultEntry* resultsEntry = iterator->second;
      if(resultsEntry->timeStampSeqID() == socketError->ee_data) {
         int             txTimeStampType = -1;
         int             txTimeSource    = -1;
         ResultTimePoint txTimePoint;
         if(socketTimestamp->ts[2].tv_sec != 0) {
            // Hardware timestamp (raw):
            txTimeSource = TimeSourceType::TST_TIMESTAMPING_HW;
            txTimePoint  = ResultTimePoint(
                              std::chrono::seconds(socketTimestamp->ts[2].tv_sec) +
                              std::chrono::nanoseconds(socketTimestamp->ts[2].tv_nsec));
            switch(socketError->ee_info) {
               case SCM_TSTAMP_SND:
                  txTimeStampType = TXTimeStampType::TXTST_TransmissionHW;
                break;
               default:
                  HPCT_LOG(warning) << "Got unexpected HW timestamp with socketError->ee_info="
                                    << socketError->ee_info;
                break;
            }
         }
         if(socketTimestamp->ts[0].tv_sec != 0) {
            // Software timestamp (system time from kernel):
            txTimeSource = TimeSourceType::TST_TIMESTAMPING_SW;
            txTimePoint  = ResultTimePoint(
                              std::chrono::seconds(socketTimestamp->ts[0].tv_sec) +
                              std::chrono::nanoseconds(socketTimestamp->ts[0].tv_nsec));
            switch(socketError->ee_info) {
               case SCM_TSTAMP_SCHED:
                  txTimeStampType = TXTimeStampType::TXTST_SchedulerSW;
                break;
               case SCM_TSTAMP_SND:
                  txTimeStampType = TXTimeStampType::TXTST_TransmissionSW;
                break;
               default:
                  HPCT_LOG(warning) << "Got unexpected SW timestamp with socketError->ee_info="
                                    << socketError->ee_info;
                break;
            }
         }
         if( (txTimeStampType >= 0) && (txTimeSource >= 0) ) {
            resultsEntry->setSendTime((TXTimeStampType)txTimeStampType,
                                      (TimeSourceType)txTimeSource, txTimePoint);
         }
         else {
            HPCT_LOG(warning) << "Got unexpected timestamping information";
         }
         return;   // Done!
      }
   }
   HPCT_LOG(warning) << "Not found: timeStampSeqID=" << socketError->ee_data;
#endif
}


// ###### Handle payload response (i.e. not from error queue) ###############
void ICMPModule::handlePayloadResponse(const int     socketDescriptor,
                                       ReceivedData& receivedData)
{
   // ====== Handle ICMP header =============================================
   boost::interprocess::bufferstream is(receivedData.MessageBuffer,
                                        receivedData.MessageLength);

   // ------ IPv6 -----------------------------------------------------------
   ICMPHeader icmpHeader;
   if(SourceAddress.is_v6()) {
      is >> icmpHeader;
      if(is) {

         // ------ IPv6 -> ICMPv6[Echo Reply] -------------------------------
         if( (icmpHeader.type() == ICMP6_ECHO_REPLY) &&
             (icmpHeader.identifier() == Identifier) ) {
            // ------ TraceServiceHeader ------------------------------------
            TraceServiceHeader tsHeader;
            is >> tsHeader;
            if(is) {
               if(tsHeader.magicNumber() == MagicNumber) {
                  // This is ICMP payload checked by the kernel =>
                  // not setting receivedData.Source and receivedData.Destination here!
                  recordResult(receivedData,
                               icmpHeader.type(), icmpHeader.code(),
                               icmpHeader.seqNumber(),
                               40 + receivedData.MessageLength);
               }
            }
         }

         // ------ IPv6 -> ICMPv6[Error] ------------------------------------
         else if( (icmpHeader.type() == ICMP6_TIME_EXCEEDED) ||
                  (icmpHeader.type() == ICMP6_DST_UNREACH) ) {
            IPv6Header innerIPv6Header;
            ICMPHeader innerICMPHeader;
            TraceServiceHeader tsHeader;
            is >> innerIPv6Header >> innerICMPHeader >> tsHeader;
            if( (is) &&
                (innerIPv6Header.nextHeader() == IPPROTO_ICMPV6) &&
                (innerICMPHeader.identifier() == Identifier) &&
                (tsHeader.magicNumber() == MagicNumber) ) {
               receivedData.Source      = boost::asio::ip::udp::endpoint(innerIPv6Header.sourceAddress(), 0);
               receivedData.Destination = boost::asio::ip::udp::endpoint(innerIPv6Header.destinationAddress(), 0);
               recordResult(receivedData,
                            icmpHeader.type(), icmpHeader.code(),
                            innerICMPHeader.seqNumber(),
                            40 + receivedData.MessageLength);
            }
         }

      }
   }

   // ------ IPv4 -----------------------------------------------------------
   else {
      // NOTE: For IPv4, also the IPv4 header of the message is included!
      IPv4Header ipv4Header;
      is >> ipv4Header;
      if( (is) && (ipv4Header.protocol() == IPPROTO_ICMP) ) {
         is >> icmpHeader;
         if(is) {

            // ------ IPv4 -> ICMP[Echo Reply] ------------------------------
            if( (icmpHeader.type() == ICMP_ECHOREPLY) &&
                (icmpHeader.identifier() == Identifier) ) {
               // ------ TraceServiceHeader ---------------------------------
               TraceServiceHeader tsHeader;
               is >> tsHeader;
               if( (is) && (tsHeader.magicNumber() == MagicNumber) ) {
                  // NOTE: This is the reponse
                  //       -> source and destination are swapped!
                  receivedData.Source      = boost::asio::ip::udp::endpoint(ipv4Header.destinationAddress(), 0);
                  receivedData.Destination = boost::asio::ip::udp::endpoint(ipv4Header.sourceAddress(), 0);
                  recordResult(receivedData,
                               icmpHeader.type(), icmpHeader.code(),
                               icmpHeader.seqNumber(),
                               receivedData.MessageLength);
               }
            }

            // ------ IPv4 -> ICMP[Error] -----------------------------------
            else if( (icmpHeader.type() == ICMP_TIMXCEED) ||
                     (icmpHeader.type() == ICMP_UNREACH) ) {
               IPv4Header innerIPv4Header;
               ICMPHeader innerICMPHeader;
               is >> innerIPv4Header >> innerICMPHeader;
               if( (is) &&
                   (innerIPv4Header.protocol() == IPPROTO_ICMP) &&
                   (innerICMPHeader.identifier() == Identifier) ) {
                  // Unfortunately, ICMPv4 does not return the full
                  // TraceServiceHeader here! So, the sequence number
                  // has to be used to identify the outgoing request!
                  receivedData.Source      = boost::asio::ip::udp::endpoint(innerIPv4Header.sourceAddress(), 0);
                  receivedData.Destination = boost::asio::ip::udp::endpoint(innerIPv4Header.destinationAddress(), 0);
                  recordResult(receivedData,
                               icmpHeader.type(), icmpHeader.code(),
                               innerICMPHeader.seqNumber(),
                               receivedData.MessageLength);
               }
            }

         }
      }
   }
}


// ###### Handle error response (i.e. from error queue) #####################
void ICMPModule::handleErrorResponse(const int          socketDescriptor,
                                     ReceivedData&      receivedData,
                                     sock_extended_err* socketError)
{
   // Nothing to do here! ICMP error responses are the actual ICMP payload!
}
