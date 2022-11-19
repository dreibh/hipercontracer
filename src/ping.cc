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
// Copyright (C) 2015-2022 by Thomas Dreibholz
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
#include "logger.h"

#include <functional>
#include <boost/bind.hpp>
#include <boost/format.hpp>


// ###### Constructor #######################################################
Ping::Ping(ResultsWriter*                   resultsWriter,
           const unsigned int               iterations,
           const bool                       removeDestinationAfterRun,
           const boost::asio::ip::address&  sourceAddress,
           const std::set<DestinationInfo>& destinationArray,
           const unsigned long long         interval,
           const unsigned int               expiration,
           const unsigned int               ttl,
           const unsigned int               packetSize)
   : Traceroute(resultsWriter, iterations, removeDestinationAfterRun,
                sourceAddress, destinationArray,
                interval, expiration, ttl, ttl, ttl, 1,
                packetSize),
     PingInstanceName(std::string("Ping(") + sourceAddress.to_string() + std::string(")"))
{
}


// ###### Destructor ########################################################
Ping::~Ping()
{
}


// ###### Start thread ######################################################
const std::string& Ping::getName() const
{
   return PingInstanceName;
}


// ###### All requests have received a response #############################
void Ping::noMoreOutstandingRequests()
{
   // Nothing to do for Ping!
}


// ###### Prepare a new run #################################################
bool Ping::prepareRun(const bool newRound)
{
   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

   IterationNumber++;
   if((Iterations > 0) && (IterationNumber > Iterations)) {
       // ====== Done -> exit! ==============================================
       StopRequested.exchange(true);
       cancelIntervalTimer();
       cancelTimeoutTimer();
       IOModule->cancelSocket();
   }

   RunStartTimeStamp = std::chrono::steady_clock::now();
   return(Destinations.begin() == Destinations.end());
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
   if((Iterations == 0) || (IterationNumber < Iterations)) {
      // Deviate next send interval by 20%, to avoid synchronisation!
      const unsigned long long deviation = std::max(10ULL, Interval / 5ULL);   // 20% deviation
      const unsigned long long duration  = Interval + (std::rand() % deviation);
      TimeoutTimer.expires_from_now(boost::posix_time::milliseconds(duration));
   }
   else {
      // Last ping run: no need to wait for interval, just wait for expiration!
      TimeoutTimer.expires_from_now(boost::posix_time::milliseconds(Expiration));
   }
   TimeoutTimer.async_wait(std::bind(&Ping::handleTimeoutEvent, this,
                                     std::placeholders::_1));

   // ====== Check, whether it is time for starting a new transaction =======
   if(ResultsOutput) {
      ResultsOutput->mayStartNewTransaction();
   }
}


// ###### Comparison function for results output ############################
int Ping::comparePingResults(const ResultEntry* a, const ResultEntry* b)
{
   return(a->destination() < b->destination());
}


// ###### Process results ###################################################
void Ping::processResults()
{
   // ====== Sort results ===================================================
   std::vector<ResultEntry*> resultsVector;
   for(std::map<unsigned short, ResultEntry*>::iterator iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), &comparePingResults);

   // ====== Process results ================================================
   const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
   for(std::vector<ResultEntry*>::iterator iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;

      // ====== Time-out entries ============================================
      if( (resultEntry->status() == Unknown) &&
          (std::chrono::duration_cast<std::chrono::milliseconds>(now - resultEntry->sendTime()).count() >= Expiration) ) {
         resultEntry->setStatus(Timeout);
         resultEntry->setReceiveTime(resultEntry->sendTime() + std::chrono::milliseconds(Expiration));
      }

      // ====== Print completed entries =====================================
      if(resultEntry->status() != Unknown) {
         HPCT_LOG(trace) << getName() << ": " << *resultEntry;

         if(ResultCallback) {
            ResultCallback(this, resultEntry);
         }

         if(ResultsOutput) {
            ResultsOutput->insert(
               str(boost::format("#P %s %s %x %x %d %d %x %d")
                  % SourceAddress.to_string()
                  % resultEntry->destinationAddress().to_string()
                  % usSinceEpoch(resultEntry->sendTime())
                  % resultEntry->checksum()
                  % resultEntry->status()
                  % std::chrono::duration_cast<std::chrono::microseconds>(resultEntry->receiveTime() - resultEntry->sendTime()).count()
                  % (unsigned int)resultEntry->destination().trafficClass()
                  % resultEntry->packetSize()
            ));
         }
      }

      // ====== Remove completed entries ====================================
      if(resultEntry->status() != Unknown) {
         assert(ResultsMap.erase(resultEntry->seqNumber()) == 1);
         delete resultEntry;
         if(OutstandingRequests > 0) {
            OutstandingRequests--;
         }
      }
   }


   if(RemoveDestinationAfterRun == true) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
      DestinationIterator = Destinations.begin();
      while(DestinationIterator != Destinations.end()) {
         Destinations.erase(DestinationIterator);
         DestinationIterator = Destinations.begin();
      }
   }
}


// ###### Send requests to all destinations #################################
void Ping::sendRequests()
{
   if((Iterations == 0) || (IterationNumber <= Iterations)) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

      // ====== Send requests, if there are destination addresses ===========
      if(Destinations.begin() != Destinations.end()) {
         // All packets of this request block (for each destination) use the same checksum.
         // The next block of requests may then use another checksum.
         uint32_t targetChecksum = ~0U;
         for(std::set<DestinationInfo>::const_iterator destinationIterator = Destinations.begin();
            destinationIterator != Destinations.end(); destinationIterator++) {
            const DestinationInfo& destination = *destinationIterator;
            ResultEntry* resultEntry =
               IOModule->sendRequest(destination, FinalMaxTTL, 0, SeqNumber, targetChecksum);
            if(resultEntry) {
               OutstandingRequests++;
            }
         }

         scheduleTimeoutEvent();
      }

      // ====== No destination addresses -> wait ============================
      else {
         scheduleIntervalEvent();
      }
   }
}
