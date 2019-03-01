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

#include "ping.h"
#include "tools.h"

#include <netinet/in.h>
#include <netinet/ip.h>

#include <boost/bind.hpp>
#include <boost/format.hpp>


// ###### Constructor #######################################################
Ping::Ping(ResultsWriter*                           resultsWriter,
           const unsigned int                       iterations,
           const bool                               removeDestinationAfterRun,
           const bool                               verboseMode,
           const boost::asio::ip::address&          sourceAddress,
           const std::set<boost::asio::ip::address> destinationAddressArray,
           const unsigned long long                 interval,
           const unsigned int                       expiration,
           const unsigned int                       ttl)
   : Traceroute(resultsWriter, iterations, false, verboseMode,
                sourceAddress, destinationAddressArray,
                interval, expiration, ttl, ttl, ttl)
{
}


// ###### Destructor ########################################################
Ping::~Ping()
{
}


// ###### All requests have received a response #############################
void Ping::noMoreOutstandingRequests()
{
   // Nothing to do for Ping!
}


// ###### Prepare a new run #################################################
bool Ping::prepareRun(const bool newRound)
{
   IterationNumber++;
   if((Iterations > 0) && (IterationNumber > Iterations)) {
       // ====== Done -> exit! ==============================================
       StopRequested = true;
       IOService.stop();
   }
   return(false);   // No scheduling necessary for Ping!
}


// ###### The destination has not been reached with the current TTL #########
bool Ping::notReachedWithCurrentTTL()
{
   // Nothing to do for Ping!
   return(false);
}


// ###### Schedule timeout timer ############################################
void Ping::scheduleTimeoutEvent()
{
   // ====== Schedule event =================================================
   const unsigned long long deviation = std::max(10ULL, Interval / 5ULL);   // 20% deviation
   const unsigned long long duration  = Interval + (std::rand() % deviation);
   TimeoutTimer.expires_from_now(boost::posix_time::milliseconds(duration));
   TimeoutTimer.async_wait(boost::bind(&Ping::handleTimeoutEvent, this,
                                       boost::asio::placeholders::error));

   // ====== Check, whether it is time for starting a new transaction =======
   if(ResultsOutput) {
      ResultsOutput->mayStartNewTransaction();
   }
}


// ###### Comparison function for results output ############################
int Ping::comparePingResults(const ResultEntry* a, const ResultEntry* b)
{
   return(a->address() < b->address());
}


// ###### Process results ###################################################
void Ping::processResults()
{
   // ====== Sort results ===================================================
   std::vector<ResultEntry*> resultsVector;
   for(std::map<unsigned short, ResultEntry>::iterator iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(&iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), &comparePingResults);

   // ====== Process results ================================================
   const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
   for(std::vector<ResultEntry*>::iterator iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;

      // ====== Time-out entries ============================================
      if( (resultEntry->status() == Unknown) &&
          (std::chrono::duration_cast<std::chrono::milliseconds>(now - resultEntry->sendTime()).count() >= Expiration) ) {
         resultEntry->status(Timeout);
         resultEntry->receiveTime(resultEntry->sendTime() + std::chrono::milliseconds(Expiration));
      }

      // ====== Print completed entries =====================================
      if(resultEntry->status() != Unknown) {
         if(VerboseMode) {
            std::cout << *resultEntry << std::endl;
         }

         if(ResultsOutput) {
            ResultsOutput->insert(
               str(boost::format("#P %s %s %x %x %d %d")
                  % SourceAddress.to_string()
                  % resultEntry->address().to_string()
                  % usSinceEpoch(resultEntry->sendTime())
                  % resultEntry->checksum()
                  % resultEntry->status()
                  % std::chrono::duration_cast<std::chrono::microseconds>(resultEntry->receiveTime() - resultEntry->sendTime()).count()
            ));
         }
      }

      // ====== Remove completed entries ====================================
      if(resultEntry->status() != Unknown) {
         assert(ResultsMap.erase(resultEntry->seqNumber()) == 1);
         if(OutstandingRequests > 0) {
            OutstandingRequests--;
         }
      }
   }
}


// ###### Send requests to all destinations #################################
void Ping::sendRequests()
{
   std::lock_guard<std::recursive_mutex> lock(DestinationAddressMutex);

   // ====== Send requests, if there are destination addresses ==============
   if(DestinationAddresses.begin() != DestinationAddresses.end()) {
      // All packets of this request block (for each destination) use the same checksum.
      // The next block of requests may then use another checksum.
      uint32_t targetChecksum = ~0U;
      for(std::set<boost::asio::ip::address>::const_iterator destinationIterator = DestinationAddresses.begin();
          destinationIterator != DestinationAddresses.end(); destinationIterator++) {
         const boost::asio::ip::address& destinationAddress = *destinationIterator;
         sendICMPRequest(destinationAddress, FinalMaxTTL, 0, targetChecksum);
      }

      scheduleTimeoutEvent();
   }

   // ====== No destination addresses -> wait ===============================
   else {
      puts("EMPTY!");
      scheduleIntervalEvent();
   }
}
