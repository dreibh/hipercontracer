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

#include "ping.h"
#include "tools.h"
#include "logger.h"

#include <functional>
#include <boost/format.hpp>


// ###### Constructor #######################################################
Ping::Ping(const std::string                moduleName,
           ResultsWriter*                   resultsWriter,
           const OutputFormatType           outputFormat,
           const unsigned int               iterations,
           const bool                       removeDestinationAfterRun,
           const boost::asio::ip::address&  sourceAddress,
           const std::set<DestinationInfo>& destinationArray,
           const unsigned long long         interval,
           const unsigned int               expiration,
           const unsigned int               rounds,
           const unsigned int               ttl,
           const unsigned int               packetSize,
           const uint16_t                   destinationPort)
   : Traceroute(moduleName,
                resultsWriter, outputFormat, iterations, removeDestinationAfterRun,
                sourceAddress, destinationArray,
                interval, expiration, rounds, ttl, ttl, ttl,
                packetSize, destinationPort),
     PingInstanceName(std::string("Ping(") + sourceAddress.to_string() + std::string(")"))
{
   IOModule->setName(PingInstanceName);
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


// ###### Prepare a new run #################################################
bool Ping::prepareRun(const bool newRound)
{
   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

   IterationNumber++;
   if((Iterations > 0) && (IterationNumber > Iterations)) {
       // ====== Done -> exit! ==============================================
       StopRequested.exchange(true);
       cancelIntervalEvent();
       cancelTimeoutEvent();
       IOModule->cancelSocket();
   }

   RunStartTimeStamp = std::chrono::steady_clock::now();
   return(Destinations.begin() == Destinations.end());
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


// ###### All requests have received a response #############################
void Ping::noMoreOutstandingRequests()
{
   // Nothing to do for Ping!
}


// ###### The destination has not been reached with the current TTL #########
bool Ping::notReachedWithCurrentTTL()
{
   // Nothing to do for Ping!
   return(false);
}


// ###### Send requests to all destinations #################################
void Ping::sendRequests()
{
   if((Iterations == 0) || (IterationNumber <= Iterations)) {
      // All packets in this call use the same checksum.
      // The next sendRequests() call may use a different checksum.
      TargetChecksumArray[0] = IOModule->getIdentifier() ^ SeqNumber;
      for(unsigned int i = 1; i < Rounds; i++) {
         TargetChecksumArray[i] = TargetChecksumArray[0];
      }

      // ====== Send requests, if there are destination addresses ===========
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
      if(Destinations.begin() != Destinations.end()) {
         assert(Rounds > 0);

         for(const DestinationInfo& destination : Destinations) {
            OutstandingRequests +=
               IOModule->sendRequest(destination,
                                     FinalMaxTTL, FinalMaxTTL,
                                     0, Rounds - 1,
                                     SeqNumber, TargetChecksumArray);
         }

         scheduleTimeoutEvent();
      }

      // ====== No destination addresses -> wait ============================
      else {
         scheduleIntervalEvent();
      }
   }
}


// ###### Comparison function for results output ############################
int Ping::comparePingResults(const ResultEntry* a, const ResultEntry* b)
{
   // Ping:
   // The results in ResultsMap are for different destinations.
   // There are different rounds as well.
   // => sort by: destination / round

   // ====== Level 1: Round =================================================
   if(a->destination() < b->destination()) {
      return true;
   }
   else if(a->destination() == b->destination()) {
      // ====== Level 2: Hop ================================================
      if(a->round() < b->round()) {
         return true;
      }
   }
   return false;
}


// ###### Process results ###################################################
void Ping::processResults()
{
   // ====== Sort results ===================================================
   std::vector<ResultEntry*> resultsVector =
      makeSortedResultsVector(&comparePingResults);

   // ====== Process results ================================================
   const ResultTimePoint now = ResultClock::now();
   for(ResultEntry* resultEntry : resultsVector) {

      // ====== Time-out entries ============================================
      if( (resultEntry->status() == Unknown) &&
          (std::chrono::duration_cast<std::chrono::milliseconds>(now - resultEntry->sendTime(TXTimeStampType::TXTST_Application)).count() >= Expiration) ) {
         resultEntry->expire(Expiration);
      }

      // ====== Print completed entries =====================================
      if(resultEntry->status() != Unknown) {
         HPCT_LOG(trace) << getName() << ": " << *resultEntry;
         if(ResultCallback) {
            ResultCallback(this, resultEntry);
         }
         writePingResultEntry(resultEntry);
      }

      // ====== Remove completed entries ====================================
      if(resultEntry->status() != Unknown) {
         const std::size_t elementsErased = ResultsMap.erase(resultEntry->seqNumber());
         assert(elementsErased == 1);
         delete resultEntry;
         if(OutstandingRequests > 0) {
            OutstandingRequests--;
         }
      }
   }

   // ====== Handle "remove destination after run" option ===================
   if(RemoveDestinationAfterRun == true) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
      DestinationIterator = Destinations.begin();
      while(DestinationIterator != Destinations.end()) {
         Destinations.erase(DestinationIterator);
         DestinationIterator = Destinations.begin();
      }
   }
}


// ###### Write Ping result entry to output file ############################
void Ping::writePingResultEntry(const ResultEntry* resultEntry,
                                const char*        indentation)
{
   if(ResultsOutput) {

      // ====== Current output format =======================================
      if(OutputFormat >= OFT_HiPerConTracer_Version2) {
         const unsigned long long sendTimeStamp = nsSinceEpoch<ResultTimePoint>(
            resultEntry->sendTime(TXTimeStampType::TXTST_Application));

         unsigned int timeSourceApplication;
         unsigned int timeSourceQueuing;
         unsigned int timeSourceSoftware;
         unsigned int timeSourceHardware;
         const ResultDuration rttApplication = resultEntry->rtt(RXTimeStampType::RXTST_Application, timeSourceApplication);
         const ResultDuration queuingDelay   = resultEntry->queuingDelay(timeSourceQueuing);
         const ResultDuration rttSoftware    = resultEntry->rtt(RXTimeStampType::RXTST_ReceptionSW, timeSourceSoftware);
         const ResultDuration rttHardware    = resultEntry->rtt(RXTimeStampType::RXTST_ReceptionHW, timeSourceHardware);
         const unsigned int   timeSource     = (timeSourceApplication << 24) |
                                               (timeSourceQueuing     << 16) |
                                               (timeSourceSoftware    << 8) |
                                               timeSourceHardware;

         ResultsOutput->insert(
            str(boost::format("%s#P%c %s %s %x %d %x %d %x %d %08x %d %d %d %d")
               % indentation
               % (unsigned char)IOModule->getProtocolType()

               % resultEntry->sourceAddress().to_string()
               % resultEntry->destinationAddress().to_string()
               % sendTimeStamp
               % resultEntry->round()

               % (unsigned int)resultEntry->destination().trafficClass()
               % resultEntry->packetSize()
               % resultEntry->checksum()
               % resultEntry->status()

               % timeSource
               % std::chrono::duration_cast<std::chrono::nanoseconds>(rttApplication).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(queuingDelay).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(rttSoftware).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(rttHardware).count()
         ));
      }

      // ====== Old output format ===========================================
      else {
         unsigned int timeSource;
         const ResultDuration rtt = resultEntry->obtainMostAccurateRTT(RXTimeStampType::RXTST_ReceptionSW,
                                                                       timeSource);
         const unsigned long long sendTimeStamp = usSinceEpoch<ResultTimePoint>(
            resultEntry->sendTime(TXTimeStampType::TXTST_Application));

         ResultsOutput->insert(
            str(boost::format("#P %s %s %x %x %d %d %x %d %02x")
               % resultEntry->sourceAddress().to_string()
               % resultEntry->destinationAddress().to_string()
               % sendTimeStamp
               % resultEntry->checksum()
               % resultEntry->status()
               % std::chrono::duration_cast<std::chrono::microseconds>(rtt).count()
               % (unsigned int)resultEntry->destination().trafficClass()
               % resultEntry->packetSize()
               % timeSource
         ));
      }

   }
}
