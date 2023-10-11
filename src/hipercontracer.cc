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

#include <iostream>
#include <vector>

#include <boost/version.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/program_options.hpp>

#include <sys/utsname.h>

#include "icmpheader.h"
#include "jitter.h"
#include "logger.h"
#include "ping.h"
#include "resultswriter.h"
#include "service.h"
#include "tools.h"
#include "traceroute.h"


static std::map<boost::asio::ip::address, std::set<uint8_t>> SourceArray;
static std::set<boost::asio::ip::address>                    DestinationArray;
static std::set<ResultsWriter*>                              ResultsWriterSet;
static std::set<Service*>                                    ServiceSet;
static boost::asio::io_service                               IOService;
static boost::asio::signal_set                               Signals(IOService, SIGINT, SIGTERM);
static boost::posix_time::milliseconds                       CleanupTimerInterval(1000);
static boost::asio::deadline_timer                           CleanupTimer(IOService, CleanupTimerInterval);


// ###### Signal handler ####################################################
static void signalHandler(const boost::system::error_code& error, int signal_number)
{
   if(error != boost::asio::error::operation_aborted) {
      puts("\n*** Shutting down! ***\n");   // Avoids a false positive from Helgrind.
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
   }
   else {
      Signals.cancel();
   }
}


// ###### Check environment #################################################
static void checkEnvironment()
{
   // ====== System information =============================================
   utsname sysInfo;
   if(uname(&sysInfo) == 0) {
      std::cout << "System Information:\n"
                << "* System: \t" << sysInfo.sysname  << "\n"
                << "* Name:   \t" << sysInfo.nodename << "\n"
                << "* Release:\t" << sysInfo.release  << "\n"
                << "* Version:\t" << sysInfo.version  << "\n"
                << "* Machine:\t" << sysInfo.machine  << "\n";
   }

   // ====== Build environment ==============================================
   std::cout << "Build Environment:\n"
             << "* BOOST Version:  \t" << BOOST_VERSION  << "\n"
             << "* BOOST Compiler: \t" << BOOST_COMPILER << "\n"
             << "* BOOST StdLib:   \t" << BOOST_STDLIB   << "\n"
             << "* C++ Standard:   \t" << __cplusplus    << "\n";
#ifdef BOOST_HAS_CLOCK_GETTIME
   std::cout << "* clock_gettime():\tyes\n";
#else
   std::cout << "* clock_gettime():\tno\n";
#endif
#ifdef BOOST_HAS_GETTIMEOFDAY
   std::cout << "* gettimeofday(): \tyes\n";
#else
   std::cout << "* gettimeofday(): \tno\n";
#endif

   // ====== Clock granularities ============================================
   const std::chrono::time_point<std::chrono::system_clock>          n1a = std::chrono::system_clock::now();
   const std::chrono::time_point<std::chrono::system_clock>          n1b = nowInUTC<std::chrono::time_point<std::chrono::system_clock>>();
   const std::chrono::time_point<std::chrono::steady_clock>          n2a = std::chrono::steady_clock::now();
   const std::chrono::time_point<std::chrono::steady_clock>          n2b = nowInUTC<std::chrono::time_point<std::chrono::steady_clock>>();
   const std::chrono::time_point<std::chrono::high_resolution_clock> n3a = std::chrono::high_resolution_clock::now();
   const std::chrono::time_point<std::chrono::high_resolution_clock> n3b = nowInUTC<std::chrono::time_point<std::chrono::high_resolution_clock>>();

   std::cout << "Clocks Granularities:\n"

             << "* std::chrono::system_clock:        \t"
             << std::chrono::time_point<std::chrono::system_clock>::period::num << "/"
             << std::chrono::time_point<std::chrono::system_clock>::period::den << " s\t"
             << (std::chrono::system_clock::is_steady ? "steady    " : "not steady") << "\t"
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n1a.time_since_epoch()).count() << " ns / "
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n1b.time_since_epoch()).count() << " ns since epoch\n"

             << "* std::chrono::steady_clock:        \t"
             << std::chrono::time_point<std::chrono::steady_clock>::period::num << "/"
             << std::chrono::time_point<std::chrono::steady_clock>::period::den << " s\t"
             << (std::chrono::steady_clock::is_steady ? "steady    " : "not steady") << "\t"
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n2a.time_since_epoch()).count() << " ns / "
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n2b.time_since_epoch()).count() << " ns since epoch\n"

             << "* std::chrono::high_resolution_clock:\t"
             << std::chrono::time_point<std::chrono::high_resolution_clock>::period::num << "/"
             << std::chrono::time_point<std::chrono::high_resolution_clock>::period::den << " s\t"
             << (std::chrono::high_resolution_clock::is_steady ? "steady    " : "not steady") << "\t"
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n3a.time_since_epoch()).count() << " ns / "
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n3b.time_since_epoch()).count() << " ns since epoch\n"
             ;
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   unsigned int             measurementID;
   unsigned int             logLevel;
   bool                     logColor;
   std::filesystem::path    logFile;
   std::string              user((getlogin() != nullptr) ? getlogin() : "0");
   std::string              configurationFileName;
   bool                     serviceJitter;
   bool                     servicePing;
   bool                     serviceTraceroute;
   unsigned int             iterations;
   std::vector<std::string> ioModulesList;
   std::set<std::string>    ioModules;

   unsigned long long       jitterInterval;
   unsigned int             jitterExpiration;
   unsigned int             jitterBurst;
   unsigned int             jitterTTL;
   unsigned int             jitterPacketSize;
   bool                     jitterRecordRawResults;

   unsigned long long       pingInterval;
   unsigned int             pingExpiration;
   unsigned int             pingBurst;
   unsigned int             pingTTL;
   unsigned int             pingPacketSize;

   unsigned long long       tracerouteInterval;
   unsigned int             tracerouteExpiration;
   unsigned int             tracerouteRounds;
   unsigned int             tracerouteInitialMaxTTL;
   unsigned int             tracerouteFinalMaxTTL;
   unsigned int             tracerouteIncrementMaxTTL;
   unsigned int             traceroutePacketSize;

   uint16_t                 udpDestinationPort;

   unsigned int             resultsTransactionLength;
   std::filesystem::path    resultsDirectory;
   std::string              resultsCompressionString;
   ResultsWriterCompressor  resultsCompression;
   unsigned int             resultsFormat;

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
      ( "traceroutepacketsize",
           boost::program_options::value<unsigned int>(&traceroutePacketSize)->default_value(0),
           "Traceroute packet size in B" )

      ( "pinginterval",
           boost::program_options::value<unsigned long long>(&pingInterval)->default_value(1000),
           "Ping interval in ms" )
      ( "pingexpiration",
           boost::program_options::value<unsigned int>(&pingExpiration)->default_value(30000),
           "Ping expiration timeout in ms" )
      ( "pingburst",
           boost::program_options::value<unsigned int>(&pingBurst)->default_value(1),
           "Ping burst" )
      ( "pingttl",
           boost::program_options::value<unsigned int>(&pingTTL)->default_value(64),
           "Ping initial maximum TTL value" )
      ( "pingpacketsize",
           boost::program_options::value<unsigned int>(&pingPacketSize)->default_value(0),
           "Ping packet size in B" )

      ( "jitterinterval",
           boost::program_options::value<unsigned long long>(&jitterInterval)->default_value(10000),
           "Jitter interval in ms" )
      ( "jitterexpiration",
           boost::program_options::value<unsigned int>(&jitterExpiration)->default_value(5000),
           "Jitter expiration timeout in ms" )
      ( "jitterburst",
           boost::program_options::value<unsigned int>(&jitterBurst)->default_value(16),
           "Jitter burst" )
      ( "jitterttl",
           boost::program_options::value<unsigned int>(&jitterTTL)->default_value(64),
           "Jitter initial maximum TTL value" )
      ( "jitterpacketsize",
           boost::program_options::value<unsigned int>(&jitterPacketSize)->default_value(128),
           "Jitter packet size in B" )
      ( "jitterrecordraw",
           boost::program_options::value<bool>(&jitterRecordRawResults)->default_value(false)->implicit_value(true),
           "Record raw Ping results for Jitter computation" )

      ( "udpdestinationport",
           boost::program_options::value<uint16_t>(&udpDestinationPort)->default_value(7),
           "UDP destination port" )

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
           boost::program_options::value<unsigned int>(&resultsFormat)->default_value(OutputFormatType::OFT_HiPerConTracer_Version2),
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
      checkEnvironment();
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
      std::cerr << "ERROR: Invalid MeasurementID setting: " << measurementID << "\n";
      return 1;
   }
   if( (resultsFormat < OutputFormatType::OFT_Min) ||
       (resultsFormat > OutputFormatType::OFT_Max) ) {
      std::cerr << "ERROR: Invalid results format version: " << resultsFormat << "\n";
      return 1;
   }
   if(jitterExpiration >= jitterInterval) {
      std::cerr << "ERROR: Jitter expiration must be smaller than jitter interval" << "\n";
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
   if( (SourceArray.size() < 1) || (DestinationArray.size() < 1) ) {
      HPCT_LOG(fatal) << "At least one source and one destination are needed!";
      return 1;
   }
   if( (serviceJitter == false) && (servicePing == false) && (serviceTraceroute == false) ) {
      HPCT_LOG(fatal) << "Enable at least on service (Traceroute, Ping, Jitter)!";
      return 1;
   }

   std::srand(std::time(0));
   jitterInterval            = std::min(std::max(100ULL, jitterInterval),        3600U*10000ULL);
   jitterExpiration          = std::min(std::max(100U, jitterExpiration),        3600U*10000U);
   jitterTTL                 = std::min(std::max(1U, jitterTTL),                 255U);
   jitterBurst               = std::min(std::max(2U, jitterBurst),               1024U);
   jitterPacketSize          = std::min(65535U, jitterPacketSize);
   pingInterval              = std::min(std::max(100ULL, pingInterval),          3600U*60000ULL);
   pingExpiration            = std::min(std::max(100U, pingExpiration),          3600U*60000U);
   pingTTL                   = std::min(std::max(1U, pingTTL),                   255U);
   pingBurst                 = std::min(std::max(1U, pingBurst),                 1024U);
   pingPacketSize            = std::min(65535U, pingPacketSize);
   tracerouteInterval        = std::min(std::max(1000ULL, tracerouteInterval),   3600U*60000ULL);
   tracerouteExpiration      = std::min(std::max(1000U, tracerouteExpiration),   60000U);
   tracerouteInitialMaxTTL   = std::min(std::max(1U, tracerouteInitialMaxTTL),   255U);
   tracerouteFinalMaxTTL     = std::min(std::max(1U, tracerouteFinalMaxTTL),     255U);
   tracerouteIncrementMaxTTL = std::min(std::max(1U, tracerouteIncrementMaxTTL), 255U);
   traceroutePacketSize      = std::min(65535U, traceroutePacketSize);
   tracerouteRounds          = std::min(std::max(1U, tracerouteRounds),          64U);

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
                     << "* Interval           = " << jitterInterval   << " ms" << "\n"
                     << "* Expiration         = " << jitterExpiration << " ms" << "\n"
                     << "* Burst              = " << jitterBurst               << "\n"
                     << "* TTL                = " << jitterTTL                 << "\n"
                     << "* Packet Size        = " << jitterPacketSize          << " B";
   }
   if(servicePing) {
      HPCT_LOG(info) << "Ping Service:" << std:: endl
                     << "* Interval           = " << pingInterval   << " ms" << "\n"
                     << "* Expiration         = " << pingExpiration << " ms" << "\n"
                     << "* Burst              = " << pingBurst               << "\n"
                     << "* TTL                = " << pingTTL                 << "\n"
                     << "* Packet Size        = " << pingPacketSize          << " B";
   }
   if(serviceTraceroute) {
      HPCT_LOG(info) << "Traceroute Service:" << std:: endl
                     << "* Interval           = " << tracerouteInterval        << " ms" << "\n"
                     << "* Expiration         = " << tracerouteExpiration      << " ms" << "\n"
                     << "* Rounds             = " << tracerouteRounds          << "\n"
                     << "* Initial MaxTTL     = " << tracerouteInitialMaxTTL   << "\n"
                     << "* Final MaxTTL       = " << tracerouteFinalMaxTTL     << "\n"
                     << "* Increment MaxTTL   = " << tracerouteIncrementMaxTTL << "\n"
                     << "* Packet Size        = " << traceroutePacketSize      << " B";
   }


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
            // std::cout << destinationAddress << " " << (unsigned int)trafficClass << "\n";
            destinationsForSource.insert(DestinationInfo(destinationAddress, trafficClass));
         }
      }

