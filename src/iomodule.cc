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
#include "internet16.h"
#include "udpheader.h"
#include "traceserviceheader.h"

#include <boost/interprocess/streams/bufferstream.hpp>

#include <ifaddrs.h>
#ifdef __linux__
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>

#include <iostream>   // FIXME!

// linux/icmp.h defines the socket option ICMP_FILTER, but this include
// conflicts with netinet/ip_icmp.h. Just adding the needed definitions here:
#define ICMP_FILTER 1
struct icmp_filter {
   __u32 data;
};
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
   return Endpoint();
}





std::list<IOModuleBase::RegisteredIOModule*>* IOModuleBase::IOModuleList = nullptr;


// ###### Constructor #######################################################
IOModuleBase::IOModuleBase(boost::asio::io_service&                 ioService,
                           std::map<unsigned short, ResultEntry*>&  resultsMap,
                           const boost::asio::ip::address&          sourceAddress,
                           std::function<void (const ResultEntry*)> newResultCallback)
   : IOService(ioService),
     ResultsMap(resultsMap),
     SourceAddress(sourceAddress),
     NewResultCallback(newResultCallback),
     MagicNumber( ((std::rand() & 0xffff) << 16) | (std::rand() & 0xffff) )
{
   Identifier       = 0;
   TimeStampSeqID   = 0;
   PayloadSize      = 0;
   ActualPacketSize = 0;
}


// ###### Destructor ########################################################
IOModuleBase::~IOModuleBase()
{
}


