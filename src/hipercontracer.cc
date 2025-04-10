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

#include <iostream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/program_options.hpp>

#include "check.h"
// #include "jitter.h"
#include "logger.h"
#include "package-version.h"
#include "ping.h"
#include "resultswriter.h"
#include "tools.h"
#include "traceroute.h"


static const std::string                                     ProgramID = std::string("HiPerConTracer/") + HPCT_VERSION;
static std::map<boost::asio::ip::address, std::set<uint8_t>> SourceArray;
static std::set<boost::asio::ip::address>                    DestinationArray;
static std::set<ResultsWriter*>                              ResultsWriterSet;
static std::set<Service*>                                    ServiceSet;
static boost::asio::io_context                               IOContext;
static boost::asio::signal_set                               Signals(IOContext, SIGINT, SIGTERM);
static boost::posix_time::milliseconds                       CleanupTimerInterval(1000);
static boost::asio::deadline_timer                           CleanupTimer(IOContext, CleanupTimerInterval);


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


// ###### Filter source/destination addresses ###############################
static void cleanAddresses(unsigned int& sourcesIPv4,
                           unsigned int& sourcesIPv6,
                           unsigned int& destinationsIPv4,
                           unsigned int& destinationsIPv6)
{
   sourcesIPv4 = 0;
   sourcesIPv6 = 0;
   for(std::map<boost::asio::ip::address, std::set<uint8_t>>::iterator sourceIterator = SourceArray.begin();
      sourceIterator != SourceArray.end(); sourceIterator++) {
      const boost::asio::ip::address& sourceAddress = sourceIterator->first;
      if(sourceAddress.is_v6()) {
         sourcesIPv6++;
      }
      else {
         sourcesIPv4++;
      }
   }
   destinationsIPv4 = 0;
   destinationsIPv6 = 0;
   for(std::set<boost::asio::ip::address>::iterator destinationIterator = DestinationArray.begin();
         destinationIterator != DestinationArray.end(); destinationIterator++) {
      const boost::asio::ip::address& destinationAddress = *destinationIterator;
      if(destinationAddress.is_v6()) {
         destinationsIPv6++;
      }
      else {
         destinationsIPv4++;
      }
   }
   if( (sourcesIPv4 == 0) && (sourcesIPv6 == 0) ) {
      if(destinationsIPv4 > 0) {
         HPCT_LOG(info) << "NOTE: Adding 0.0.0.0 as IPv4 source, since none is given!";
         addSourceAddress(SourceArray, "0.0.0.0");
         sourcesIPv4++;
      }
      if(destinationsIPv6 > 0) {
         HPCT_LOG(info) << "NOTE: Adding :: as IPv6 source, since none is given!";
         addSourceAddress(SourceArray, "::");
         sourcesIPv6++;
      }
   }
   if( (sourcesIPv4 == 0) || (destinationsIPv4 == 0) ) {
      HPCT_LOG(info) << "No IPv4 source-destination pair -> removing IPv4!";
      std::map<boost::asio::ip::address, std::set<uint8_t>>::iterator sourceIterator = SourceArray.begin();
      while(sourceIterator != SourceArray.end()) {
         const boost::asio::ip::address& sourceAddress = sourceIterator->first;
         if(sourceAddress.is_v4()) {
            sourceIterator = SourceArray.erase(sourceIterator);
            sourcesIPv4--;
         }
         else {
            sourceIterator++;
         }
      }
      std::set<boost::asio::ip::address>::iterator destinationIterator = DestinationArray.begin();
      while(destinationIterator != DestinationArray.end()) {
         const boost::asio::ip::address& destinationAddress = *destinationIterator;
         if(destinationAddress.is_v4()) {
            destinationIterator = DestinationArray.erase(destinationIterator);
            destinationsIPv4--;
         }
         else {
            destinationIterator++;
         }
      }
   }
   if( (sourcesIPv6 == 0) || (destinationsIPv6 == 0) ) {
      HPCT_LOG(info) << "No IPv6 source-destination pair -> removing IPv6!";
      std::map<boost::asio::ip::address, std::set<uint8_t>>::iterator sourceIterator = SourceArray.begin();
      while(sourceIterator != SourceArray.end()) {
         const boost::asio::ip::address& sourceAddress = sourceIterator->first;
         if(sourceAddress.is_v6()) {
            sourceIterator = SourceArray.erase(sourceIterator);
            sourcesIPv6--;
         }
         else {
            sourceIterator++;
         }
      }
      std::set<boost::asio::ip::address>::iterator destinationIterator = DestinationArray.begin();
      while(destinationIterator != DestinationArray.end()) {
         const boost::asio::ip::address& destinationAddress = *destinationIterator;
         if(destinationAddress.is_v6()) {
            destinationIterator = DestinationArray.erase(destinationIterator);
            destinationsIPv6--;
         }
         else {
            destinationIterator++;
         }
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



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   unsigned int                       measurementID;
   unsigned int                       logLevel;
   bool                               logColor;
   std::filesystem::path              logFile;
   std::string                        user((getlogin() != nullptr) ? getlogin() : "0");
   // bool                               serviceJitter;
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

   TracerouteParameters               pingParameters;
   uint16_t                           pingUDPSourcePort;
   uint16_t                           pingUDPDestinationPort;

#if 0
   TracerouteParameters               jitterParameters;
   uint16_t                           jitterUDPSourcePort;
   uint16_t                           jitterUDPDestinationPort;
   bool                               jitterRecordRawResults;
#endif

   unsigned int                       resultsTransactionLength;
   std::filesystem::path              resultsDirectory;
   std::string                        resultsCompressionString;
   ResultsWriterCompressor            resultsCompression;
   unsigned int                       resultsFormatVersion;
   unsigned int                       resultsTimestampDepth;

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

#if 0
      ( "jitter,J",
           boost::program_options::value<bool>(&serviceJitter)->default_value(false)->implicit_value(true),
           "Start Jitter service" )
#endif
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
      ( "tracerouteintervaldeviation",
           boost::program_options::value<float>(&tracerouteParameters.Deviation)->default_value(0.1),
           "Traceroute interval deviation fraction (0.0 to 1.0)" )
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
      ( "tracerouteudpdestinationport",
           boost::program_options::value<uint16_t>(&tracerouteUDPDestinationPort)->default_value(7),
           "Traceroute UDP destination port" )

      ( "pinginterval",
           boost::program_options::value<unsigned long long>(&pingParameters.Interval)->default_value(1000),
           "Ping interval in ms" )
      ( "pingintervaldeviation",
           boost::program_options::value<float>(&pingParameters.Deviation)->default_value(0.1),
           "Ping interval deviation fraction (0.0 to 1.0)" )
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
      ( "pingudpdestinationport",
           boost::program_options::value<uint16_t>(&pingUDPDestinationPort)->default_value(7),
           "Ping UDP destination port" )

#if 0
      ( "jitterinterval",
           boost::program_options::value<unsigned long long>(&jitterParameters.Interval)->default_value(10000),
           "Jitter interval in ms" )
      ( "jitterintervaldeviation",
           boost::program_options::value<float>(&jitterParameters.Deviation)->default_value(0.1),
           "Jitter interval deviation fraction (0.0 to 1.0)" )
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
      ( "jitterudpdestinationport",
           boost::program_options::value<uint16_t>(&jitterUDPDestinationPort)->default_value(7),
           "Jitter UDP destination port" )
      ( "jitterrecordraw",
           boost::program_options::value<bool>(&jitterRecordRawResults)->default_value(false)->implicit_value(true),
           "Record raw Ping results for Jitter computation" )
#endif

      ( "resultsdirectory,R",
           boost::program_options::value<std::filesystem::path>(&resultsDirectory)->default_value(std::string()),
           "Results directory" )
      ( "resultstransactionlength,x",
           boost::program_options::value<unsigned int>(&resultsTransactionLength)->default_value(60),
           "Results directory in s" )
      ( "resultscompression,C",
           boost::program_options::value<std::string>(&resultsCompressionString)->default_value(std::string("XZ")),
           "Results compression" )
      ( "resultsformat,F",
           boost::program_options::value<unsigned int>(&resultsFormatVersion)->default_value(OutputFormatVersionType::OFT_HiPerConTracer_Version2),
           "Results format version" )
      ( "resultstimestampdepth,z",
           boost::program_options::value<unsigned int>(&resultsTimestampDepth)->default_value(0),
           "Results timestamp depth" )
    ;

   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
                                       style(
                                          boost::program_options::command_line_style::style_t::unix_style
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
       std::cerr << "Usage: " << argv[0] << " OPTIONS" << "\n"
                 << commandLineOptions;
       return 1;
   }
   else if(vm.count("check")) {
      checkEnvironment("HiPerConTracer");
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
      std::cerr << "ERROR: Invalid MeasurementID setting: " << measurementID << "\n";
      return 1;
   }
   if( (pingParameters.Deviation < 0.0) || (pingParameters.Deviation > 1.0) ) {
      std::cerr << "ERROR: Invalid Ping interval deviation setting: "
                << pingParameters.Deviation << "\n";
      return 1;
   }
   if( (tracerouteParameters.Deviation < 0.0) || (tracerouteParameters.Deviation > 1.0) ) {
      std::cerr << "ERROR: Invalid Traceroute interval deviation setting: "
                << tracerouteParameters.Deviation << "\n";
   }
   if(tracerouteParameters.InitialMaxTTL > tracerouteParameters.FinalMaxTTL) {
      std::cerr << "NOTE: Setting TracerouteInitialMaxTTL to TracerouteFinalMaxTTL=" << tracerouteParameters.FinalMaxTTL << "!\n";
      tracerouteParameters.InitialMaxTTL = tracerouteParameters.FinalMaxTTL;
      return 1;
   }
   if( (resultsFormatVersion < OutputFormatVersionType::OFT_Min) ||
       (resultsFormatVersion > OutputFormatVersionType::OFT_Max) ) {
      std::cerr << "ERROR: Invalid results format version: " << resultsFormatVersion << "\n";
      return 1;
   }
#if 0
   if(jitterParameters.Expiration >= jitterParameters.Interval) {
      std::cerr << "ERROR: Jitter expiration must be smaller than jitter interval" << "\n";
      return 1;
   }
   if( (jitterParameters.Deviation < 0.0) || (jitterParameters.Deviation > 1.0) ) {
      std::cerr << "ERROR: Invalid Jitter interval deviation setting: "
                << jitterParameters.Deviation << "\n";
      return 1;
   }
#endif
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
                    (!logFile.empty()) ? logFile.string().c_str() : nullptr);
   const passwd* pw = getUser(user.c_str());
   if(pw == nullptr) {
      HPCT_LOG(fatal) << "Cannot find user \"" << user << "\"!";
      return 1;
   }
   unsigned int sourcesIPv4;
   unsigned int sourcesIPv6;
   unsigned int destinationsIPv4;
   unsigned int destinationsIPv6;
   cleanAddresses(sourcesIPv4, sourcesIPv6, destinationsIPv4, destinationsIPv6);
   if( (SourceArray.size() < 1) || (DestinationArray.size() < 1) ) {
      HPCT_LOG(fatal) << "At least one source and one destination are needed!";
      return 1;
   }
   if( /* (serviceJitter == false) && */ (servicePing == false) && (serviceTraceroute == false) ) {
      HPCT_LOG(fatal) << "Enable at least on service (Traceroute, Ping, Jitter)!";
      return 1;
   }
   HPCT_LOG(info) << "Addresses:" << "\n"
                  << "* Sources            = " << sourcesIPv4 << " IPv4 / " << sourcesIPv6 << " IPv6\n"
                  << "* Destinations       = " << destinationsIPv4 << " IPv4 / " << destinationsIPv6 << " IPv6";

   std::srand(std::time(nullptr));
#if 0
   jitterParameters.Interval            = std::min(std::max(100ULL, jitterParameters.Interval),        3600U*10000ULL);
   jitterParameters.Expiration          = std::min(std::max(100U, jitterParameters.Expiration),        3600U*10000U);
   jitterParameters.InitialMaxTTL       = std::min(std::max(1U, jitterParameters.InitialMaxTTL),       255U);
   jitterParameters.FinalMaxTTL         = jitterParameters.InitialMaxTTL;
   jitterParameters.IncrementMaxTTL     = 1;
   jitterParameters.Rounds              = std::min(std::max(2U, jitterParameters.Rounds),              1024U);
   jitterParameters.PacketSize          = std::min(65535U, jitterParameters.PacketSize);
#endif
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

#if 0
   if(serviceJitter) {
      HPCT_LOG(info) << "Jitter Service:" << std:: endl
                     << "* Interval           = " << jitterParameters.Interval            << " ms ± "
                        << 100.0 * jitterParameters.Deviation << "%\n"
                     << "* Expiration         = " << jitterParameters.Expiration << " ms" << "\n"
                     << "* Burst              = " << jitterParameters.Rounds              << "\n"
                     << "* TTL                = " << jitterParameters.InitialMaxTTL       << "\n"
                     << "* Packet Size        = " << jitterParameters.PacketSize          << " B\n"
                     << "* Ports              = (none for ICMP) / UDP: "
                        << jitterUDPSourcePort << " -> " << jitterUDPDestinationPort << "\n";
   }
#endif
   if(servicePing) {
      HPCT_LOG(info) << "Ping Service:" << std:: endl
                     << "* Interval           = " << pingParameters.Interval              << " ms ± "
                        << 100.0 * pingParameters.Deviation << "%\n"
                     << "* Expiration         = " << pingParameters.Expiration            << " ms" << "\n"
                     << "* Burst              = " << pingParameters.Rounds                << "\n"
                     << "* TTL                = " << pingParameters.InitialMaxTTL         << "\n"
                     << "* Packet Size        = " << pingParameters.PacketSize            << " B\n"
                     << "* Ports              = (none for ICMP) / UDP: "
                        << pingUDPSourcePort << " -> " << pingUDPDestinationPort << "\n";
   }
   if(serviceTraceroute) {
      HPCT_LOG(info) << "Traceroute Service:" << std:: endl
                     << "* Interval           = " << tracerouteParameters.Interval        << " ms ± "
                        << 100.0 * tracerouteParameters.Deviation << "%\n"
                     << "* Expiration         = " << tracerouteParameters.Expiration      << " ms" << "\n"
                     << "* Rounds             = " << tracerouteParameters.Rounds          << "\n"
                     << "* Initial MaxTTL     = " << tracerouteParameters.InitialMaxTTL   << "\n"
                     << "* Final MaxTTL       = " << tracerouteParameters.FinalMaxTTL     << "\n"
                     << "* Increment MaxTTL   = " << tracerouteParameters.IncrementMaxTTL << "\n"
                     << "* Packet Size        = " << tracerouteParameters.PacketSize      << " B\n"
                     << "* Ports              = (none for ICMP) / UDP: "
                        << tracerouteUDPSourcePort << " -> " << tracerouteUDPDestinationPort << "\n";
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
#if 0
         if(serviceJitter) {
            try {
               ResultsWriter* resultsWriter = nullptr;
               if(!resultsDirectory.empty()) {
                  resultsWriter = ResultsWriter::makeResultsWriter(
                                     ResultsWriterSet, ProgramID, measurementID,
                                     sourceAddress, "Jitter-" + ioModule,
                                     resultsDirectory, resultsTransactionLength, resultsTimestampDepth,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  assert(resultsWriter != nullptr);
               }
               if(ioModule == "UDP") {
                  jitterParameters.SourcePort      = jitterUDPSourcePort;
                  jitterParameters.DestinationPort = jitterUDPDestinationPort;
               }
               else {
                  jitterParameters.SourcePort      = 0;
                  jitterParameters.DestinationPort = 0;
               }
               Service* service = new Jitter(ioModule,
                                             resultsWriter, "Jitter", (OutputFormatVersionType)resultsFormatVersion,
                                             iterations, false,
                                             sourceAddress, destinationsForSource,
                                             jitterParameters,
                                             jitterRecordRawResults);
               ServiceSet.insert(service);
            }
            catch (std::exception& e) {
               HPCT_LOG(fatal) << "Cannot create Jitter service - " << e.what();
               return 1;
            }
         }
#endif
         if(servicePing) {
            try {
               ResultsWriter* resultsWriter = nullptr;
               if(!resultsDirectory.empty()) {
                  resultsWriter = ResultsWriter::makeResultsWriter(
                                     ResultsWriterSet, ProgramID, measurementID,
                                     sourceAddress, "Ping-" + ioModule,
                                     resultsDirectory, resultsTransactionLength, resultsTimestampDepth,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  assert(resultsWriter != nullptr);
               }
               if(ioModule == "UDP") {
                  pingParameters.SourcePort      = pingUDPSourcePort;
                  pingParameters.DestinationPort = pingUDPDestinationPort;
               }
               else {
                  pingParameters.SourcePort      = 0;
                  pingParameters.DestinationPort = 0;
               }
               Service* service = new Ping(ioModule,
                                           resultsWriter, "Ping", (OutputFormatVersionType)resultsFormatVersion,
                                           iterations, false,
                                           sourceAddress, destinationsForSource,
                                           pingParameters);
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
                                     sourceAddress, "Traceroute-" + ioModule,
                                     resultsDirectory, resultsTransactionLength, resultsTimestampDepth,
                                     (pw != nullptr) ? pw->pw_uid : 0, (pw != nullptr) ? pw->pw_gid : 0,
                                     resultsCompression);
                  assert(resultsWriter != nullptr);
               }
               if(ioModule == "UDP") {
                  tracerouteParameters.SourcePort      = tracerouteUDPSourcePort;
                  tracerouteParameters.DestinationPort = tracerouteUDPDestinationPort;
               }
               else {
                  tracerouteParameters.SourcePort      = 0;
                  tracerouteParameters.DestinationPort = 0;
               }
               Service* service = new Traceroute(ioModule,
                                                 resultsWriter, "Traceroute", (OutputFormatVersionType)resultsFormatVersion,
                                                 iterations, false,
                                                 sourceAddress, destinationsForSource,
                                                 tracerouteParameters);
               ServiceSet.insert(service);
            }
            catch (std::exception& e) {
               HPCT_LOG(fatal) << "Cannot create Traceroute service - " << e.what();
               return 1;
            }
         }
      }
   }


   // ====== Prepare service start (before reducing privileges) =============
   for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      if(!service->prepare(true)) {
         HPCT_LOG(fatal) << "Preparing service start failed";
         return 1;
      }
   }


   // ====== Reduce privileges ==============================================
   if(reducePrivileges(pw) == false) {
      HPCT_LOG(fatal) << "Failed to reduce privileges!";
      return 1;
   }


   // ====== Prepare service start (after reducing privileges) ==============
   for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      if(!service->prepare(false)) {
         HPCT_LOG(fatal) << "Preparing service start failed";
         return 1;
      }
      if(service->start() == false) {
         HPCT_LOG(fatal) << "Service start failed";
         return 1;
      }
   }


   // ====== Wait for termination signal ====================================
   Signals.async_wait(signalHandler);
   CleanupTimer.async_wait(tryCleanup);
   IOContext.run();


   // ====== Shut down service threads ======================================
   for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      service->join();
      delete service;
   }
   for(std::set<ResultsWriter*>::iterator resultsWriterIterator = ResultsWriterSet.begin(); resultsWriterIterator != ResultsWriterSet.end(); resultsWriterIterator++) {
      delete *resultsWriterIterator;
   }

   return 0;
}
