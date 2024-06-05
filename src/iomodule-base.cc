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
// Copyright (C) 2015-2024 by Thomas Dreibholz
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

#include "iomodule-base.h"
#include "tools.h"
#include "logger.h"
#include "icmpheader.h"

#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __linux__
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#endif


//  ###### IO Module Registry ###############################################

#include "iomodule-icmp.h"
#include "iomodule-udp.h"

REGISTER_IOMODULE(ProtocolType::PT_ICMP, "ICMP", ICMPModule);
REGISTER_IOMODULE(ProtocolType::PT_UDP,  "UDP",   UDPModule);

//  #########################################################################


std::list<IOModuleBase::RegisteredIOModule*>* IOModuleBase::IOModuleList = nullptr;


// ###### Constructor #######################################################
IOModuleBase::IOModuleBase(boost::asio::io_service&                 ioService,
                           std::map<unsigned short, ResultEntry*>&  resultsMap,
                           const boost::asio::ip::address&          sourceAddress,
                           const uint16_t                           sourcePort,
                           const uint16_t                           destinationPort,
                           std::function<void (const ResultEntry*)> newResultCallback)
   : IOService(ioService),
     ResultsMap(resultsMap),
     SourceAddress(sourceAddress),
     SourcePort(sourcePort),
     DestinationPort(destinationPort),
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
#if defined (IP_RECVERR) && defined (IPV6_RECVERR)
   if(setsockopt(socketDescriptor,
                 (sourceAddress.is_v6() == true) ? SOL_IPV6: SOL_IP,
                 (sourceAddress.is_v6() == true) ? IPV6_RECVERR : IP_RECVERR,
                 &on, sizeof(on)) < 0) {
      HPCT_LOG(error) << "Unable to enable IP_RECVERR/IPV6_RECVERR option on socket";
      return false;
   }
#else
#warning No IP_RECVERR/IPV6_RECVERR!
#endif

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
         const int tdClockType = SO_TS_REALTIME;
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
                                const unsigned short seqNumber,
                                const unsigned int   responseLength)
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
      resultEntry->setResponseSize(responseLength);

      // Just set address, keep traffic class and identifier settings:
      resultEntry->setHopAddress(receivedData.ReplyEndpoint.address());

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
         status = Success;
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
      const uint16_t                           sourcePort,
      const uint16_t                           destinationPort,
      std::function<void (const ResultEntry*)> newResultCallback,
      const unsigned int                       packetSize))
{
   if(IOModuleList == nullptr) {
      IOModuleList = new std::list<RegisteredIOModule*>;
      assert(IOModuleList != nullptr);
   }
   printf("REG: <%s>\n", moduleName.c_str());
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
                                           const uint16_t                           sourcePort,
                                           const uint16_t                           destinationPort,
                                           std::function<void (const ResultEntry*)> newResultCallback,
                                           const unsigned int                       packetSize)
{
   for(RegisteredIOModule* registeredIOModule : *IOModuleList) {
      if(registeredIOModule->Name == moduleName) {
         return registeredIOModule->CreateIOModuleFunction(
                   ioService, resultsMap, sourceAddress, sourcePort, destinationPort,
                   newResultCallback,
                   packetSize);
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
