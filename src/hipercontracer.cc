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
// Copyright (C) 2015-2016 by Thomas Dreibholz
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

#include <set>
#include <algorithm>

#include <boost/asio/basic_signal_set.hpp>

#include "sqlwriter.h"
#include "service.h"
#include "traceroute.h"
#include "ping.h"


static std::set<boost::asio::ip::address> SourceArray;
static std::set<boost::asio::ip::address> DestinationArray;
static std::set<SQLWriter*>               SqlWriterSet;
static std::set<Service*>                 ServiceSet;


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
void signalHandler(const boost::system::error_code& error, int signal_number)
{
   // Nothing to do here!
}


// ###### Prepare SQL writer ################################################
static SQLWriter* makeSQLWriter(const boost::asio::ip::address& sourceAddress,
                                const std::string&              sqlTable,
                                const std::string&              sqlDirectory,
                                const unsigned int              sqlTransactionLength)
{
   SQLWriter* sqlWriter = NULL;
   if(!sqlTable.empty()) {
      std::string uniqueID =
         sqlTable + "-" +
         sourceAddress.to_string() + "-" +
         boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::universal_time());
      replace(uniqueID.begin(), uniqueID.end(), ' ', '-');
      sqlWriter = new SQLWriter(sqlDirectory, uniqueID, sqlTable, sqlTransactionLength);
      if(sqlWriter->prepare() == false) {
         ::exit(1);
      }
      SqlWriterSet.insert(sqlWriter);
   }
   return(sqlWriter);
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   bool               servicePing               = false;
   bool               serviceTraceroute         = false;

   unsigned long long tracerouteInterval        = 10000;
   unsigned int       tracerouteExpiration      = 3000;
   unsigned int       tracerouteInitialMaxTTL   = 6;
   unsigned int       tracerouteFinalMaxTTL     = 36;
   unsigned int       tracerouteIncrementMaxTTL = 6;

   unsigned long long pingInterval              = 1000;
   unsigned int       pingExpiration            = 30000;
   unsigned int       pingTTL                   = 64;

   bool               verboseMode               = true;
   unsigned int       sqlTransactionLength      = 60;
   std::string        sqlDirectory              = "hipercontracer-output";
   std::string        sqlPingTable;
   std::string        sqlTracerouteTable;

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
      else if(strncmp(argv[i], "-tracerouteinterval=", 20) == 0) {
         tracerouteInterval = atol((const char*)&argv[i][20]);
      }
      else if(strncmp(argv[i], "-tracerouteduration=", 20) == 0) {
         tracerouteExpiration = atol((const char*)&argv[i][20]);
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
      else if(strncmp(argv[i], "-sqlpingtable=", 14) == 0) {
         sqlPingTable = (const char*)&argv[i][14];
      }
      else if(strncmp(argv[i], "-sqltraceroutetable=", 20) == 0) {
         sqlTracerouteTable = (const char*)&argv[i][20];
      }
      else if(strncmp(argv[i], "-sqldirectory=", 14) == 0) {
         sqlDirectory = (const char*)&argv[i][14];
      }
      else if(strncmp(argv[i], "-sqltransactionlength=", 22) == 0) {
         sqlTransactionLength = atol((const char*)&argv[i][22]);
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

   std::srand(std::time(0));
   tracerouteInterval        = std::min(std::max(1000ULL, tracerouteInterval),   3600U*60000ULL);
   tracerouteExpiration      = std::min(std::max(1000U, tracerouteExpiration),   60000U);
   tracerouteInitialMaxTTL   = std::min(std::max(1U, tracerouteInitialMaxTTL),   255U);
   tracerouteFinalMaxTTL     = std::min(std::max(1U, tracerouteFinalMaxTTL),     255U);
   tracerouteIncrementMaxTTL = std::min(std::max(1U, tracerouteIncrementMaxTTL), 255U);
   pingInterval              = std::min(std::max(100ULL, pingInterval),          3600U*60000ULL);
   pingExpiration            = std::min(std::max(100U, pingExpiration),          3600U*60000U);
   pingTTL                   = std::min(std::max(1U, pingTTL),                   255U);

   std::cout << "SQL Output:" << std::endl;
   if( (!sqlPingTable.empty()) || (!sqlTracerouteTable.empty()) ) {
      std::cout << "* SQL Directory      = " << sqlDirectory         << std::endl
                << "* Transaction Length = " << sqlTransactionLength << " s" << std::endl;
   }
   else {
      std::cout << "-- turned off--" << std::endl;
   }
   if(servicePing) {
      std::cout << "Ping Service:" << std:: endl
                << "* Interval           = " << pingInterval   << " ms" << std::endl
                << "* Expiration         = " << pingExpiration << " ms" << std::endl
                << "* TTL                = " << pingTTL        << std::endl
                << "* Ping Table         = " << sqlPingTable   << std::endl;
   }
   if(serviceTraceroute) {
      std::cout << "Traceroute Service:" << std:: endl
                << "* Interval           = " << tracerouteInterval        << " ms" << std::endl
                << "* Expiration         = " << tracerouteExpiration      << " ms" << std::endl
                << "* Initial MaxTTL     = " << tracerouteInitialMaxTTL   << std::endl
                << "* Final MaxTTL       = " << tracerouteFinalMaxTTL     << std::endl
                << "* Increment MaxTTL   = " << tracerouteIncrementMaxTTL << std::endl
                << "* Traceroute Table   = " << sqlTracerouteTable        << std::endl;
   }


   // ====== Start service threads ==========================================
   for(std::set<boost::asio::ip::address>::iterator sourceIterator = SourceArray.begin(); sourceIterator != SourceArray.end(); sourceIterator++) {
      if(servicePing) {
         try {
            Service* service = new Ping(makeSQLWriter(*sourceIterator, sqlPingTable, sqlDirectory, sqlTransactionLength),
                                        verboseMode,
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
            Service* service = new Traceroute(makeSQLWriter(*sourceIterator, sqlTracerouteTable, sqlDirectory, sqlTransactionLength),
                                              verboseMode,
                                              *sourceIterator, DestinationArray,
                                              tracerouteInterval, tracerouteExpiration,
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


   // ====== Wait for termination signal ====================================
   boost::asio::io_service ioService;
   boost::asio::signal_set signals(ioService, SIGINT, SIGTERM);
   signals.async_wait(signalHandler);
   ioService.run();
   puts("\n*** Shutting down! ***\n");   // Avoids a false positive from Helgrind.


   // ====== Shut down service threads ======================================
   for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      service->requestStop();
   }
   for(std::set<Service*>::iterator serviceIterator = ServiceSet.begin(); serviceIterator != ServiceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      service->join();
      delete service;
   }
   for(std::set<SQLWriter*>::iterator sqlWriterIterator = SqlWriterSet.begin(); sqlWriterIterator != SqlWriterSet.end(); sqlWriterIterator++) {
      delete *sqlWriterIterator;
   }

   return(0);
}