// ###### Configure socket (timestamping, etc.) #############################
bool IOModuleBase::configureSocket(const int                      socketDescriptor,
                                   const boost::asio::ip::address sourceAddress)
{
   // ====== Enable RECVERR/IPV6_RECVERR option =============================
   const int on = 1;
   if(setsockopt(socketDescriptor,
                 (sourceAddress.is_v6() == true) ? SOL_IPV6: SOL_IP,
                 (sourceAddress.is_v6() == true) ? IPV6_RECVERR : IP_RECVERR,
                 &on, sizeof(on)) < 0) {
      HPCT_LOG(error) << "Unable to enable RECVERR/IPV6_RECVERR option on socket";
      return false;
   }

   // ====== Try to use SO_TIMESTAMPING option ==============================
   static bool logTimestampType = true;
#if defined (SO_TIMESTAMPING)
   // Documentation: <linux-src>/Documentation/networking/timestamping.rst
   const int type =
      SOF_TIMESTAMPING_TX_HARDWARE|    /* Get hardware TX timestamp                     */
      SOF_TIMESTAMPING_RX_HARDWARE|    /* Get hardware RX timestamp                     */
      SOF_TIMESTAMPING_RAW_HARDWARE|   /* Hardware timestamps as set by the hardware    */

      SOF_TIMESTAMPING_TX_SOFTWARE|    /* Get software TX timestamp                     */
      SOF_TIMESTAMPING_RX_SOFTWARE|    /* Get software RX timestamp                     */
      SOF_TIMESTAMPING_SOFTWARE|       /* Get software timestamp as well                */

      SOF_TIMESTAMPING_OPT_ID|         /* Attach ID to packet (TimeStampSeqID)          */
      SOF_TIMESTAMPING_OPT_TSONLY|     /* Get only the timestamp, not the full packet   */
      SOF_TIMESTAMPING_OPT_TX_SWHW|    /* Get both, software and hardware TX time stamp */

      SOF_TIMESTAMPING_TX_SCHED        /* Get TX scheduling timestamp as well           */
      ;
   if(setsockopt(socketDescriptor, SOL_SOCKET, SO_TIMESTAMPING,
                 &type, sizeof(type)) < 0) {
      HPCT_LOG(error) << "Unable to enable SO_TIMESTAMPING option on socket: "
                      << strerror(errno);
#else
#warning No SO_TIMESTAMPING!
#endif

      // ====== Try to use SO_TIMESTAMPNS ===================================
#if defined (SO_TIMESTAMPNS)
      if(setsockopt(socketDescriptor, SOL_SOCKET, SO_TIMESTAMPNS,
                    &on, sizeof(on)) < 0) {

#else
#warning No SO_TIMESTAMPNS!
#endif

         // ====== Try to use SO_TIMESTAMP ==================================
         if(setsockopt(socketDescriptor, SOL_SOCKET, SO_TIMESTAMP,
                       &on, sizeof(on)) < 0) {
            HPCT_LOG(error) << "Unable to enable SO_TIMESTAMP option on socket: "
                            << strerror(errno);
            return false;
         }

#if defined (SO_TS_CLOCK)
         const int tdClockType =
         if(setsockopt(socketDescriptor, SOL_SOCKET, SO_TS_CLOCK,
                       &tdClockType, sizeof(tdClockType)) < 0) {
            HPCT_LOG(error) << "Unable to set SO_TS_CLOCK option on socket: "
                            << strerror(errno);
            return false;
         }
         if(logTimestampType) {
            HPCT_LOG(info) << "Using SO_TIMESTAMP+SO_TS_CLOCK (nanoseconds accuracy)";
            logTimestampType = false;
         }
         else {
#else
            HPCT_LOG(info) << "Using SO_TIMESTAMP (microseconds accuracy)";
#endif
#if defined (SO_TS_CLOCK)
         }
#endif

#if defined (SO_TIMESTAMPNS)
      }
      else {
         HPCT_LOG(info) << "Using SO_TIMESTAMPNS (nanoseconds accuracy)";
         logTimestampType = false;
      }
#endif
#if defined (SO_TIMESTAMPING)
   }
   else {
      if(logTimestampType) {
         HPCT_LOG(info) << "Using SO_TIMESTAMPING (nanoseconds accuracy)";
         logTimestampType = false;
      }

      // ====== Enable hardware timestamping, if possible ===================
      ifaddrs* ifaddr;
      if(getifaddrs(&ifaddr) == 0) {
         // ------ Get set of interfaces ------------------------------------
         std::set<std::string> interfaceSet;
         for(ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if( (ifa->ifa_addr != nullptr) &&
                ( (ifa->ifa_addr->sa_family == AF_INET) ||
                  (ifa->ifa_addr->sa_family == AF_INET6) ) ) {
               if(sourceAddress.is_unspecified()) {
                  // Source address is 0.0.0.0/:: -> add all interfaces
                  interfaceSet.insert(ifa->ifa_name);
               }
               else {
                  // Source address is specific -> find the corresponding interface
                  const boost::asio::ip::address address =
                     sockaddrToEndpoint<boost::asio::ip::icmp::endpoint>(
                        ifa->ifa_addr,
                        (ifa->ifa_addr->sa_family == AF_INET) ?
                           sizeof(sockaddr_in) :
                           sizeof(sockaddr_in6)
                     ).address();
                  if(address == sourceAddress) {
                     interfaceSet.insert(ifa->ifa_name);
                  }
               }
            }
         }
         freeifaddrs(ifaddr);

         // ====== Try to configure SIOCSHWTSTAMP ===========================
         static bool	logSIOCSHWTSTAMP = true;
         for(std::string interfaceName : interfaceSet) {
            hwtstamp_config hwTimestampConfig;
            memset(&hwTimestampConfig, 0, sizeof(hwTimestampConfig));
            hwTimestampConfig.tx_type   = HWTSTAMP_TX_ON;
            hwTimestampConfig.rx_filter = HWTSTAMP_FILTER_ALL;
            const hwtstamp_config hwTimestampConfigDesired = hwTimestampConfig;

            ifreq hwTimestampRequest;
            memset(&hwTimestampRequest, 0, sizeof(hwTimestampRequest));
            memcpy(&hwTimestampRequest.ifr_name,
                   interfaceName.c_str(), interfaceName.size() + 1);
            hwTimestampRequest.ifr_data = (char*)&hwTimestampConfig;

            if(ioctl(socketDescriptor, SIOCSHWTSTAMP, &hwTimestampRequest) < 0) {
               if(logSIOCSHWTSTAMP) {
                  if(errno == ENOTSUP) {
                     HPCT_LOG(info) << "Hardware timestamping not supported on interface "
                                    << interfaceName;
                  }
                  else {
                     HPCT_LOG(info) << "Hardware timestamping probably not supported on interface "
                                    << interfaceName
                                    << " (SIOCSHWTSTAMP: " << strerror(errno) << ")";
                  }
               }
            }
            else {
               if( (hwTimestampConfig.tx_type   == hwTimestampConfigDesired.tx_type) &&
                   (hwTimestampConfig.rx_filter == hwTimestampConfigDesired.rx_filter) ) {
                  if(logSIOCSHWTSTAMP) {
                     HPCT_LOG(info) << "Hardware timestamping enabled on interface " << interfaceName;
                  }
               }
            }
         }
         logSIOCSHWTSTAMP = false;
      }
      else {
          HPCT_LOG(error) << "getifaddrs() failed: " << strerror(errno);
          return false;
      }
   }
#endif
   return true;
}