/*
      for(std::set<DestinationInfo>::iterator iterator = destinationsForSource.begin();
          iterator != destinationsForSource.end(); iterator++) {
         std::cout << " -> " << *iterator << "\n";
      }
*/

      for(const std::string& ioModule : ioModules) {
         const uint16_t port = udpDestinationPort;
         if(serviceJitter) {
            try {
               ResultsWriter* resultsWriter = nullptr;
               if(!resultsDirectory.empty()) {
                  resultsWriter = ResultsWriter::makeResultsWriter(
                                     ResultsWriterSet, measurementID,
                                     sourceAddress, "Jitter-" + ioModule,
                                     resultsDirectory, resultsTransactionLength,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  if(resultsWriter == nullptr) {
                     HPCT_LOG(fatal) << "Cannot initialise results directory " << resultsDirectory << "!";
                     return 1;
                  }
               }
               Service* service = new Jitter(ioModule,
                                             resultsWriter, (OutputFormatType)resultsFormat,
                                             iterations, false,
                                             sourceAddress, destinationsForSource,
                                             jitterRecordRawResults,
                                             jitterInterval, jitterExpiration,
                                             jitterBurst, jitterTTL,
                                             jitterPacketSize, port);
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
                                     ResultsWriterSet, measurementID,
                                     sourceAddress, "Ping-" + ioModule,
                                     resultsDirectory, resultsTransactionLength,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  if(resultsWriter == nullptr) {
                     HPCT_LOG(fatal) << "Cannot initialise results directory " << resultsDirectory << "!";
                     return 1;
                  }
               }
               Service* service = new Ping(ioModule,
                                           resultsWriter, (OutputFormatType)resultsFormat,
                                           iterations, false,
                                           sourceAddress, destinationsForSource,
                                           pingInterval, pingExpiration,
                                           pingBurst, pingTTL,
                                           pingPacketSize, port);
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
                                     ResultsWriterSet, measurementID,
                                     sourceAddress, "Traceroute-" + ioModule,
                                     resultsDirectory, resultsTransactionLength,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  if(resultsWriter == nullptr) {
                     HPCT_LOG(fatal) << "Cannot initialise results directory " << resultsDirectory << "!";
                     return 1;
                  }
               }
               Service* service = new Traceroute(ioModule,
                                                 resultsWriter, (OutputFormatType)resultsFormat,
                                                 iterations, false,
                                                 sourceAddress, destinationsForSource,
                                                 tracerouteInterval, tracerouteExpiration,
                                                 tracerouteRounds,
                                                 tracerouteInitialMaxTTL, tracerouteFinalMaxTTL,
                                                 tracerouteIncrementMaxTTL,
                                                 traceroutePacketSize, port);
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

   return(0);
}
