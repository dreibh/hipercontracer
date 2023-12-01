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

#include <iostream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/program_options.hpp>

#include "check.h"
#include "icmpheader.h"
#include "jitter.h"
#include "logger.h"
#include "package-version.h"
#include "ping.h"
#include "resultswriter.h"
#include "service.h"
#include "tools.h"
#include "traceroute.h"


struct TargetInfo
{
   std::chrono::steady_clock::time_point LastSeen;
   unsigned int                          TriggerCounter;
};

static const std::string                                     ProgramID = std::string("HPCT-Results-Tool/") + HPCT_VERSION;
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
                  const DestinationInfo destination(IncomingPingSource.address(), trafficClass);

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
         if(targetInfo != nullptr) {
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
      if( (!errorCode) && (length >= sizeof(ip)) ) {
         // ====== Decode IPv4 packet =======================================
         // NOTE: raw socket for IPv4 delivers IPv4 header as well!
         const ip* ipHeader = (const ip*)IncomingPingMessageBuffer;
         if( (ipHeader->ip_v == 4) &&
             (ntohs(ipHeader->ip_len) == length) ) {
             const std::size_t headerLength = ipHeader->ip_hl << 2;
             const uint8_t     protocol     = ipHeader->ip_p;
             // ====== Decode ICMP message ==================================
             if( (headerLength + 8 <= length) && (protocol == IPPROTO_ICMP) ) {
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
   unsigned int                       measurementID;
   unsigned int                       logLevel;
   bool                               logColor;
   std::filesystem::path              logFile;
   std::string                        user((getlogin() != nullptr) ? getlogin() : "0");
   bool                               serviceJitter;
   bool                               servicePing;
   bool                               serviceTraceroute;
   unsigned int                       iterations;
   std::vector<std::string>           ioModulesList;
   std::set<std::string>              ioModules;
   std::vector<std::filesystem::path> sourcesFileList;
   std::vector<std::filesystem::path> destinationsFileList;

   TracerouteParameters               tracerouteParameters;
   uint16_t                           tracerouteUDPSourcePort;
   uint16_t                           tracerouteUDPDestinationPort;
   uint16_t                           tracerouteTCPSourcePort;
   uint16_t                           tracerouteTCPDestinationPort;

   TracerouteParameters               pingParameters;
   uint16_t                           pingUDPSourcePort;
   uint16_t                           pingUDPDestinationPort;
   uint16_t                           pingTCPSourcePort;
   uint16_t                           pingTCPDestinationPort;

   TracerouteParameters               jitterParameters;
   uint16_t                           jitterUDPSourcePort;
   uint16_t                           jitterUDPDestinationPort;
   uint16_t                           jitterTCPSourcePort;
   uint16_t                           jitterTCPDestinationPort;
   bool                               jitterRecordRawResults;

   unsigned int                       resultsTransactionLength;
   std::filesystem::path              resultsDirectory;
   std::string                        resultsCompressionString;
   ResultsWriterCompressor            resultsCompression;
   unsigned int                       resultsFormatVersion;

   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )
      ( "check",
           "Check environment" )

      ( "loglevel,L",
           boost::program_options::value<unsigned int>(&logLevel)->default_value(boost::log::trivial::severity_level::info),
           "Set logging level" )
      ( "logfile,O",
           boost::program_options::value<std::filesystem::path>(&logFile)->default_value(std::filesystem::path()),
           "Log file" )
      ( "logcolor,Z",
           boost::program_options::value<bool>(&logColor)->default_value(true),
           "Use ANSI color escape sequences for log output" )
      ( "verbose,v",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::trace),
           "Verbose logging level" )
      ( "quiet,q",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::warning),
           "Quiet logging level" )
      ( "user,U",
           boost::program_options::value<std::string>(&user),
           "User" )

      ( "measurement-id,#",
           boost::program_options::value<unsigned int>(&measurementID)->default_value(0),
           "Measurement identifier" )
      ( "source,S",
           boost::program_options::value<std::vector<std::string>>(),
           "Source address" )
      ( "destination,D",
           boost::program_options::value<std::vector<std::string>>(),
           "Destination address" )
      ( "sources-from-file",
           boost::program_options::value<std::vector<std::filesystem::path>>(&sourcesFileList),
           "Read source addresses from file" )
      ( "destinations-from-file",
           boost::program_options::value<std::vector<std::filesystem::path>>(&destinationsFileList),
           "Read destination addresses from file" )
      ( "iomodule,M",
           boost::program_options::value<std::vector<std::string>>(&ioModulesList),
           "I/O module" )

      ( "jitter,J",
           boost::program_options::value<bool>(&serviceJitter)->default_value(false)->implicit_value(true),
           "Start Jitter service" )
      ( "ping,P",
           boost::program_options::value<bool>(&servicePing)->default_value(false)->implicit_value(true),
           "Start Ping service" )
      ( "traceroute,T",
           boost::program_options::value<bool>(&serviceTraceroute)->default_value(false)->implicit_value(true),
           "Start Traceroute service" )
      ( "iterations,I",
           boost::program_options::value<unsigned int>(&iterations)->default_value(0),
           "Iterations" )

      ( "tracerouteinterval",
           boost::program_options::value<unsigned long long>(&tracerouteParameters.Interval)->default_value(10000),
           "Traceroute interval in ms" )
      ( "tracerouteduration",
           boost::program_options::value<unsigned int>(&tracerouteParameters.Expiration)->default_value(3000),
           "Traceroute duration in ms" )
      ( "tracerouterounds",
           boost::program_options::value<unsigned int>(&tracerouteParameters.Rounds)->default_value(1),
           "Traceroute rounds" )
      ( "tracerouteinitialmaxttl",
           boost::program_options::value<unsigned int>(&tracerouteParameters.InitialMaxTTL)->default_value(6),
           "Traceroute initial maximum TTL value" )
      ( "traceroutefinalmaxttl",
           boost::program_options::value<unsigned int>(&tracerouteParameters.FinalMaxTTL)->default_value(36),
           "Traceroute final maximum TTL value" )
      ( "tracerouteincrementmaxttl",
           boost::program_options::value<unsigned int>(&tracerouteParameters.IncrementMaxTTL)->default_value(6),
           "Traceroute increment maximum TTL value" )
      ( "traceroutepacketsize",
           boost::program_options::value<unsigned int>(&tracerouteParameters.PacketSize)->default_value(0),
           "Traceroute packet size in B" )
      ( "tracerouteudpsourceport",
           boost::program_options::value<uint16_t>(&tracerouteUDPSourcePort)->default_value(0),
           "Traceroute UDP source port" )
      ( "traceroutedestinationport",
           boost::program_options::value<uint16_t>(&tracerouteUDPDestinationPort)->default_value(7),
           "Traceroute UDP destination port" )
      ( "traceroutetcpsourceport",
           boost::program_options::value<uint16_t>(&tracerouteTCPSourcePort)->default_value(0),
           "Traceroute TCP source port" )
      ( "traceroutedestinationport",
           boost::program_options::value<uint16_t>(&tracerouteTCPDestinationPort)->default_value(80),
           "Traceroute TCP destination port" )

      ( "pinginterval",
           boost::program_options::value<unsigned long long>(&pingParameters.Interval)->default_value(1000),
           "Ping interval in ms" )
      ( "pingexpiration",
           boost::program_options::value<unsigned int>(&pingParameters.Expiration)->default_value(30000),
           "Ping expiration timeout in ms" )
      ( "pingburst",
           boost::program_options::value<unsigned int>(&pingParameters.Rounds)->default_value(1),
           "Ping burst" )
      ( "pingttl",
           boost::program_options::value<unsigned int>(&pingParameters.InitialMaxTTL)->default_value(64),
           "Ping initial maximum TTL value" )
      ( "pingpacketsize",
           boost::program_options::value<unsigned int>(&pingParameters.PacketSize)->default_value(0),
           "Ping packet size in B" )
      ( "pingudpsourceport",
           boost::program_options::value<uint16_t>(&pingUDPSourcePort)->default_value(0),
           "Ping UDP source port" )
      ( "pingdestinationport",
           boost::program_options::value<uint16_t>(&pingUDPDestinationPort)->default_value(7),
           "Ping UDP destination port" )
      ( "pingtcpsourceport",
           boost::program_options::value<uint16_t>(&pingTCPSourcePort)->default_value(0),
           "Ping TCP source port" )
      ( "pingdestinationport",
           boost::program_options::value<uint16_t>(&pingTCPDestinationPort)->default_value(80),
           "Ping TCP destination port" )

      ( "jitterinterval",
           boost::program_options::value<unsigned long long>(&jitterParameters.Interval)->default_value(10000),
           "Jitter interval in ms" )
      ( "jitterexpiration",
           boost::program_options::value<unsigned int>(&jitterParameters.Expiration)->default_value(5000),
           "Jitter expiration timeout in ms" )
      ( "jitterburst",
           boost::program_options::value<unsigned int>(&jitterParameters.Rounds)->default_value(16),
           "Jitter burst" )
      ( "jitterttl",
           boost::program_options::value<unsigned int>(&jitterParameters.InitialMaxTTL)->default_value(64),
           "Jitter initial maximum TTL value" )
      ( "jitterpacketsize",
           boost::program_options::value<unsigned int>(&jitterParameters.PacketSize)->default_value(128),
           "Jitter packet size in B" )
      ( "jitterudpsourceport",
           boost::program_options::value<uint16_t>(&jitterUDPSourcePort)->default_value(0),
           "Jitter UDP source port" )
      ( "jitterdestinationport",
           boost::program_options::value<uint16_t>(&jitterUDPDestinationPort)->default_value(7),
           "Jitter UDP destination port" )
      ( "jittertcpsourceport",
           boost::program_options::value<uint16_t>(&jitterTCPSourcePort)->default_value(0),
           "Jitter TCP source port" )
      ( "jitterdestinationport",
           boost::program_options::value<uint16_t>(&jitterTCPDestinationPort)->default_value(80),
           "Jitter TCP destination port" )
      ( "jitterrecordraw",
           boost::program_options::value<bool>(&jitterRecordRawResults)->default_value(false)->implicit_value(true),
           "Record raw Ping results for Jitter computation" )

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
           boost::program_options::value<std::filesystem::path>(&resultsDirectory)->default_value(std::string()),
           "Results directory" )
      ( "resultstransactionlength,l",
           boost::program_options::value<unsigned int>(&resultsTransactionLength)->default_value(60),
           "Results directory in s" )
      ( "resultscompression,C",
           boost::program_options::value<std::string>(&resultsCompressionString)->default_value(std::string("XZ")),
           "Results compression" )
      ( "resultsformat,F",
           boost::program_options::value<unsigned int>(&resultsFormatVersion)->default_value(OutputFormatVersionType::OFT_HiPerConTracer_Version2),
           "Results format version" )
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
      std::cerr << "ERROR: Bad parameter: " << e.what() << "\n";
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << "\n"
                 << commandLineOptions;
       return 1;
   }
   else if(vm.count("check")) {
      checkEnvironment("HPCT Trigger");
      return 0;
   }
   if(vm.count("source")) {
      const std::vector<std::string>& sourceAddressVector = vm["source"].as<std::vector<std::string>>();
      for(std::vector<std::string>::const_iterator iterator = sourceAddressVector.begin();
          iterator != sourceAddressVector.end(); iterator++) {
         if(!addSourceAddress(SourceArray, iterator->c_str())) {
            return 1;
         }
      }
   }
   if(vm.count("destination")) {
      const std::vector<std::string>& destinationAddressVector = vm["destination"].as<std::vector<std::string>>();
      for(std::vector<std::string>::const_iterator iterator = destinationAddressVector.begin();
          iterator != destinationAddressVector.end(); iterator++) {
         if(!addDestinationAddress(DestinationArray, iterator->c_str())) {
            return 1;
         }
      }
   }
   for(const std::filesystem::path& sourceFile : sourcesFileList) {
      if(!addSourceAddressesFromFile(SourceArray, sourceFile)) {
         return -1;
      }
   }
   for(const std::filesystem::path& destinationFile : destinationsFileList) {
      if(!addDestinationAddressesFromFile(DestinationArray, destinationFile)) {
         return -1;
      }
   }
   if(vm.count("iomodule")) {
      for(std::string& ioModule : ioModulesList) {
         boost::algorithm::to_upper(ioModule);
         if(IOModuleBase::checkIOModule(ioModule) == false) {
            std::cerr << "ERROR: Bad IO module name: " << ioModule << "\n";
            return 1;
         }
         ioModules.insert(ioModule);
      }
   }
   else {
      ioModules.insert("ICMP");
   }
   if(measurementID > 0x7fffffff) {
      std::cerr << "ERROR: Invalid Identifier setting: " << measurementID << "\n";
      return 1;
   }
   if( (resultsFormatVersion < OutputFormatVersionType::OFT_Min) ||
       (resultsFormatVersion > OutputFormatVersionType::OFT_Max) ) {
      std::cerr << "ERROR: Invalid results format version: " << resultsFormatVersion << "\n";
      return 1;
   }
   boost::algorithm::to_upper(resultsCompressionString);
   if(resultsCompressionString == "XZ") {
      resultsCompression = ResultsWriterCompressor::XZ;
   }
   else if(resultsCompressionString == "BZIP2") {
      resultsCompression = ResultsWriterCompressor::BZip2;
   }
   else if(resultsCompressionString == "GZIP") {
      resultsCompression = ResultsWriterCompressor::GZip;
   }
   else if(resultsCompressionString == "NONE") {
      resultsCompression = ResultsWriterCompressor::None;
   }
   else {
      std::cerr << "ERROR: Invalid results compression: " << resultsCompressionString << "\n";
      return 1;
   }


   // ====== Initialize =====================================================
   initialiseLogger(logLevel, logColor,
                    (logFile != std::filesystem::path()) ? logFile.string().c_str() : nullptr);
   const passwd* pw = getUser(user.c_str());
   if(pw == nullptr) {
      HPCT_LOG(fatal) << "Cannot find user \"" << user << "\"!";
      return 1;
   }
   if(SourceArray.size() < 1) {
      HPCT_LOG(fatal) << "At least one source is needed!";
      return 1;
   }
   if( (serviceJitter == false) && (servicePing == false) && (serviceTraceroute == false) ) {
      HPCT_LOG(fatal) << "Enable at least on service (Traceroute, Ping, Jitter)!";
      return 1;
   }

   std::srand(std::time(0));
   pingParameters.Interval              = std::min(std::max(100ULL, pingParameters.Interval),          3600U*60000ULL);
   pingParameters.Expiration            = std::min(std::max(100U, pingParameters.Expiration),          3600U*60000U);
   pingParameters.InitialMaxTTL         = std::min(std::max(1U, pingParameters.InitialMaxTTL),         255U);
   pingParameters.FinalMaxTTL           = pingParameters.InitialMaxTTL;
   pingParameters.IncrementMaxTTL       = 1;
   pingParameters.Rounds                = std::min(std::max(1U, pingParameters.Rounds),                1024U);
   pingParameters.PacketSize            = std::min(65535U, pingParameters.PacketSize);
   tracerouteParameters.Interval        = std::min(std::max(1000ULL, tracerouteParameters.Interval),   3600U*60000ULL);
   tracerouteParameters.Expiration      = std::min(std::max(1000U, tracerouteParameters.Expiration),   60000U);
   tracerouteParameters.InitialMaxTTL   = std::min(std::max(1U, tracerouteParameters.InitialMaxTTL),   255U);
   tracerouteParameters.FinalMaxTTL     = std::min(std::max(1U, tracerouteParameters.FinalMaxTTL),     255U);
   tracerouteParameters.IncrementMaxTTL = std::min(std::max(1U, tracerouteParameters.IncrementMaxTTL), 255U);
   tracerouteParameters.PacketSize      = std::min(65535U, tracerouteParameters.PacketSize);
   tracerouteParameters.Rounds          = std::min(std::max(1U, tracerouteParameters.Rounds),          64U);

   if(!resultsDirectory.empty()) {
      HPCT_LOG(info) << "Results Output:" << "\n"
                     << "* MeasurementID      = " << measurementID            << "\n"
                     << "* Results Directory  = " << resultsDirectory         << "\n"
                     << "* Transaction Length = " << resultsTransactionLength << " s";
   }
   else {
      HPCT_LOG(info) << "Results Output:" << "\n"
                     << "-- turned off--";
   }

   if(serviceJitter) {
      HPCT_LOG(info) << "Jitter Service:" << std:: endl
                     << "* Interval           = " << jitterParameters.Interval   << " ms" << "\n"
                     << "* Expiration         = " << jitterParameters.Expiration << " ms" << "\n"
                     << "* Burst              = " << jitterParameters.Rounds              << "\n"
                     << "* TTL                = " << jitterParameters.InitialMaxTTL       << "\n"
                     << "* Packet Size        = " << jitterParameters.PacketSize          << " B";
   }
   if(servicePing) {
      HPCT_LOG(info) << "Ping Service:" << std:: endl
                     << "* Interval           = " << pingParameters.Interval   << " ms" << "\n"
                     << "* Expiration         = " << pingParameters.Expiration << " ms" << "\n"
                     << "* Burst              = " << pingParameters.Rounds              << "\n"
                     << "* TTL                = " << pingParameters.InitialMaxTTL       << "\n"
                     << "* Packet Size        = " << pingParameters.PacketSize          << " B";
   }
   if(serviceTraceroute) {
      HPCT_LOG(info) << "Traceroute Service:" << std:: endl
                     << "* Interval           = " << tracerouteParameters.Interval        << " ms" << "\n"
                     << "* Expiration         = " << tracerouteParameters.Expiration      << " ms" << "\n"
                     << "* Rounds             = " << tracerouteParameters.Rounds          << "\n"
                     << "* Initial MaxTTL     = " << tracerouteParameters.InitialMaxTTL   << "\n"
                     << "* Final MaxTTL       = " << tracerouteParameters.FinalMaxTTL     << "\n"
                     << "* Increment MaxTTL   = " << tracerouteParameters.IncrementMaxTTL << "\n"
                     << "* Packet Size        = " << tracerouteParameters.PacketSize      << " B";
   }
   HPCT_LOG(info) << "Trigger:" << std::endl
                  << "* Ping Trigger Age     = " << PingTriggerAge << " s" << std::endl
                  << "* Ping Trigger Length  = " << PingTriggerLength      << std::endl
                  << "* Pings before Queuing = " << PingsBeforeQueuing;


   // ====== Start service threads ==========================================
   for(std::map<boost::asio::ip::address, std::set<uint8_t>>::iterator sourceIterator = SourceArray.begin();
      sourceIterator != SourceArray.end(); sourceIterator++) {
      const boost::asio::ip::address& sourceAddress = sourceIterator->first;

      std::set<DestinationInfo> destinationsForSource;
      for(std::set<boost::asio::ip::address>::iterator destinationIterator = DestinationArray.begin();
          destinationIterator != DestinationArray.end(); destinationIterator++) {
         const boost::asio::ip::address& destinationAddress = *destinationIterator;
         for(std::set<uint8_t>::iterator trafficClassIterator = sourceIterator->second.begin();
             trafficClassIterator != sourceIterator->second.end(); trafficClassIterator++) {
            const uint8_t trafficClass = *trafficClassIterator;
            std::cout << destinationAddress << " " << (unsigned int)trafficClass << std::endl;
            destinationsForSource.insert(DestinationInfo(destinationAddress, trafficClass));
         }
      }


      for(std::set<DestinationInfo>::iterator iterator = destinationsForSource.begin();
          iterator != destinationsForSource.end(); iterator++) {
         std::cout << " -> " << *iterator << std::endl;
      }

      for(const std::string& ioModule : ioModules) {
         if(serviceJitter) {
            try {
               ResultsWriter* resultsWriter = nullptr;
               if(!resultsDirectory.empty()) {
                  resultsWriter = ResultsWriter::makeResultsWriter(
                                     ResultsWriterSet, ProgramID, measurementID,
                                     sourceAddress, "TriggeredJitter-" + ioModule,
                                     resultsDirectory, resultsTransactionLength,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  if(resultsWriter == nullptr) {
                     HPCT_LOG(fatal) << "Cannot initialise results directory " << resultsDirectory << "!";
                     return 1;
                  }
               }
               if(ioModule == "UDP") {
                  jitterParameters.SourcePort      = jitterUDPSourcePort;
                  jitterParameters.DestinationPort = jitterUDPDestinationPort;
               }
               else if(ioModule == "TCP") {
                  jitterParameters.SourcePort      = jitterTCPSourcePort;
                  jitterParameters.DestinationPort = jitterTCPDestinationPort;
               }
               else {
                  jitterParameters.SourcePort      = 0;
                  jitterParameters.DestinationPort = 0;
               }
               Service* service = new Jitter(ioModule,
                                             resultsWriter, "Jitter", (OutputFormatVersionType)resultsFormatVersion, iterations, true,
                                             sourceAddress, destinationsForSource,
                                             jitterParameters);
               if(service->start() == false) {
                  return 1;
               }
               ServiceSet.insert(service);
            }
            catch (std::exception& e) {
               HPCT_LOG(fatal) << "Cannot create Jitter service - " << e.what();
               return 1;
            }
         }
         if(servicePing) {
            try {
               ResultsWriter* resultsWriter = nullptr;
               if(!resultsDirectory.empty()) {
                  resultsWriter = ResultsWriter::makeResultsWriter(
                                     ResultsWriterSet, ProgramID, measurementID,
                                     sourceAddress, "TriggeredPing-" + ioModule,
                                     resultsDirectory, resultsTransactionLength,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  if(resultsWriter == nullptr) {
                     HPCT_LOG(fatal) << "Cannot initialise results directory " << resultsDirectory << "!";
                     return 1;
                  }
               }
               if(ioModule == "UDP") {
                  pingParameters.SourcePort      = pingUDPSourcePort;
                  pingParameters.DestinationPort = pingUDPDestinationPort;
               }
               else if(ioModule == "TCP") {
                  pingParameters.SourcePort      = pingTCPSourcePort;
                  pingParameters.DestinationPort = pingTCPDestinationPort;
               }
               else {
                  pingParameters.SourcePort      = 0;
                  pingParameters.DestinationPort = 0;
               }
               Service* service = new Ping(ioModule,
                                           resultsWriter, "Ping", (OutputFormatVersionType)resultsFormatVersion, iterations, true,
                                           sourceAddress, destinationsForSource,
                                           pingParameters);
               if(service->start() == false) {
                  return 1;
               }
               ServiceSet.insert(service);
            }
            catch (std::exception& e) {
               HPCT_LOG(fatal) << "Cannot create Ping service - " << e.what();
               return 1;
            }
         }
         if(serviceTraceroute) {
            try {
               ResultsWriter* resultsWriter = nullptr;
               if(!resultsDirectory.empty()) {
                  resultsWriter = ResultsWriter::makeResultsWriter(
                                     ResultsWriterSet, ProgramID, measurementID,
                                     sourceAddress, "TriggeredTraceroute-" + ioModule,
                                     resultsDirectory, resultsTransactionLength,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  if(resultsWriter == nullptr) {
                     HPCT_LOG(fatal) << "Cannot initialise results directory " << resultsDirectory << "!";
                     return 1;
                  }
               }
               if(ioModule == "UDP") {
                  tracerouteParameters.SourcePort      = tracerouteUDPSourcePort;
                  tracerouteParameters.DestinationPort = tracerouteUDPDestinationPort;
               }
               else if(ioModule == "TCP") {
                  tracerouteParameters.SourcePort      = tracerouteTCPSourcePort;
                  tracerouteParameters.DestinationPort = tracerouteTCPDestinationPort;
               }
               else {
                  tracerouteParameters.SourcePort      = 0;
                  tracerouteParameters.DestinationPort = 0;
               }
               Service* service = new Traceroute(ioModule,
                                                 resultsWriter, "Traceroute", (OutputFormatVersionType)resultsFormatVersion, iterations, true,
                                                 sourceAddress, destinationsForSource,
                                                 tracerouteParameters);
               if(service->start() == false) {
                  return 1;
               }
               ServiceSet.insert(service);
            }
            catch (std::exception& e) {
               HPCT_LOG(fatal) << "Cannot create Traceroute service - " << e.what();
               return 1;
            }
         }
      }
   }


   // ====== Raw socket =====================================================
   SnifferSocketV4.open(boost::asio::ip::icmp::v4());
   SnifferSocketV6.open(boost::asio::ip::icmp::v6());
   SnifferSocketV4.async_receive_from(boost::asio::buffer(IncomingPingMessageBuffer), IncomingPingSource, receivedPingV4);
   SnifferSocketV6.async_receive_from(boost::asio::buffer(IncomingPingMessageBuffer), IncomingPingSource, receivedPingV6);


   // ====== Reduce privileges ==============================================
   if(reducePrivileges(pw) == false) {
      HPCT_LOG(fatal) << "Failed to reduce privileges!";
      return 1;
   }


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