std::map<boost::asio::ip::address, boost::asio::ip::address> IOModuleBase::SourceForDestinationMap;
std::mutex                                                   IOModuleBase::SourceForDestinationMapMutex;

// ###### Find source address for given destination address #################
boost::asio::ip::address IOModuleBase::findSourceForDestination(const boost::asio::ip::address& destinationAddress)
{
   std::lock_guard<std::mutex> lock(SourceForDestinationMapMutex);

   // ====== Cache lookup ===================================================
   std::map<boost::asio::ip::address, boost::asio::ip::address>::const_iterator found =
      SourceForDestinationMap.find(destinationAddress);
   if(found != SourceForDestinationMap.end()) {
      return found->second;
   }

   // ====== Get source address from kernel =================================
   // Procedure:
   // - Create UDP socket
   // - Connect it to remote address
   // - Obtain local address
   // - Write this information into a cache for later lookup
   try {
      boost::asio::io_service        ioService;
      boost::asio::ip::udp::endpoint destinationEndpoint(destinationAddress, 7);
      boost::asio::ip::udp::socket   udpSpcket(ioService, (destinationAddress.is_v6() == true) ?
                                                             boost::asio::ip::udp::v6() :
                                                             boost::asio::ip::udp::v4());
      udpSpcket.connect(destinationEndpoint);
      const boost::asio::ip::address sourceAddress = udpSpcket.local_endpoint().address();
      SourceForDestinationMap.insert(std::pair<boost::asio::ip::address, boost::asio::ip::address>(destinationAddress,
                                                                                                   sourceAddress));
      return sourceAddress;
   }
   catch(...) {
      return boost::asio::ip::address();
   }
}


// ###### Record result from response message ###############################
void IOModuleBase::recordResult(const ReceivedData&  receivedData,
                                const uint8_t        icmpType,
                                const uint8_t        icmpCode,
                                const unsigned short seqNumber)
{
   // ====== Find corresponding request =====================================
   std::map<unsigned short, ResultEntry*>::iterator found = ResultsMap.find(seqNumber);
   if(found == ResultsMap.end()) {
      return;
   }
   ResultEntry* resultEntry = found->second;

   // ====== Checks =========================================================
   if( ( (!receivedData.Source.address().is_unspecified())      && (!resultEntry->sourceAddress().is_unspecified())      && (receivedData.Source.address() != resultEntry->sourceAddress()) )  ||
       ( (!receivedData.Destination.address().is_unspecified()) && (!resultEntry->destinationAddress().is_unspecified()) && (receivedData.Destination.address() != resultEntry->destinationAddress()) ) ) {
      HPCT_LOG(warning) << "Mapping mismatch: "
        << " ResultEntry: "  << resultEntry->sourceAddress()  << " -> " << resultEntry->destinationAddress()
        << " ReceivedData: " << receivedData.Source.address() << " -> " << receivedData.Destination.address()
        << " T=" << (unsigned int)icmpType << " C=" << (unsigned int)icmpCode;
      return;
   }

   // ====== Get status =====================================================
   if(resultEntry->status() == Unknown) {
      // Just set address, keep traffic class and identifier settings:
      resultEntry->setDestinationAddress(receivedData.ReplyEndpoint.address());

      // Set receive time stamps:
      resultEntry->setReceiveTime(RXTimeStampType::RXTST_Application,
                                  TimeSourceType::TST_SysClock,
                                  receivedData.ApplicationReceiveTime);
      resultEntry->setReceiveTime(RXTimeStampType::RXTST_ReceptionSW,
                                  receivedData.ReceiveSWSource,
                                  receivedData.ReceiveSWTime);
      resultEntry->setReceiveTime(RXTimeStampType::RXTST_ReceptionHW,
                                  receivedData.ReceiveHWSource,
                                  receivedData.ReceiveHWTime);

      // Set ICMP error status:
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

      NewResultCallback(resultEntry);
   }
}


