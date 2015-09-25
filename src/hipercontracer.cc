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
// Copyright (C) 2015 by Thomas Dreibholz
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

#include "service.h"
#include "traceroute.h"
#include "ping.h"


static std::set<boost::asio::ip::address> sourceArray;
static std::set<boost::asio::ip::address> destinationArray;


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


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   enum ServiceType {
      TST_Ping       = 1,
      TST_Traceroute = 2
   };
   ServiceType        serviceType               = TST_Traceroute;

   unsigned long long tracerouteInterval        = 10000;
   unsigned int       tracerouteExpiration      = 3000;
   unsigned int       tracerouteInitialMaxTTL   = 6;
   unsigned int       tracerouteFinalMaxTTL     = 36;
   unsigned int       tracerouteIncrementMaxTTL = 6;

   unsigned long long pingInterval              = 1000;
   unsigned int       pingExpiration            = 30000;
   unsigned int       pingTTL                   = 64;


   // ====== Handle arguments ===============================================
   for(int i = 1; i < argc; i++) {
      if(strncmp(argv[i], "-source=", 8) == 0) {
         addAddress(sourceArray, (const char*)&argv[i][8]);
      }
      else if(strncmp(argv[i], "-destination=", 13) == 0) {
         addAddress(destinationArray, (const char*)&argv[i][13]);
      }
      else if(strcmp(argv[i], "-ping") == 0) {
         serviceType = TST_Ping;
      }
      else if(strcmp(argv[i], "-traceroute") == 0) {
         serviceType = TST_Traceroute;
      }
      else if(strncmp(argv[i], "-tracerouteinterval=", 20) == 0) {
         pingInterval = atol((const char*)&argv[i][20]);
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
      else if(strncmp(argv[i], "-tracerouteincrementmaxttl=", 28) == 0) {
         tracerouteIncrementMaxTTL = atol((const char*)&argv[i][28]);
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
      else {
         std::cerr << "Usage: " << argv[0] << " -source=source ... -destination=destination ..." << std::endl;
      }
   }


   // ====== Initialize =====================================================
   if( (sourceArray.size() < 1) ||
       (destinationArray.size() < 1) ) {
      std::cerr << "ERROR: At least one source and destination are needed!" << std::endl;
      ::exit(1);
   }

   std::srand(std::time(0));
   tracerouteInterval        = std::min(std::max(1000ULL, tracerouteInterval),   3600U*60000ULL);
   tracerouteExpiration        = std::min(std::max(1000U, tracerouteExpiration),     60000U);
   tracerouteInitialMaxTTL   = std::min(std::max(1U, tracerouteInitialMaxTTL),   255U);
   tracerouteFinalMaxTTL     = std::min(std::max(1U, tracerouteFinalMaxTTL),     255U);
   tracerouteIncrementMaxTTL = std::min(std::max(1U, tracerouteIncrementMaxTTL), 255U);
   pingInterval              = std::min(std::max(100ULL, pingInterval),          3600U*60000ULL);
   pingExpiration            = std::min(std::max(100U, pingExpiration),          3600U*60000U);
   pingTTL                   = std::min(std::max(1U, pingTTL),                   255U);

   switch(serviceType) {
      case TST_Ping:
         std::cout << "Traceroute Service:" << std:: endl
               << "* Interval   = " << pingInterval   << " ms" << std::endl
               << "* Expiration = " << pingExpiration << " ms" << std::endl
               << "* TTL        = " << pingTTL        << std::endl
               << std::endl;
       break;
      case TST_Traceroute:
         std::cout << "Traceroute Service:" << std:: endl
               << "* Interval         = " << tracerouteInterval        << " ms" << std::endl
               << "* Expiration       = " << tracerouteExpiration      << " ms" << std::endl
               << "* Initial MaxTTL   = " << tracerouteInitialMaxTTL   << std::endl
               << "* Final MaxTTL     = " << tracerouteFinalMaxTTL     << std::endl
               << "* Increment MaxTTL = " << tracerouteIncrementMaxTTL << std::endl
               << std::endl;
       break;
   }


   // ====== Start service threads ==========================================
   std::set<Service*> serviceSet;
   for(std::set<boost::asio::ip::address>::iterator sourceIterator = sourceArray.begin(); sourceIterator != sourceArray.end(); sourceIterator++) {
       Service* service = NULL;
       switch(serviceType) {
          case TST_Ping:
             service = new Ping(*sourceIterator, destinationArray,
                                pingInterval, pingExpiration, pingTTL);
           break;
          case TST_Traceroute:
             service = new Traceroute(*sourceIterator, destinationArray,
                                      tracerouteInterval, tracerouteExpiration,
                                      tracerouteInitialMaxTTL, tracerouteFinalMaxTTL, tracerouteIncrementMaxTTL);
           break;
       }
       if(service->start() == false) {
          std::cerr << "exiting!" << std::endl;
          ::exit(1);
       }
       serviceSet.insert(service);
   }


   // ====== Wait for termination signal ====================================
   boost::asio::io_service ioService;
   boost::asio::signal_set signals(ioService, SIGINT, SIGTERM);
   signals.async_wait(signalHandler);
   ioService.run();
   std::cout << std::endl << "*** Shutting down! ***" << std::endl;


   // ====== Shut down service threads ======================================
   for(std::set<Service*>::iterator serviceIterator = serviceSet.begin(); serviceIterator != serviceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      service->requestStop();
   }
   for(std::set<Service*>::iterator serviceIterator = serviceSet.begin(); serviceIterator != serviceSet.end(); serviceIterator++) {
      Service* service = *serviceIterator;
      service->join();
   }

   return(0);
}
