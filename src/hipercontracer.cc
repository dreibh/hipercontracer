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
// #include <boost/program_options/cmdline.hpp>
#include <boost/asio/ip/address.hpp>

#include "tools.h"
#include "logger.h"
#include "resultswriter.h"
#include "service.h"
#include "traceroute.h"
#include "ping.h"


static std::map<boost::asio::ip::address, std::set<uint8_t>> SourceArray;
static std::set<boost::asio::ip::address>                    DestinationArray;
static std::set<ResultsWriter*>                              ResultsWriterSet;
static std::set<Service*>                                    ServiceSet;
static boost::asio::io_service                               IOService;
static boost::asio::signal_set                               Signals(IOService, SIGINT, SIGTERM);
static boost::posix_time::milliseconds                       CleanupTimerInterval(250);
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



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
//    unsigned int       logLevel                  = boost::log::trivial::severity_level::info;
//    const char*        user                      = NULL;
//    bool               servicePing               = false;
//    bool               serviceTraceroute         = false;
//    unsigned int       iterations                = 0;
//
//    unsigned long long tracerouteInterval        = 10000;
//    unsigned int       tracerouteExpiration      = 3000;
//    unsigned int       tracerouteRounds          = 1;
//    unsigned int       tracerouteInitialMaxTTL   = 6;
//    unsigned int       tracerouteFinalMaxTTL     = 36;
//    unsigned int       tracerouteIncrementMaxTTL = 6;

//    unsigned long long pingInterval              = 1000;
//    unsigned int       pingExpiration            = 30000;
//    unsigned int       pingTTL                   = 64;

//    unsigned int       resultsTransactionLength  = 60;
//    std::string        resultsDirectory;

   // ====== Handle arguments ===============================================
//    for(int i = 1; i < argc; i++) {
//       if(strncmp(argv[i], "-source=", 8) == 0) {
//          addSourceAddress(SourceArray, (const char*)&argv[i][8]);
//       }
//       else if(strncmp(argv[i], "-destination=", 13) == 0) {
//          addDestinationAddress(DestinationArray, (const char*)&argv[i][13]);
//       }
//       else if(strcmp(argv[i], "-ping") == 0) {
//          servicePing = true;
//       }
//       else if(strcmp(argv[i], "-traceroute") == 0) {
//          serviceTraceroute = true;
//       }
//       else if(strncmp(argv[i], "-loglevel=", 10) == 0) {
//          logLevel = std::strtoul((const char*)&argv[i][10], NULL, 10);
//       }
//       else if(strcmp(argv[i], "-quiet") == 0) {
//          logLevel = boost::log::trivial::severity_level::warning;
//       }
//       else if(strcmp(argv[i], "-verbose") == 0) {
//          logLevel = boost::log::trivial::severity_level::trace;
//       }
//       else if(strncmp(argv[i], "-user=", 6) == 0) {
//          user = (const char*)&argv[i][6];
//       }
//       else if(strncmp(argv[i], "-iterations=", 12) == 0) {
//          iterations = std::strtoul((const char*)&argv[i][12], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-tracerouteinterval=", 20) == 0) {
//          tracerouteInterval = std::strtoul((const char*)&argv[i][20], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-tracerouteduration=", 20) == 0) {
//          tracerouteExpiration = std::strtoul((const char*)&argv[i][20], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-tracerouterounds=", 18) == 0) {
//          tracerouteRounds = std::strtoul((const char*)&argv[i][18], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-tracerouteinitialmaxttl=", 25) == 0) {
//          tracerouteInitialMaxTTL = std::strtoul((const char*)&argv[i][25], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-traceroutefinalmaxttl=", 23) == 0) {
//          tracerouteFinalMaxTTL = std::strtoul((const char*)&argv[i][23], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-tracerouteincrementmaxttl=", 27) == 0) {
//          tracerouteIncrementMaxTTL = std::strtoul((const char*)&argv[i][27], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-pinginterval=", 14) == 0) {
//          pingInterval = std::strtoul((const char*)&argv[i][14], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-pingexpiration=", 16) == 0) {
//          pingExpiration = std::strtoul((const char*)&argv[i][16], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-pingttl=", 9) == 0) {
//          pingTTL = std::strtoul((const char*)&argv[i][9], NULL, 10);
//       }
//       else if(strncmp(argv[i], "-resultsdirectory=", 18) == 0) {
//          resultsDirectory = (const char*)&argv[i][18];
//       }
//       else if(strncmp(argv[i], "-resultstransactionlength=", 26) == 0) {
//          resultsTransactionLength = std::strtoul((const char*)&argv[i][26], NULL, 10);
//       }
//       else if(strcmp(argv[i], "--") == 0) {
//       }
//       else {
//          std::cerr << "ERROR: Unknown parameter " << argv[i] << std::endl
//                    << "Usage: " << argv[0] << " -source=source ... -destination=destination ..." << std::endl;
//          return(1);
//       }
//    }

   unsigned int       logLevel;
   std::string        user;
   std::string        configurationFileName;
   bool               servicePing;
   bool               serviceTraceroute;
   unsigned int       iterations;

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
           boost::program_options::value<unsigned int>(&logLevel)->default_value(boost::log::trivial::severity_level::trace),
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
      ( "iterations",
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

      ( "pinginterval",
           boost::program_options::value<unsigned long long>(&pingInterval)->default_value(1000),
           "Ping interval in ms" )
      ( "pingexpiration",
           boost::program_options::value<unsigned int>(&pingExpiration)->default_value(30000),
           "Ping expiration timeout in ms" )
      ( "pingttl",
           boost::program_options::value<unsigned int>(&pingTTL)->default_value(64),
           "Ping initial maximum TTL value" )

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
   if( (SourceArray.size() < 1) || (DestinationArray.size() < 1) ) {
      HPCT_LOG(fatal) << "ERROR: At least one source and one destination are needed!";
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
                                           ResultsWriterSet, sourceAddress, "Ping",
                                           resultsDirectory, resultsTransactionLength,
                                           (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                        iterations, false,
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
                                                 ResultsWriterSet, sourceAddress, "Traceroute",
                                                 resultsDirectory, resultsTransactionLength,
                                                 (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                              iterations, false,
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

   return(0);
}