// ###### Register IO module ################################################
bool IOModuleBase::registerIOModule(
   const ProtocolType  moduleType,
   const std::string&  moduleName,
   IOModuleBase*       (*createIOModuleFunction)(
      boost::asio::io_service&                 ioService,
      std::map<unsigned short, ResultEntry*>&  resultsMap,
      const boost::asio::ip::address&          sourceAddress,
      std::function<void (const ResultEntry*)> newResultCallback,
      const unsigned int                       packetSize,
      const uint16_t                           destinationPort))
{
   if(IOModuleList == nullptr) {
      IOModuleList = new std::list<RegisteredIOModule*>;
      assert(IOModuleList != nullptr);
   }
   RegisteredIOModule* registeredIOModule = new RegisteredIOModule;
   registeredIOModule->Type                   = moduleType;
   registeredIOModule->Name                   = moduleName;
   registeredIOModule->CreateIOModuleFunction = createIOModuleFunction;
   IOModuleList->push_back(registeredIOModule);
   return true;
}


// ###### Create new IO module ##############################################
IOModuleBase* IOModuleBase::createIOModule(const std::string&                       moduleName,
                                           boost::asio::io_service&                 ioService,
                                           std::map<unsigned short, ResultEntry*>&  resultsMap,
                                           const boost::asio::ip::address&          sourceAddress,
                                           std::function<void (const ResultEntry*)> newResultCallback,
                                           const unsigned int                       packetSize,
                                           const uint16_t                           destinationPort)
{
   for(RegisteredIOModule* registeredIOModule : *IOModuleList) {
      if(registeredIOModule->Name == moduleName) {
         return registeredIOModule->CreateIOModuleFunction(
                   ioService, resultsMap, sourceAddress,
                   newResultCallback, packetSize, destinationPort);
      }
   }
   return nullptr;
}


// ###### Check existence of IO module ######################################
bool IOModuleBase::checkIOModule(const std::string& moduleName)
{
   for(RegisteredIOModule* registeredIOModule : *IOModuleList) {
      if(registeredIOModule->Name == moduleName) {
         return true;
      }
   }
   return false;
}




#define VERIFY_ICMP_CHECKSUM   // Verify ICMP checksum computation (FIXME)

REGISTER_IOMODULE(ProtocolType::PT_ICMP, "ICMP", ICMPModule);


