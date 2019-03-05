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
// Copyright (C) 2015-2019 by Thomas Dreibholz
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

#include <iostream>

#include "tools.h"
#include "logger.h"
#include "service.h"
#include "traceroute.h"
#include "ping.h"
#include "resultswriter.h"
#include "icmpheader.h"


struct TargetInfo
{
   std::chrono::steady_clock::time_point LastSeen;
   unsigned int                          TriggerCounter;
};

static std::set<boost::asio::ip::address>                   SourceArray;
static std::set<boost::asio::ip::address>                   DestinationArray;
static std::map<boost::asio::ip::address, TargetInfo*>      TargetMap;
static std::set<ResultsWriter*>                             ResultsWriterSet;
static std::set<Service*>                                   ServiceSet;
static boost::asio::io_service                              IOService;
static boost::asio::basic_raw_socket<boost::asio::ip::icmp> SnifferSocketV4(IOService);
static boost::asio::basic_raw_socket<boost::asio::ip::icmp> SnifferSocketV6(IOService);
static boost::asio::ip::icmp::endpoint                      IncomingPingSource;
static char                                                 IncomingPingMessageBuffer[4096];
static boost::asio::signal_set                              Signals(IOService, SIGINT, SIGTERM);
static boost::posix_time::milliseconds                      CleanupTimerInterval(1000);
static boost::asio::deadline_timer                          CleanupTimer(IOService, CleanupTimerInterval);

static unsigned int                                         PingsBeforeQueuing = 3;
static unsigned int                                         PingTriggerLength  = 53;


// ###### Add address to set ################################################
static void addAddress(std::set<boost::asio::ip::address>& array,
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


// ###### Signal handler ####################################################
static void signalHandler(const boost::system::error_code& error, int signal_number)
{
   if(error != boost::asio::error::operation_aborted) {
      puts("\n*** Shutting down! ***\n");   // Avoids a false positive from Helgrind.
      IOService.stop();
      for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
         Service* service = *serviceIterator;
         service->requestStop();
      }
   }
 }


// ###### Check whether services can be cleaned up ##########################
static void tryCleanup(const boost::system::error_code& errorCode)
{
   bool finished = true;
   for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      if(!service->joinable()) {
         finished = false;
         break;
      }
   }

   if(!finished) {
      CleanupTimer.expires_at(CleanupTimer.expires_at() + CleanupTimerInterval);
      CleanupTimer.async_wait(tryCleanup);

      const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
      std::map<boost::asio::ip::address, TargetInfo*>::iterator iterator = TargetMap.begin();
      while(iterator != TargetMap.end()) {
         std::map<boost::asio::ip::address, TargetInfo*>::iterator current = iterator;
         iterator++;
         TargetInfo* targetInfo = current->second;
         if(now - targetInfo->LastSeen >= std::chrono::seconds(30)) {
            TargetMap.erase(current);
            delete targetInfo;
         }
      }
   }
   else {
      SnifferSocketV4.cancel();
      SnifferSocketV6.cancel();
      Signals.cancel();
   }
}


// ###### Handle Ping #######################################################
static void handlePing(const ICMPHeader& header, const std::size_t payloadLength)
{
   HPCT_LOG(debug) << "Ping from " << IncomingPingSource.address()
                   << ", payload " << payloadLength;

   if(payloadLength == PingTriggerLength) {
      std::map<boost::asio::ip::address, TargetInfo*>::iterator found =
         TargetMap.find(IncomingPingSource.address());
      if(found != TargetMap.end()) {
         TargetInfo* targetInfo = found->second;
         targetInfo->TriggerCounter++;
         targetInfo->LastSeen = std::chrono::steady_clock::now();
         if(targetInfo->TriggerCounter >= PingsBeforeQueuing) {
            for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
               Service* service = *serviceIterator;
               if(service->addDestination(IncomingPingSource.address())) {
                   HPCT_LOG(debug) << "Queued " << IncomingPingSource.address()
                                   << " from " << service->getSource();
                   targetInfo->TriggerCounter = 0;
               }
            }
         }
         else {
            HPCT_LOG(debug) << "Triggered: " <<  IncomingPingSource.address()
                            << ", n=" << targetInfo->TriggerCounter;
         }
      }
      else {
         TargetInfo* targetInfo = new TargetInfo;
         if(targetInfo != NULL) {
            targetInfo->TriggerCounter = 0;
            targetInfo->LastSeen       = std::chrono::steady_clock::now();
            TargetMap.insert(std::pair<boost::asio::ip::address, TargetInfo*>(
                                IncomingPingSource.address(), targetInfo));
         }
      }
   }
}


