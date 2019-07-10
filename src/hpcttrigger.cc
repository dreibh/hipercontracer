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
#include <vector>

#include <boost/program_options.hpp>
#include <boost/asio/ip/address.hpp>

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

static std::map<boost::asio::ip::address, std::set<uint8_t>> SourceArray;
static std::set<boost::asio::ip::address>                    DestinationArray;
static std::map<boost::asio::ip::address, TargetInfo*>       TargetMap;
static std::set<ResultsWriter*>                              ResultsWriterSet;
static std::set<Service*>                                    ServiceSet;
static boost::asio::io_service                               IOService;
static boost::asio::basic_raw_socket<boost::asio::ip::icmp>  SnifferSocketV4(IOService);
static boost::asio::basic_raw_socket<boost::asio::ip::icmp>  SnifferSocketV6(IOService);
static boost::asio::ip::icmp::endpoint                       IncomingPingSource;
static char                                                  IncomingPingMessageBuffer[4096];
static boost::asio::signal_set                               Signals(IOService, SIGINT, SIGTERM);
static boost::posix_time::milliseconds                       CleanupTimerInterval(1000);
static boost::asio::deadline_timer                           CleanupTimer(IOService, CleanupTimerInterval);

static unsigned int                                          PingsBeforeQueuing = 3;
static unsigned int                                          PingTriggerLength  = 53;
static unsigned int                                          PingTriggerAge     = 300;


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
         if(now - targetInfo->LastSeen >= std::chrono::seconds(PingTriggerAge)) {
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
   HPCT_LOG(trace) << "Ping from " << IncomingPingSource.address()
                   << ", payload " << payloadLength;

   if(payloadLength == PingTriggerLength) {
      std::map<boost::asio::ip::address, TargetInfo*>::iterator found =
         TargetMap.find(IncomingPingSource.address());
      if(found != TargetMap.end()) {
         TargetInfo* targetInfo = found->second;
         targetInfo->TriggerCounter++;
         targetInfo->LastSeen = std::chrono::steady_clock::now();
         HPCT_LOG(trace) << "Triggered: " <<  IncomingPingSource.address()
                         << ", n=" << targetInfo->TriggerCounter;
         if(targetInfo->TriggerCounter >= PingsBeforeQueuing) {
            for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
               Service* service = *serviceIterator;

               const boost::asio::ip::address sourceAddress = service->getSource();
               assert(SourceArray.find(sourceAddress) != SourceArray.end());
               for(std::set<uint8_t>::iterator trafficClassIterator = SourceArray[sourceAddress].begin();
                   trafficClassIterator != SourceArray[sourceAddress].end(); trafficClassIterator++) {
                  const uint8_t trafficClass = *trafficClassIterator;
                  const AddressWithTrafficClass destination(IncomingPingSource.address(), trafficClass);

                  if(service->addDestination(destination)) {
                      HPCT_LOG(debug) << "Queued " << destination
                                      << " from " << service->getSource();
                      targetInfo->TriggerCounter = 0;
                  }
               }
            }
         }
      }
      else {
         TargetInfo* targetInfo = new TargetInfo;
         if(targetInfo != NULL) {
            targetInfo->TriggerCounter = 1;
            targetInfo->LastSeen       = std::chrono::steady_clock::now();
            TargetMap.insert(std::pair<boost::asio::ip::address, TargetInfo*>(
                                IncomingPingSource.address(), targetInfo));
            HPCT_LOG(trace) << "Triggered: " <<  IncomingPingSource.address();
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
   unsigned int       logLevel;
   std::string        user;
   std::string        configurationFileName;
   bool               servicePing;
   bool               serviceTraceroute;

   unsigned long long tracerouteInterval;
   unsigned int       tracerouteExpiration;
   unsigned int       tracerouteRounds;
   unsigned int       tracerouteInitialMaxTTL;
   unsigned int       tracerouteFinalMaxTTL;
   unsigned int       tracerouteIncrementMaxTTL;

   unsigned long long pingInterval;
   unsigned int       pingExpiration;
   unsigned int       pingTTL;

   unsigned int       resultsTransactionLength;
   std::string        resultsDirectory;

   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )

      ( "source,S",
           boost::program_options::value<std::vector<std::string>>(),
           "Source address" )
      ( "destination,D",
           boost::program_options::value<std::vector<std::string>>(),
           "Destination address" )

      ( "loglevel,L",
           boost::program_options::value<unsigned int>(&logLevel)->default_value(boost::log::trivial::severity_level::info),
           "Set logging level" )
      ( "verbose,v",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::trace),
           "Verbose logging level" )
      ( "quiet,q",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::warning),
           "Quiet logging level" )
      ( "user,U",
           boost::program_options::value<std::string>(&user),
           "User" )

      ( "ping,P",
           boost::program_options::value<bool>(&servicePing)->default_value(false)->implicit_value(true),
           "Start Ping service" )
      ( "traceroute,T",
           boost::program_options::value<bool>(&serviceTraceroute)->default_value(false)->implicit_value(true),
           "Start Traceroute service" )

      ( "tracerouteinterval",
           boost::program_options::value<unsigned long long>(&tracerouteInterval)->default_value(10000),
           "Traceroute interval in ms" )
      ( "tracerouteduration",
           boost::program_options::value<unsigned int>(&tracerouteExpiration)->default_value(3000),
           "Traceroute duration in ms" )
      ( "tracerouterounds",
           boost::program_options::value<unsigned int>(&tracerouteRounds)->default_value(1),
           "Traceroute rounds" )
      ( "tracerouteinitialmaxttl",
           boost::program_options::value<unsigned int>(&tracerouteInitialMaxTTL)->default_value(6),
           "Traceroute initial maximum TTL value" )
      ( "traceroutefinalmaxttl",
           boost::program_options::value<unsigned int>(&tracerouteFinalMaxTTL)->default_value(36),
           "Traceroute final maximum TTL value" )
      ( "tracerouteincrementmaxttl",
           boost::program_options::value<unsigned int>(&tracerouteIncrementMaxTTL)->default_value(6),
           "Traceroute increment maximum TTL value" )

      ( "pinginterval",
           boost::program_options::value<unsigned long long>(&pingInterval)->default_value(1000),
           "Ping interval in ms" )
      ( "pingexpiration",
           boost::program_options::value<unsigned int>(&pingExpiration)->default_value(30000),
           "Ping expiration timeout in ms" )
      ( "pingttl",
           boost::program_options::value<unsigned int>(&pingTTL)->default_value(64),
           "Ping initial maximum TTL value" )

      ( "pingsbeforequeuing",
           boost::program_options::value<unsigned int>(&PingsBeforeQueuing)->default_value(3),
           "Pings before queuing" )
      ( "pingtriggerlength",
           boost::program_options::value<unsigned int>(&PingTriggerLength)->default_value(53),
           "Ping trigger length in B" )
      ( "pingtriggerage",
           boost::program_options::value<unsigned int>(&PingTriggerAge)->default_value(300),
           "Ping trigger age in s" )

      ( "resultsdirectory,R",
           boost::program_options::value<std::string>(&resultsDirectory)->default_value(std::string()),
           "Results directory" )
      ( "resultstransactionlength",
           boost::program_options::value<unsigned int>(&resultsTransactionLength)->default_value(60),
           "Results directory in s" )
    ;


   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
                                       style(
                                          boost::program_options::command_line_style::style_t::default_style|
                                          boost::program_options::command_line_style::style_t::allow_long_disguise
                                       ).
                                       options(commandLineOptions).
                                       run(), vm);
      boost::program_options::notify(vm);
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: Bad parameter: " << e.what() << std::endl;
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << std::endl
                 << commandLineOptions;
       return 1;
   }
   if(vm.count("source")) {
      const std::vector<std::string>& sourceAddressVector = vm["source"].as<std::vector<std::string>>();
      for(std::vector<std::string>::const_iterator iterator = sourceAddressVector.begin();
          iterator != sourceAddressVector.end(); iterator++) {
         addSourceAddress(SourceArray, iterator->c_str());
      }
   }
   if(vm.count("destination")) {
      const std::vector<std::string>& destinationAddressVector = vm["destination"].as<std::vector<std::string>>();
      for(std::vector<std::string>::const_iterator iterator = destinationAddressVector.begin();
          iterator != destinationAddressVector.end(); iterator++) {
         addDestinationAddress(DestinationArray, iterator->c_str());
      }
   }


   // ====== Initialize =====================================================
   initialiseLogger(logLevel);
   const passwd* pw = getUser(user.c_str());
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

   if(!resultsDirectory.empty()) {
      HPCT_LOG(info) << "Results Output:" << std::endl
                     << "* Results Directory  = " << resultsDirectory         << std::endl
                     << "* Transaction Length = " << resultsTransactionLength << " s";
   }
   else {
      HPCT_LOG(info) << "Results Output:" << std::endl
                     << "-- turned off--" << std::endl;
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
   HPCT_LOG(info) << "Trigger:" << std::endl
                  << "* Ping Trigger Age     = " << PingTriggerAge << " s" << std::endl
                  << "* Ping Trigger Length  = " << PingTriggerLength      << std::endl
                  << "* Pings before Queuing = " << PingsBeforeQueuing;


   // ====== Start service threads ==========================================
   for(std::map<boost::asio::ip::address, std::set<uint8_t>>::iterator sourceIterator = SourceArray.begin();
      sourceIterator != SourceArray.end(); sourceIterator++) {
      const boost::asio::ip::address& sourceAddress = sourceIterator->first;

      std::set<AddressWithTrafficClass> destinationsForSource;
      for(std::set<boost::asio::ip::address>::iterator destinationIterator = DestinationArray.begin();
          destinationIterator != DestinationArray.end(); destinationIterator++) {
         const boost::asio::ip::address& destinationAddress = *destinationIterator;
         for(std::set<uint8_t>::iterator trafficClassIterator = sourceIterator->second.begin();
             trafficClassIterator != sourceIterator->second.end(); trafficClassIterator++) {
            const uint8_t trafficClass = *trafficClassIterator;
            std::cout << destinationAddress << " " << (unsigned int)trafficClass << std::endl;
            destinationsForSource.insert(AddressWithTrafficClass(destinationAddress, trafficClass));
         }
      }


      for(std::set<AddressWithTrafficClass>::iterator iterator = destinationsForSource.begin();
          iterator != destinationsForSource.end(); iterator++) {
         std::cout << " -> " << *iterator << std::endl;
      }


      if(servicePing) {
         try {
            Service* service = new Ping(ResultsWriter::makeResultsWriter(
                                           ResultsWriterSet, sourceAddress, "TriggeredPing",
                                           resultsDirectory, resultsTransactionLength,
                                           (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                        0, true,
                                        sourceAddress, destinationsForSource,
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
                                                 ResultsWriterSet, sourceAddress, "TriggeredTraceroute",
                                                 resultsDirectory, resultsTransactionLength,
                                                 (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                              0, true,
                                              sourceAddress, destinationsForSource,
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