// ###### Constructor #######################################################
ICMPModule::ICMPModule(boost::asio::io_service&                 ioService,
                       std::map<unsigned short, ResultEntry*>&  resultsMap,
                       const boost::asio::ip::address&          sourceAddress,
                       std::function<void (const ResultEntry*)> newResultCallback,
                       const unsigned int                       packetSize,
                       const uint16_t                           destinationPort)
   : IOModuleBase(ioService, resultsMap, sourceAddress,
                  newResultCallback),
     UDPSocket(IOService, (sourceAddress.is_v6() == true) ? boost::asio::ip::udp::v6() :
                                                            boost::asio::ip::udp::v4() ),
     ICMPSocket(IOService, (sourceAddress.is_v6() == true) ? boost::asio::ip::icmp::v6() :
                                                             boost::asio::ip::icmp::v4() )
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
   boost::asio::ip::udp::endpoint sourceEndpoint(SourceAddress, 0);
   UDPSocket.bind(sourceEndpoint, errorCode);
   if(errorCode !=  boost::system::errc::success) {
      HPCT_LOG(error) << getName() << ": Unable to bind UDP socket to source address "
                      << sourceEndpoint << "!";
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
      assert(in->sa_family == AF_INET6);
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
#warning No ICMP_FILTER!
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
         assert(ExpectingError == false);
         ICMPSocket.async_wait(
            boost::asio::ip::icmp::socket::wait_error,
            std::bind(&ICMPModule::handleResponse, this,
                     std::placeholders::_1, ICMPSocket.native_handle(), true)
         );
         ExpectingError = true;
      }
      else {
         assert(ExpectingReply == false);
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
      ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
   echoRequest.code(0);
   echoRequest.identifier(Identifier);

   // ====== Message scatter/gather array ===================================
   std::array<boost::asio::const_buffer, 2> buffer = {
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
   // BEGIN OF TIMING-CRITICAL PART
   assert(fromRound <= toRound);
   assert(fromTTL >= toTTL);
   unsigned int messagesSent = 0;
   int currentTTL            = -1;
   for(unsigned int round = fromRound; round <= toRound; round++) {
      for(int ttl = (int)fromTTL; ttl >= (int)toTTL; ttl--) {
         assert(currentEntry < entries);

         // ====== Set TTL ==================================================
         if(ttl != currentTTL) {
            // Only need to set option again if it differs from current TTL!
            const boost::asio::ip::unicast::hops hopsOption(ttl);
            ICMPSocket.set_option(hopsOption, errorCodeArray[currentEntry]);
            currentTTL = ttl;
         }

         // ====== Update ICMP header =======================================
         uint32_t icmpChecksum = 0;
         echoRequest.seqNumber(++seqNumber);
         echoRequest.checksum(0);   // Reset the original checksum first!
         echoRequest.computeInternet16(icmpChecksum);

         // ====== Update TraceService header ===============================
         tsHeader.sendTTL(ttl);
         tsHeader.round((unsigned char)round);
         tsHeader.checksumTweak(0);
         const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();
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
            const uint16_t originalChecksum = echoRequest.checksum();

            // Compute value to tweak checksum to target value
            uint16_t diff = 0xffff - (targetChecksumArray[round] - originalChecksum);
            if(originalChecksum > targetChecksumArray[round]) {    // Handle necessary sum wrap!
               diff++;
            }
            tsHeader.checksumTweak(diff);

#ifdef VERIFY_ICMP_CHECKSUM
#warning VERIFY_ICMP_CHECKSUM is on!
            // Compute new checksum (must be equal to target checksum!)
            icmpChecksum = 0;
            echoRequest.checksum(0);   // Reset the original checksum first!
            echoRequest.computeInternet16(icmpChecksum);
            tsHeader.computeInternet16(icmpChecksum);
            echoRequest.checksum(finishInternet16(icmpChecksum));
            assert(echoRequest.checksum() == targetChecksumArray[round]);
#endif
         }
         assert((targetChecksumArray[round] & ~0xffff) == 0);

         // ====== Send the request =========================================
         sentArray[currentEntry] =
            ICMPSocket.send_to(buffer, remoteEndpoint, 0, errorCodeArray[currentEntry]);

         // ====== Store message information ================================
         if( (!errorCodeArray[currentEntry]) && (sentArray[currentEntry] > 0) ) {
            resultEntryArray[currentEntry]->initialise(
               TimeStampSeqID++,
               round, seqNumber, ttl, ActualPacketSize,
               (uint16_t)targetChecksumArray[round], sendTime,
               SourceAddress, destination, Unknown
            );
            messagesSent++;
         }

         currentEntry++;
      }
   }
   assert(currentEntry == entries);
   // END OF TIMING-CRITICAL PART

   // ====== Check results ==================================================
   for(unsigned int i = 0; i < entries; i++) {
      if( (!errorCodeArray[i]) && (sentArray[i] > 0) ) {
         std::pair<std::map<unsigned short, ResultEntry*>::iterator, bool> result =
            ResultsMap.insert(std::pair<unsigned short, ResultEntry*>(
                                 resultEntryArray[i]->seqNumber(),
                                 resultEntryArray[i]));
         assert(result.second == true);
      }
      else {
         HPCT_LOG(warning) << getName() << ": sendRequest() - send_to("
                           << SourceAddress << "->" << destination << ") failed: "
                           << errorCodeArray[i].message();
         delete resultEntryArray[i];
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

            const ssize_t length =
               recvmsg(socketDescriptor, &msg,
                       (readFromErrorQueue == true) ? MSG_ERRQUEUE|MSG_DONTWAIT : MSG_DONTWAIT);
            // NOTE: length == 0 for control data without user data!
            if(length < 0) {
               break;
            }


            // ====== Handle control data ===================================
            ReceivedData receivedData;
            receivedData.ReplyEndpoint          = boost::asio::ip::udp::endpoint();
            receivedData.ApplicationReceiveTime = std::chrono::high_resolution_clock::now();
            receivedData.ReceiveSWSource        = TimeSourceType::TST_Unknown;
            receivedData.ReceiveSWTime          = std::chrono::high_resolution_clock::time_point();
            receivedData.ReceiveHWSource        = TimeSourceType::TST_Unknown;
            receivedData.ReceiveHWTime          = std::chrono::high_resolution_clock::time_point();
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
                        receivedData.ReceiveHWTime   = std::chrono::system_clock::time_point(
                                                          std::chrono::seconds(socketTimestamp->ts[2].tv_sec) +
                                                          std::chrono::nanoseconds(socketTimestamp->ts[2].tv_nsec));
                     }
                     if(socketTimestamp->ts[0].tv_sec != 0) {
                        // Software timestamp (system clock):
                        receivedData.ReceiveSWSource = TimeSourceType::TST_TIMESTAMPING_SW;
                        receivedData.ReceiveSWTime   = std::chrono::system_clock::time_point(
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
                     receivedData.ReceiveSWTime   = std::chrono::system_clock::time_point(
                                                       std::chrono::seconds(ts->tv_sec) +
                                                       std::chrono::nanoseconds(ts->tv_nsec));
                  }
#endif
                  else if(cmsg->cmsg_type == SO_TIMESTAMP) {
                     const timeval* tv = (const timeval*)CMSG_DATA(cmsg);
                     receivedData.ReceiveSWSource = TimeSourceType::TST_TIMESTAMP;
                     receivedData.ReceiveSWTime   = std::chrono::system_clock::time_point(
                                                       std::chrono::seconds(tv->tv_sec) +
                                                       std::chrono::microseconds(tv->tv_usec));
                  }
               }
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
            }

            // ====== TX Timestamping information via error queue ===========
            if( (readFromErrorQueue) && (socketTXTimestamping != nullptr) ) {
               if(socketTimestamp != nullptr) {
                  updateSendTimeInResultEntry(socketTXTimestamping, socketTimestamp);
               }
               // This is just the timestamp -> nothing more to do here!
               continue;
            }

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
                  receivedData.ReceiveSWTime   = std::chrono::system_clock::time_point(
                                                    std::chrono::seconds(ts.tv_sec) +
                                                    std::chrono::nanoseconds(ts.tv_nsec));
               }
               // ------ Linux: get reception time via SIOCGSTAMP -----------
               else if(ioctl(socketDescriptor, SIOCGSTAMP, &tv) == 0) {
                  // Got reception time from kernel via SIOCGSTAMP
                  receivedData.ReceiveSWSource = TimeSourceType::TST_SIOCGSTAMP;
                  receivedData.ReceiveSWTime   = std::chrono::system_clock::time_point(
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
   for(std::map<unsigned short, ResultEntry*>::iterator iterator = ResultsMap.begin();
       iterator != ResultsMap.end(); iterator++) {
      ResultEntry* resultsEntry = iterator->second;
      if(resultsEntry->timeStampSeqID() == socketError->ee_data) {
         int                                   txTimeStampType = -1;
         int                                   txTimeSource    = -1;
         std::chrono::system_clock::time_point txTimePoint;
         if(socketTimestamp->ts[2].tv_sec != 0) {
            // Hardware timestamp (raw):
            txTimeSource = TimeSourceType::TST_TIMESTAMPING_HW;
            txTimePoint  = std::chrono::system_clock::time_point(
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
            txTimePoint  = std::chrono::system_clock::time_point(
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
         if( (icmpHeader.type() == ICMPHeader::IPv6EchoReply) &&
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
                               icmpHeader.seqNumber());
               }
            }
         }

         // ------ IPv6 -> ICMPv6[Error] ------------------------------------
         else if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
                  (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ) {
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
                            innerICMPHeader.seqNumber());
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
            if( (icmpHeader.type() == ICMPHeader::IPv4EchoReply) &&
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
                               icmpHeader.seqNumber());
               }
            }

            // ------ IPv4 -> ICMP[Error] -----------------------------------
            else if( (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ||
                     (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
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
                               innerICMPHeader.seqNumber());
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
   int on = 1;
   if(setsockopt(RawUDPSocket.native_handle(),
                 (SourceAddress.is_v6() == true) ? IPPROTO_IPV6 : IPPROTO_IP,
                 (SourceAddress.is_v6() == true) ? IPV6_HDRINCL : IP_HDRINCL,
                 &on, sizeof(on)) < 0) {
      HPCT_LOG(error) << "Unable to enable IP_HDRINCL/IPV6_HDRINCL option on socket: "
                        << strerror(errno);
      return false;
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
   const raw_udp::endpoint remoteEndpoint(destination.address(), DestinationPort);
   const raw_udp::endpoint localEndpoint((UDPSocketEndpoint.address().is_unspecified() ?
                                            findSourceForDestination(destination.address()) :
                                            UDPSocketEndpoint.address()),
                                         UDPSocketEndpoint.port());

   assert(fromRound <= toRound);
   assert(fromTTL >= toTTL);
   unsigned int messagesSent = 0;
   for(unsigned int round = fromRound; round <= toRound; round++) {
      for(int ttl = (int)fromTTL; ttl >= (int)toTTL; ttl--) {

         // ====== Create UDP header ========================================
         UDPHeader udpHeader;
         udpHeader.sourcePort(localEndpoint.port());
         udpHeader.destinationPort(remoteEndpoint.port());
         udpHeader.length(8 + PayloadSize);

         // ====== Create TraceServiceHeader ================================
         seqNumber++;
         TraceServiceHeader tsHeader(PayloadSize);
         tsHeader.magicNumber(MagicNumber);
         tsHeader.sendTTL(ttl);
         tsHeader.round((unsigned char)round);
         tsHeader.seqNumber(seqNumber);
         // NOTE: SendTimeStamp will be set later, for accuracy!

         // ====== Create IPv6 header =======================================
         boost::system::error_code             errorCode;
         std::chrono::system_clock::time_point sendTime;
         std::size_t                           sent;
         if(SourceAddress.is_v6()) {
            IPv6Header ipv6Header;

            ipv6Header.version(6);
            ipv6Header.trafficClass(destination.trafficClass());
            ipv6Header.flowLabel(0);
            ipv6Header.payloadLength(8 + PayloadSize);
            ipv6Header.nextHeader(IPPROTO_UDP);
            ipv6Header.hopLimit(ttl);
            ipv6Header.sourceAddress(localEndpoint.address().to_v6());
            ipv6Header.destinationAddress(remoteEndpoint.address().to_v6());

            IPv6PseudoHeader pseudoHeader(ipv6Header, udpHeader.length());

            // ------ UDP checksum ------------------------------------------
            // UDP pseudo-header:
            uint32_t udpChecksum = 0;
            udpHeader.computeInternet16(udpChecksum);
            pseudoHeader.computeInternet16(udpChecksum);

            // UDP payload:
            // Now, the SendTimeStamp in the TraceServiceHeader has to be set:
            sendTime = std::chrono::system_clock::now();
            tsHeader.sendTimeStamp(sendTime);
            tsHeader.computeInternet16(udpChecksum);

            udpHeader.checksum(finishInternet16(udpChecksum));

            // ------ Send IPv6/UDP/TraceServiceHeader packet ---------------
            const std::array<boost::asio::const_buffer, 3> buffer = {
               boost::asio::buffer(ipv6Header.data(), ipv6Header.size()),
               boost::asio::buffer(udpHeader.data(), udpHeader.size()),
               boost::asio::buffer(tsHeader.data(), tsHeader.size())
            };
            sent = RawUDPSocket.send_to(buffer, raw_udp::endpoint(remoteEndpoint.address(), 0),
                                        0, errorCode);
         }

         // ====== Create IPv4 header =======================================
         else {
            IPv4Header ipv4Header;
            ipv4Header.version(4);
            ipv4Header.typeOfService(destination.trafficClass());
            ipv4Header.headerLength(20);
            ipv4Header.totalLength(ActualPacketSize);
            // NOTE: Using IPv4 Identification for the sequence number!
            ipv4Header.identification(seqNumber);
            ipv4Header.fragmentOffset(0);
            ipv4Header.protocol(IPPROTO_UDP);
            ipv4Header.timeToLive(ttl);
            ipv4Header.sourceAddress(localEndpoint.address().to_v4());
            ipv4Header.destinationAddress(remoteEndpoint.address().to_v4());

            IPv4PseudoHeader pseudoHeader(ipv4Header, udpHeader.length());

            // ------ IPv4 header checksum ----------------------------------
            uint32_t ipv4HeaderChecksum = 0;
            ipv4Header.computeInternet16(ipv4HeaderChecksum);
            ipv4Header.headerChecksum(finishInternet16(ipv4HeaderChecksum));

            // ------ UDP checksum ------------------------------------------
            // UDP pseudo-header:
            uint32_t udpChecksum = 0;
            udpHeader.computeInternet16(udpChecksum);
            pseudoHeader.computeInternet16(udpChecksum);

            // UDP payload:
            // Now, the SendTimeStamp in the TraceServiceHeader has to be set:
            sendTime = std::chrono::system_clock::now();
            tsHeader.sendTimeStamp(sendTime);
            tsHeader.computeInternet16(udpChecksum);

            udpHeader.checksum(finishInternet16(udpChecksum));

            const std::array<boost::asio::const_buffer, 3> buffer = {
               boost::asio::buffer(ipv4Header.data(), ipv4Header.size()),
               boost::asio::buffer(udpHeader.data(), udpHeader.size()),
               boost::asio::buffer(tsHeader.data(), tsHeader.size())
            };
            sent = RawUDPSocket.send_to(buffer, remoteEndpoint, 0, errorCode);


            /* FIXME */
            if(errorCode) {
             puts("RETRY-1");
             sent = RawUDPSocket.send_to(buffer, remoteEndpoint, 0, errorCode);
            }
            if(errorCode) {
             puts("RETRY-2");
             sent = RawUDPSocket.send_to(buffer, remoteEndpoint, 0, errorCode);
            }
            if(errorCode) {
             puts("RETRY-3");
             sent = RawUDPSocket.send_to(buffer, remoteEndpoint, 0, errorCode);
            }
            /* FIXME */
         }

         // ====== Create ResultEntry on success ============================
         if( (!errorCode) && (sent > 0) ) {
            ResultEntry* resultEntry = new ResultEntry;
            resultEntry->initialise(TimeStampSeqID++,
                                    round, seqNumber, ttl, ActualPacketSize,
                                    (uint16_t)targetChecksumArray[round], sendTime,
                                    localEndpoint.address(), destination, Unknown);
            assert(resultEntry != nullptr);

            std::pair<std::map<unsigned short, ResultEntry*>::iterator, bool> result =
               ResultsMap.insert(std::pair<unsigned short, ResultEntry*>(seqNumber, resultEntry));
            assert(result.second == true);

            messagesSent++;
         }
         else {
            HPCT_LOG(warning) << getName() << ": sendRequest() - send_to("
                              << SourceAddress << "->" << destination
                              << ", TTL " << ttl << ") failed: "
                              << errorCode.message();
         }
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
         recordResult(receivedData, 0, 0, tsHeader.seqNumber());
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
void UDPModule::handleErrorResponse(const int          socketDescriptor,
                                    ReceivedData&      receivedData,
                                    sock_extended_err* socketError)
{
   // Nothing to do here!
}
