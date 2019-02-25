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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>

#include <netinet/ip.h>

#include <set>
#include <algorithm>

#include <boost/asio/basic_signal_set.hpp>

#include "resultswriter.h"
#include "service.h"
#include "traceroute.h"
#include "ping.h"


static std::set<boost::asio::ip::address> SourceArray;
static std::set<boost::asio::ip::address> DestinationArray;
static std::set<ResultsWriter*>           ResultsWriterSet;
static std::set<Service*>                 ServiceSet;
static boost::asio::io_service            IOService;
static boost::asio::signal_set            Signals(IOService, SIGINT, SIGTERM);
static boost::posix_time::milliseconds    CleanupTimerInterval(250);
static boost::asio::deadline_timer        CleanupTimer(IOService, CleanupTimerInterval);


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


// ###### Prepare results writer ############################################
static ResultsWriter* makeResultsWriter(const boost::asio::ip::address& sourceAddress,
                                        const std::string&              resultsFormat,
                                        const std::string&              resultsDirectory,
                                        const unsigned int              resultsTransactionLength,
                                        const uid_t                     uid,
                                        const gid_t                     gid)
{
   ResultsWriter* resultsWriter = NULL;
   if(!resultsDirectory.empty()) {
      std::string uniqueID =
         resultsFormat + "-" +
         sourceAddress.to_string() + "-" +
         boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::universal_time());
      replace(uniqueID.begin(), uniqueID.end(), ' ', '-');
      resultsWriter = new ResultsWriter(resultsDirectory, uniqueID, resultsFormat, resultsTransactionLength,
                                        uid, gid);
      if(resultsWriter->prepare() == false) {
         ::exit(1);
      }
      ResultsWriterSet.insert(resultsWriter);
   }
   return(resultsWriter);
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   const char*        user                      = NULL;
   passwd*            pw                        = NULL;
   bool               servicePing               = false;
   bool               serviceTraceroute         = false;
   unsigned int       iterations                = 0;

   unsigned long long tracerouteInterval        = 10000;
   unsigned int       tracerouteExpiration      = 3000;
   unsigned int       tracerouteRounds          = 1;
   unsigned int       tracerouteInitialMaxTTL   = 6;
   unsigned int       tracerouteFinalMaxTTL     = 36;
   unsigned int       tracerouteIncrementMaxTTL = 6;

   unsigned long long pingInterval              = 1000;
   unsigned int       pingExpiration            = 30000;
   unsigned int       pingTTL                   = 64;

   bool               verboseMode               = true;
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
      else if(strcmp(argv[i], "-quiet") == 0) {
         verboseMode = false;
      }
      else if(strcmp(argv[i], "-verbose") == 0) {
         verboseMode = true;
      }
      else if(strncmp(argv[i], "-iterations=", 12) == 0) {
         iterations = atol((const char*)&argv[i][12]);
      }
      else if(strncmp(argv[i], "-user=", 6) == 0) {
         user = (const char*)&argv[i][6];
      }
      else if(strncmp(argv[i], "-tracerouteinterval=", 20) == 0) {
         tracerouteInterval = atol((const char*)&argv[i][20]);
      }
      else if(strncmp(argv[i], "-tracerouteduration=", 20) == 0) {
         tracerouteExpiration = atol((const char*)&argv[i][20]);
      }
      else if(strncmp(argv[i], "-tracerouterounds=", 18) == 0) {
         tracerouteRounds = atol((const char*)&argv[i][18]);
      }
      else if(strncmp(argv[i], "-tracerouteinitialmaxttl=", 25) == 0) {
         tracerouteInitialMaxTTL = atol((const char*)&argv[i][25]);
      }
      else if(strncmp(argv[i], "-traceroutefinalmaxttl=", 23) == 0) {
         tracerouteFinalMaxTTL = atol((const char*)&argv[i][23]);
      }
      else if(strncmp(argv[i], "-tracerouteincrementmaxttl=", 27) == 0) {
         tracerouteIncrementMaxTTL = atol((const char*)&argv[i][27]);
      }
      else if(strncmp(argv[i], "-pinginterval=", 14) == 0) {
         pingInterval = atol((const char*)&argv[i][14]);
      }
      else if(strncmp(argv[i], "-pingexpiration=", 16) == 0) {
         pingExpiration = atol((const char*)&argv[i][16]);
      }
      else if(strncmp(argv[i], "-pingttl=", 9) == 0) {
         pingTTL = atol((const char*)&argv[i][9]);
      }
      else if(strncmp(argv[i], "-resultsdirectory=", 18) == 0) {
         resultsDirectory = (const char*)&argv[i][18];
      }
      else if(strncmp(argv[i], "-resultstransactionlength=", 26) == 0) {
         resultsTransactionLength = atol((const char*)&argv[i][26]);
      }
      else if(strcmp(argv[i], "--") == 0) {
      }
      else {
         std::cerr << "ERROR: Unknown parameter " << argv[i] << std::endl
                   << "Usage: " << argv[0] << " -source=source ... -destination=destination ..." << std::endl;
         return(1);
      }
   }


   // ====== Initialize =====================================================
   if( (SourceArray.size() < 1) ||
       (DestinationArray.size() < 1) ) {
      std::cerr << "ERROR: At least one source and destination are needed!" << std::endl;
      return(1);
   }
   if((servicePing == false) && (serviceTraceroute == false)) {
      std::cerr << "ERROR: Enable at least on service (Ping or Traceroute)!" << std::endl;
      return(1);
   }
   if(user != NULL) {
      pw = getpwnam(user);
      if(pw == NULL) {
         pw = getpwuid(atoi(user));
         if(pw == NULL) {
            std::cerr << "ERROR: Provided user " << user << " is not a user name or UID!" << std::endl;
            return(1);
         }
      }
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

   std::cout << "Results Output:" << std::endl;
   if(!resultsDirectory.empty()) {
      std::cout << "* Results Directory  = " << resultsDirectory         << std::endl
                << "* Transaction Length = " << resultsTransactionLength << " s" << std::endl;
   }
   else {
      std::cout << "-- turned off--" << std::endl;
   }
   if(servicePing) {
      std::cout << "Ping Service:" << std:: endl
                << "* Interval           = " << pingInterval   << " ms" << std::endl
                << "* Expiration         = " << pingExpiration << " ms" << std::endl
                << "* TTL                = " << pingTTL        << std::endl;
   }
   if(serviceTraceroute) {
      std::cout << "Traceroute Service:" << std:: endl
                << "* Interval           = " << tracerouteInterval        << " ms" << std::endl
                << "* Expiration         = " << tracerouteExpiration      << " ms" << std::endl
                << "* Rounds             = " << tracerouteRounds          << std::endl
                << "* Initial MaxTTL     = " << tracerouteInitialMaxTTL   << std::endl
                << "* Final MaxTTL       = " << tracerouteFinalMaxTTL     << std::endl
                << "* Increment MaxTTL   = " << tracerouteIncrementMaxTTL << std::endl;
   }


   // ====== Start service threads ==========================================
   for(std::set<boost::asio::ip::address>::iterator sourceIterator = SourceArray.begin(); sourceIterator != SourceArray.end(); sourceIterator++) {
      if(servicePing) {
         try {
            Service* service = new Ping(makeResultsWriter(*sourceIterator, "Ping", resultsDirectory, resultsTransactionLength,
                                                          (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                        iterations, verboseMode,
                                        *sourceIterator, DestinationArray,
                                        pingInterval, pingExpiration, pingTTL);
            if(service->start() == false) {
               ::exit(1);
            }
            ServiceSet.insert(service);
         }
         catch (std::exception& e) {
            std::cerr << "ERROR: Cannot create Ping service - " << e.what() << std::endl;
            ::exit(1);
         }
      }
      if(serviceTraceroute) {
         try {
            Service* service = new Traceroute(makeResultsWriter(*sourceIterator, "Traceroute", resultsDirectory, resultsTransactionLength,
                                                                (pw != NULL) ? pw->pw_uid : 0, (pw != NULL) ? pw->pw_gid : 0),
                                              iterations, verboseMode,
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
            std::cerr << "ERROR: Cannot create Traceroute service - " << e.what() << std::endl;
            ::exit(1);
         }
       }
   }


   // ====== Reduce permissions =============================================
   if((pw != NULL) && (pw->pw_uid != 0)) {
      if(verboseMode) {
         std::cerr << "NOTE: Using UID " << pw->pw_uid
                   << ", GID " << pw->pw_gid << std::endl;
      }
      if(setgid(pw->pw_gid) != 0) {
         std::cerr << "ERROR: setgid(" << pw->pw_gid << ") failed: " << strerror(errno) << std::endl;
         ::exit(1);
      }
      if(setuid(pw->pw_uid) != 0) {
         std::cerr << "ERROR: setuid(" << pw->pw_uid << ") failed: " << strerror(errno) << std::endl;
         ::exit(1);
      }
   }
   else {
      std::cerr << "NOTE: Working as root (uid 0). This is not recommended!" << std::endl;
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