// ###### Decode raw ICMP packet ############################################
static void receivedPingV4(const boost::system::error_code& errorCode, std::size_t length)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      if( (!errorCode) && (length >= sizeof(iphdr)) ) {
         // ====== Decode IPv4 packet =======================================
         // NOTE: raw socket for IPv4 delivers IPv4 header as well!
         const iphdr* ipHeader = (const iphdr*)IncomingPingMessageBuffer;
         if( (ipHeader->version == 4) &&
             (ntohs(ipHeader->tot_len) == length) ) {
             const std::size_t headerLength = ipHeader->ihl << 2;
             // ====== Decode ICMP message ==================================
             if( (headerLength + 8 <= length) &&
                 (ipHeader->protocol == IPPROTO_ICMP) ) {
                 ICMPHeader header((const char*)&IncomingPingMessageBuffer[headerLength],
                                   length - headerLength);
                 if(header.type() == ICMPHeader::IPv4EchoRequest) {
                    handlePing(header, length - headerLength - 8);
                 }
             }
         }
      }
      SnifferSocketV4.async_receive_from(
         boost::asio::buffer(IncomingPingMessageBuffer, sizeof(IncomingPingMessageBuffer)),
                             IncomingPingSource, receivedPingV4);
   }
}


// ###### Decode raw ICMPv6 packet ##########################################
static void receivedPingV6(const boost::system::error_code& errorCode, std::size_t length)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      if( (!errorCode) && (length >= 8) ) {
         // ====== Decode ICMPv6 message ====================================
         // NOTE: raw socket for IPv6 just delivery the IPv6 payload!
         if(length >= 8) {
            ICMPHeader header((const char*)&IncomingPingMessageBuffer, length);
            if(header.type() == ICMPHeader::IPv6EchoRequest) {
               handlePing(header, length - 8);
            }
         }
      }
      SnifferSocketV6.async_receive_from(
         boost::asio::buffer(IncomingPingMessageBuffer, sizeof(IncomingPingMessageBuffer)),
                             IncomingPingSource, receivedPingV6);
   }
}



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   unsigned int       logLevel                  = boost::log::trivial::severity_level::info;
   const char*        user                      = NULL;
   bool               servicePing               = false;
   bool               serviceTraceroute         = false;

   unsigned long long tracerouteInterval        = 10000;
   unsigned int       tracerouteExpiration      = 3000;
   unsigned int       tracerouteRounds          = 1;
   unsigned int       tracerouteInitialMaxTTL   = 6;
   unsigned int       tracerouteFinalMaxTTL     = 36;
   unsigned int       tracerouteIncrementMaxTTL = 6;

   unsigned long long pingInterval              = 1000;
   unsigned int       pingExpiration            = 30000;
   unsigned int       pingTTL                   = 64;

   unsigned int       resultsTransactionLength  = 60;
   std::string        resultsDirectory;

   // ====== Handle arguments ===============================================
   for(int i = 1; i < argc; i++) {
      if(strncmp(argv[i], "-source=", 8) == 0) {
         addAddress(SourceArray, (const char*)&argv[i][8]);
      }
      else if(strncmp(argv[i], "-destination=", 13) == 0) {
         addAddress(DestinationArray, (const char*)&argv[i][13]);
      }
      else if(strcmp(argv[i], "-ping") == 0) {
         servicePing = true;
      }
      else if(strcmp(argv[i], "-traceroute") == 0) {
         serviceTraceroute = true;
      }
      else if(strncmp(argv[i], "-user=", 6) == 0) {
         user = (const char*)&argv[i][6];
      }
      else if(strncmp(argv[i], "-loglevel=", 10) == 0) {
         logLevel = std::strtoul((const char*)&argv[i][10], NULL, 10);
      }
      else if(strcmp(argv[i], "-quiet") == 0) {
         logLevel = boost::log::trivial::severity_level::warning;
      }
      else if(strcmp(argv[i], "-verbose") == 0) {
         logLevel = boost::log::trivial::severity_level::trace;
      }
      else if(strncmp(argv[i], "-user=", 6) == 0) {
         user = (const char*)&argv[i][6];
      }
      else if(strncmp(argv[i], "-tracerouteinterval=", 20) == 0) {
         tracerouteInterval = std::strtoul((const char*)&argv[i][20], NULL, 10);
      }
      else if(strncmp(argv[i], "-tracerouteduration=", 20) == 0) {
         tracerouteExpiration = std::strtoul((const char*)&argv[i][20], NULL, 10);
      }
      else if(strncmp(argv[i], "-tracerouterounds=", 18) == 0) {
         tracerouteRounds = std::strtoul((const char*)&argv[i][18], NULL, 10);
      }
      else if(strncmp(argv[i], "-tracerouteinitialmaxttl=", 25) == 0) {
         tracerouteInitialMaxTTL = std::strtoul((const char*)&argv[i][25], NULL, 10);
      }
      else if(strncmp(argv[i], "-traceroutefinalmaxttl=", 23) == 0) {
         tracerouteFinalMaxTTL = std::strtoul((const char*)&argv[i][23], NULL, 10);
      }
      else if(strncmp(argv[i], "-tracerouteincrementmaxttl=", 27) == 0) {
         tracerouteIncrementMaxTTL = std::strtoul((const char*)&argv[i][27], NULL, 10);
      }
      else if(strncmp(argv[i], "-pinginterval=", 14) == 0) {
         pingInterval = std::strtoul((const char*)&argv[i][14], NULL, 10);
      }
      else if(strncmp(argv[i], "-pingexpiration=", 16) == 0) {
         pingExpiration = std::strtoul((const char*)&argv[i][16], NULL, 10);
      }
      else if(strncmp(argv[i], "-pingttl=", 9) == 0) {
         pingTTL = std::strtoul((const char*)&argv[i][9], NULL, 10);
      }
      else if(strncmp(argv[i], "-resultsdirectory=", 18) == 0) {
         resultsDirectory = (const char*)&argv[i][18];
      }
      else if(strncmp(argv[i], "-resultstransactionlength=", 26) == 0) {
         resultsTransactionLength = std::strtoul((const char*)&argv[i][26], NULL, 10);
      }
      else if(strncmp(argv[i], "-pingsbeforequeuing=", 20) == 0) {
         PingsBeforeQueuing = std::strtoul((const char*)&argv[i][20], NULL, 10);
      }
      else if(strncmp(argv[i], "-pingtriggerlength=", 19) == 0) {
         PingTriggerLength = std::strtoul((const char*)&argv[i][19], NULL, 10);
      }
      else if(strcmp(argv[i], "--") == 0) {
      }
      else {
         std::cerr << "ERROR: Unknown parameter " << argv[i] << std::endl
                   << "Usage: " << argv[0] << " ..." << std::endl;
         return(1);
      }
   }


   // ====== Initialize =====================================================
   initialiseLogger(logLevel);
   const passwd* pw = getUser(user);
   if(SourceArray.size() < 1) {
      HPCT_LOG(fatal) << "ERROR: At least one source is needed!";
      ::exit(1);
   }
   if((servicePing == false) && (serviceTraceroute == false)) {
      HPCT_LOG(fatal) << "ERROR: Enable at least on service (Ping or Traceroute)!";
      ::exit(1);
   }

   std::srand(std::time(0));
   tracerouteInterval        = std::min(std::max(1000ULL, tracerouteInterval),   3600U*60000ULL);
   tracerouteExpiration      = std::min(std::max(1000U, tracerouteExpiration),   60000U);
   tracerouteInitialMaxTTL   = std::min(std::max(1U, tracerouteInitialMaxTTL),   255U);
   tracerouteFinalMaxTTL     = std::min(std::max(1U, tracerouteFinalMaxTTL),     255U);
   tracerouteIncrementMaxTTL = std::min(std::max(1U, tracerouteIncrementMaxTTL), 255U);
   pingInterval              = std::min(std::max(100ULL, pingInterval),          3600U*60000ULL);
   pingExpiration            = std::min(std::max(100U, pingExpiration),          3600U*60000U);
   pingTTL                   = std::min(std::max(1U, pingTTL),                   255U);

   HPCT_LOG(info) << "Results Output:";
   if(!resultsDirectory.empty()) {
      HPCT_LOG(info) << "* Results Directory  = " << resultsDirectory         << std::endl
                     << "* Transaction Length = " << resultsTransactionLength << " s";
   }
   else {
      HPCT_LOG(info) << "-- turned off--" << std::endl;
   }
   if(servicePing) {
      HPCT_LOG(info) << "Ping Service:" << std:: endl
                     << "* Interval           = " << pingInterval   << " ms" << std::endl
                     << "* Expiration         = " << pingExpiration << " ms" << std::endl
                     << "* TTL                = " << pingTTL;
   }
   if(serviceTraceroute) {
      HPCT_LOG(info) << "Traceroute Service:" << std:: endl
                     << "* Interval           = " << tracerouteInterval        << " ms" << std::endl
                     << "* Expiration         = " << tracerouteExpiration      << " ms" << std::endl
                     << "* Rounds             = " << tracerouteRounds          << std::endl
                     << "* Initial MaxTTL     = " << tracerouteInitialMaxTTL   << std::endl
                     << "* Final MaxTTL       = " << tracerouteFinalMaxTTL     << std::endl
                     << "* Increment MaxTTL   = " << tracerouteIncrementMaxTTL;
   }


   // ====== Start service threads ==========================================
   for(std::set<boost::asio::ip::address>::iterator sourceIterator = SourceArray.begin(); sourceIterator != SourceArray.end(); sourceIterator++) {
      if(servicePing) {
         try {
            Service* service = new Ping(ResultsWriter::makeResultsWriter(
                                           ResultsWriterSet,
                                           *sourceIterator, "Ping", resultsDirectory, resultsTransactionLength,
                                           (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                        0, true,
                                        *sourceIterator, DestinationArray,
                                        pingInterval, pingExpiration, pingTTL);
            if(service->start() == false) {
               ::exit(1);
            }
            ServiceSet.insert(service);
         }
         catch (std::exception& e) {
            HPCT_LOG(fatal) << "ERROR: Cannot create Ping service - " << e.what();
            ::exit(1);
         }
      }
      if(serviceTraceroute) {
         try {
            Service* service = new Traceroute(ResultsWriter::makeResultsWriter(
                                                 ResultsWriterSet,
                                                 *sourceIterator, "Traceroute", resultsDirectory, resultsTransactionLength,
                                                 (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                              0, true,
                                              *sourceIterator, DestinationArray,
                                              tracerouteInterval, tracerouteExpiration,
                                              tracerouteRounds,
                                              tracerouteInitialMaxTTL, tracerouteFinalMaxTTL,
                                              tracerouteIncrementMaxTTL);
            if(service->start() == false) {
               ::exit(1);
            }
            ServiceSet.insert(service);
         }
         catch (std::exception& e) {
            HPCT_LOG(fatal) << "ERROR: Cannot create Traceroute service - " << e.what();
            ::exit(1);
         }
       }
   }


   // ====== Raw socket =====================================================
   SnifferSocketV4.open(boost::asio::ip::icmp::v4());
   SnifferSocketV6.open(boost::asio::ip::icmp::v6());
   SnifferSocketV4.async_receive_from(boost::asio::buffer(IncomingPingMessageBuffer), IncomingPingSource, receivedPingV4);
   SnifferSocketV6.async_receive_from(boost::asio::buffer(IncomingPingMessageBuffer), IncomingPingSource, receivedPingV6);


   // ====== Reduce permissions =============================================
   reducePermissions(pw);


   // ====== Wait for termination signal ====================================
   Signals.async_wait(signalHandler);
   CleanupTimer.async_wait(tryCleanup);
   IOService.run();


   // ====== Shut down service threads ======================================
   for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      service->join();
      delete service;
   }
   for(std::set<ResultsWriter*>::iterator resultsWriterIterator = ResultsWriterSet.begin(); resultsWriterIterator != ResultsWriterSet.end(); resultsWriterIterator++) {
      delete *resultsWriterIterator;
   }
   std::map<boost::asio::ip::address, TargetInfo*>::iterator iterator = TargetMap.begin();
   while(iterator != TargetMap.end()) {
      delete iterator->second;
      TargetMap.erase(iterator);
      iterator = TargetMap.begin();
   }

   return(0);
}
