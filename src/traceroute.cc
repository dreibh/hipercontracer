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

#include "traceroute.h"
#include "tools.h"
#include "logger.h"
#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"

#include <netinet/in.h>
#include <netinet/ip.h>

#include <exception>
#include <functional>
#include <boost/format.hpp>
#include <boost/version.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#if BOOST_VERSION >= 106600
#include <boost/uuid/detail/sha1.hpp>
#else
#include <boost/uuid/sha1.hpp>
#endif

#ifdef __linux__
#include <linux/sockios.h>
#endif



// ###### Constructor #######################################################
Traceroute::Traceroute(const std::string                moduleName,
                       ResultsWriter*                   resultsWriter,
                       const OutputFormatType           outputFormat,
                       const unsigned int               iterations,
                       const bool                       removeDestinationAfterRun,
                       const boost::asio::ip::address&  sourceAddress,
                       const std::set<DestinationInfo>& destinationArray,
                       const unsigned long long         interval,
                       const unsigned int               expiration,
                       const unsigned int               rounds,
                       const unsigned int               initialMaxTTL,
                       const unsigned int               finalMaxTTL,
                       const unsigned int               incrementMaxTTL,
                       const unsigned int               packetSize,
                       const uint16_t                   destinationPort)
   : TracerouteInstanceName(std::string("Traceroute(") + sourceAddress.to_string() + std::string(")")),
     ResultsOutput(resultsWriter),
     OutputFormat(outputFormat),
     Iterations(iterations),
     RemoveDestinationAfterRun(removeDestinationAfterRun),
     Interval(interval),
     Expiration(expiration),
     Rounds(rounds),
     InitialMaxTTL(initialMaxTTL),
     FinalMaxTTL(finalMaxTTL),
     IncrementMaxTTL(incrementMaxTTL),
     IOService(),
     SourceAddress(sourceAddress),
     TimeoutTimer(IOService),
     IntervalTimer(IOService)
{
   // ====== Some initialisations ===========================================
   IOModule = IOModuleBase::createIOModule(
                 moduleName,
                 IOService, ResultsMap, SourceAddress,
                 std::bind(&Traceroute::newResult, this, std::placeholders::_1),
                 packetSize, destinationPort);
   if(IOModule == nullptr) {
      throw std::runtime_error("Unable to initialise IO module for " + moduleName);
   }
   IOModule->setName(TracerouteInstanceName);
   SeqNumber           = (unsigned short)(std::rand() & 0xffff);
   OutstandingRequests = 0;
   LastHop             = 0xffffffff;
   IterationNumber     = 0;
   MinTTL              = 1;
   MaxTTL              = InitialMaxTTL;
   TargetChecksumArray = new uint32_t[Rounds];
   assert(TargetChecksumArray != nullptr);
   StopRequested.exchange(false);

   // ====== Prepare destination endpoints ==================================
   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
   for(std::set<DestinationInfo>::const_iterator destinationIterator = destinationArray.begin();
       destinationIterator != destinationArray.end(); destinationIterator++) {
      const DestinationInfo& destination = *destinationIterator;
      if(destination.address().is_v6() == SourceAddress.is_v6()) {
         Destinations.insert(destination);
      }
   }
   DestinationIterator = Destinations.end();
}


// ###### Destructor ########################################################
Traceroute::~Traceroute()
{
   std::map<unsigned short, ResultEntry*>::iterator iterator = ResultsMap.begin();
   while(iterator != ResultsMap.end()) {
      delete iterator->second;
      iterator = ResultsMap.erase(iterator);
   }
   delete IOModule;
   IOModule = nullptr;
   delete [] TargetChecksumArray;
   TargetChecksumArray = nullptr;
}


// ###### Get source address ################################################
const boost::asio::ip::address& Traceroute::getSource()
{
    return(SourceAddress);
}


// ###### Add destination address ###########################################
bool Traceroute::addDestination(const DestinationInfo& destination)
{
   if(destination.address().is_v6() == SourceAddress.is_v6()) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
      const std::set<DestinationInfo>::const_iterator destinationIterator =
         Destinations.find(destination);

      if(destinationIterator == Destinations.end()) {
         if(DestinationIterator == Destinations.end()) {
            // Address will be the first destination in list -> abort interval timer
            IntervalTimer.expires_from_now(boost::posix_time::milliseconds(0));
            IntervalTimer.async_wait(std::bind(&Traceroute::handleIntervalEvent, this,
                                               std::placeholders::_1));
         }
         Destinations.insert(destination);
         return true;
      }

      // Already there -> nothing to do.
   }
   return false;
}


// ###### Start thread ######################################################
const std::string& Traceroute::getName() const
{
   return TracerouteInstanceName;
}


// ###### Start thread ######################################################
bool Traceroute::start()
{
   if(IOModule->prepareSocket()) {
      StopRequested.exchange(false);
      Thread = std::thread(&Traceroute::run, this);
      return true;
   }
   return false;
}


// ###### Request stop of thread ############################################
void Traceroute::requestStop() {
   StopRequested.exchange(true);
   IOService.post(std::bind(&Traceroute::cancelIntervalEvent, this));
   IOService.post(std::bind(&Traceroute::cancelTimeoutEvent, this));
   IOService.post(std::bind(&IOModuleBase::cancelSocket, IOModule));
}


// ###### Join thread #######################################################
void Traceroute::join()
{
   requestStop();
   Thread.join();
   StopRequested.exchange(false);
}


// ###### Is thread joinable? ###############################################
bool Traceroute::joinable()
{
   // Joinable, if stop is requested *and* the thread is joinable!
   return ((StopRequested == true) && Thread.joinable());
}


// ###### Prepare a new run #################################################
bool Traceroute::prepareRun(const bool newRound)
{
   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

   if(newRound) {
      IterationNumber++;

      // ====== Rewind ======================================================
      DestinationIterator = Destinations.begin();
      for(unsigned int i = 0; i < Rounds; i++) {
         TargetChecksumArray[i] = ~0U;   // Use a new target checksum!
      }
   }
   else {
      // ====== Get next destination address ================================
      if(DestinationIterator != Destinations.end()) {
         if(RemoveDestinationAfterRun == false) {
            DestinationIterator++;
         }
         else {
            std::set<DestinationInfo>::iterator toBeDeleted = DestinationIterator;
            DestinationIterator++;
            HPCT_LOG(debug) << getName() << ": Removing " << *toBeDeleted;
            Destinations.erase(toBeDeleted);
         }
      }
   }

   // ====== Clear results ==================================================
   std::map<unsigned short, ResultEntry*>::iterator iterator = ResultsMap.begin();
   while(iterator != ResultsMap.end()) {
      delete iterator->second;
      iterator = ResultsMap.erase(iterator);
   }
   MinTTL              = 1;
   MaxTTL              = (DestinationIterator != Destinations.end()) ?
                            getInitialMaxTTL(*DestinationIterator) : InitialMaxTTL;
   LastHop             = 0xffffffff;
   OutstandingRequests = 0;
   RunStartTimeStamp   = std::chrono::steady_clock::now();

   // Return whether end of the list is reached. Then, a rewind is necessary.
   return(DestinationIterator == Destinations.end());
}


// ###### Run the measurement ###############################################
void Traceroute::run()
{
   prepareRun(true);
   sendRequests();
   IOService.run();
}


// ###### Schedule timeout timer ############################################
void Traceroute::scheduleTimeoutEvent()
{
   const unsigned int deviation = std::max(10U, Expiration / 5);   // 20% deviation
   const unsigned int duration  = Expiration + (std::rand() % deviation);
   TimeoutTimer.expires_from_now(boost::posix_time::milliseconds(duration));
   TimeoutTimer.async_wait(std::bind(&Traceroute::handleTimeoutEvent, this,
                                     std::placeholders::_1));
}


// ###### Cancel timeout timer ##############################################
void Traceroute::cancelTimeoutEvent()
{
   TimeoutTimer.cancel();
}


// ###### Handle timer event ################################################
void Traceroute::handleTimeoutEvent(const boost::system::error_code& errorCode)
{
   if(StopRequested == false) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

      // ====== Has destination been reached with current TTL? ==============
      if(DestinationIterator != Destinations.end()) {
         TTLCache[*DestinationIterator] = LastHop;
         if(LastHop == 0xffffffff) {
            if(notReachedWithCurrentTTL()) {
               // Try another round ...
               sendRequests();
               return;
            }
         }
      }

      // ====== Create results output =======================================
      processResults();

      // ====== Prepare new run =============================================
      if(prepareRun() == false) {
         sendRequests();
      }
      else {
         // Done with this round -> schedule next round!
         scheduleIntervalEvent();
      }
   }
}


// ###### Schedule interval timer ###########################################
void Traceroute::scheduleIntervalEvent()
{
   if((Iterations == 0) || (IterationNumber < Iterations)) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

      // ====== Schedule event ==============================================
      long long millisecondsToWait;
      if(Destinations.begin() == Destinations.end()) {
          // Nothing to do -> wait 1 day
          millisecondsToWait = 24*3600*1000;
      }
      else {
         const unsigned long long deviation       = std::max(10ULL, Interval / 5);   // 20% deviation
         const unsigned long long waitingDuration = Interval + (std::rand() % deviation);
         const std::chrono::steady_clock::duration howLongToWait =
            (RunStartTimeStamp + std::chrono::milliseconds(waitingDuration)) - std::chrono::steady_clock::now();
         millisecondsToWait = std::max(0LL, (long long)std::chrono::duration_cast<std::chrono::milliseconds>(howLongToWait).count());
      }

      IntervalTimer.expires_from_now(boost::posix_time::milliseconds(millisecondsToWait));
      IntervalTimer.async_wait(std::bind(&Traceroute::handleIntervalEvent, this,
                                         std::placeholders::_1));
      HPCT_LOG(debug) << getName() << ": Waiting " << millisecondsToWait / 1000.0
                      << "s before iteration " << (IterationNumber + 1) << " ...";

      // ====== Check, whether it is time for starting a new transaction ====
      if(ResultsOutput) {
         ResultsOutput->mayStartNewTransaction();
      }
   }
   else {
       // ====== Done -> exit! ==============================================
       StopRequested.exchange(true);
       cancelIntervalEvent();
       cancelTimeoutEvent();
       IOModule->cancelSocket();
   }
}


// ###### Cancel interval timer #############################################
void Traceroute::cancelIntervalEvent()
{
   IntervalTimer.cancel();
}


// ###### Handle timer event ################################################
void Traceroute::handleIntervalEvent(const boost::system::error_code& errorCode)
{
   if(StopRequested == false) {
      // ====== Prepare new run =============================================
      if(errorCode != boost::asio::error::operation_aborted) {
         HPCT_LOG(debug) << getName() << ": Starting iteration " << (IterationNumber + 1) << " ...";
         prepareRun(true);
         sendRequests();
      }
   }
}


// ###### All requests have received a response #############################
void Traceroute::noMoreOutstandingRequests()
{
   HPCT_LOG(trace) << getName() << ": Completed!";
   cancelTimeoutEvent();
}


// ###### The destination has not been reached with the current TTL #########
bool Traceroute::notReachedWithCurrentTTL()
{
   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

   if(MaxTTL < FinalMaxTTL) {
      MinTTL = MaxTTL + 1;
      MaxTTL = std::min(MaxTTL + IncrementMaxTTL, FinalMaxTTL);
      HPCT_LOG(debug) << getName() << ": Cannot reach " << *DestinationIterator
                      << " with TTL " << MinTTL - 1 << ", now trying TTLs "
                      << MinTTL << " to " << MaxTTL << " ...";
      return(true);
   }
   return(false);
}


// ###### Send requests to all destinations #################################
void Traceroute::sendRequests()
{
   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

   // ====== Send requests, if there are destination addresses ==============
   if(DestinationIterator != Destinations.end()) {
      const DestinationInfo& destination = *DestinationIterator;
      HPCT_LOG(debug) << getName() << ": Traceroute from " << SourceAddress
                      << " to " << destination << " ...";

      // ====== Send Echo Requests ==========================================
      assert(MinTTL > 0);
      for(unsigned int round = 0; round < Rounds; round++) {
         for(int ttl = (int)MaxTTL; ttl >= (int)MinTTL; ttl--) {
            ResultEntry* resultEntry =
               IOModule->sendRequest(destination, (unsigned int)ttl, round,
                                     SeqNumber, TargetChecksumArray[round]);
            if(resultEntry) {
               OutstandingRequests++;
            }
         }
      }
      scheduleTimeoutEvent();
   }

   // ====== No destination addresses -> wait ===============================
   else {
      scheduleIntervalEvent();
   }
}


// ###### Get value for initial MaxTTL ######################################
unsigned int Traceroute::getInitialMaxTTL(const DestinationInfo& destination) const
{
   const std::map<DestinationInfo, unsigned int>::const_iterator found = TTLCache.find(destination);
   if(found != TTLCache.end()) {
      return(std::min(found->second, FinalMaxTTL));
   }
   return(InitialMaxTTL);
}


// ###### Received a new response ###########################################
void Traceroute::newResult(const ResultEntry* resultEntry)
{
   if(OutstandingRequests > 0) {
      OutstandingRequests--;
   }

   // ====== Found last hop? ================================================
   if(resultEntry->status() == Success) {
      LastHop = std::min(LastHop, resultEntry->hop());
   }

   // ====== Check whether there are still outstanding requests =============
   if(OutstandingRequests == 0) {
      noMoreOutstandingRequests();
   }
}


// ###### Comparison function for results output ############################
int Traceroute::compareTracerouteResults(const ResultEntry* a, const ResultEntry* b)
{
  return(a->hop() < b->hop());
}


// ###### Process results ###################################################
void Traceroute::processResults()
{
   uint64_t timeStamp = 0;

   // ====== Sort results ===================================================
   std::vector<ResultEntry*> resultsVector;
   for(std::map<unsigned short, ResultEntry*>::iterator iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), &compareTracerouteResults);

   // ====== Handle the results of each round ===============================
   for(unsigned int round = 0; round < Rounds; round++) {

      // ====== Count hops ==================================================
      std::size_t totalHops          = 0;
      std::size_t currentHop         = 0;
      bool        completeTraceroute = true;   // all hops have responded
      bool        destinationReached = false;  // destination has responded
      std::string pathString         = SourceAddress.to_string();
      for(std::vector<ResultEntry*>::iterator iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
         ResultEntry* resultEntry = *iterator;
         if(resultEntry->round() == round) {
            assert(resultEntry->hop() > totalHops);
            currentHop++;
            totalHops = resultEntry->hop();

            // ====== We have reached the destination =======================
            if(resultEntry->status() == Success) {
               pathString += "-" + resultEntry->destinationAddress().to_string();
               destinationReached = true;
               break;   // done!
            }

            // ====== Unreachable (as reported by router) ===================
            else if(statusIsUnreachable(resultEntry->status())) {
               pathString += "-" + resultEntry->destinationAddress().to_string();
               break;   // we can stop here!
            }

            // ====== Time-out ==============================================
            else if(resultEntry->status() == Unknown) {
               resultEntry->setStatus(Timeout);
               resultEntry->setReceiveTime(RXTimeStampType::RXTST_Application,
                                           TimeSourceType::TST_SysClock,
                                           resultEntry->sendTime(TXTimeStampType::TXTST_Application) + std::chrono::milliseconds(Expiration));
               pathString += "-*";
               completeTraceroute = false;   // at least one hop has not sent a response :-(
            }

            // ====== Some other response (usually TTL exceeded) ============
            else {
               pathString += "-" + resultEntry->destinationAddress().to_string();
            }
         }
      }
      assert(currentHop == totalHops);

      // ====== Compute path hash ===========================================
      // Checksum: the first 64 bits of the SHA-1 sum over path string
      boost::uuids::detail::sha1 sha1Hash;
      sha1Hash.process_bytes(pathString.c_str(), pathString.length());
      uint32_t digest[5];
      sha1Hash.get_digest(digest);
      const uint64_t pathHash    = ((uint64_t)digest[0] << 32) | (uint64_t)digest[1];
      unsigned int   statusFlags = 0x0000;
      if(!completeTraceroute) {
         statusFlags |= Flag_StarredRoute;
      }
      if(destinationReached) {
         statusFlags |= Flag_DestinationReached;
      }

      // ====== Print traceroute entries =======================================
      HPCT_LOG(trace) << getName() << ": Round " << round << ":";

      bool     writeHeader   = true;
      uint16_t checksumCheck = 0;
      for(std::vector<ResultEntry*>::iterator iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
         ResultEntry* resultEntry = *iterator;
         if(resultEntry->round() == round) {
            HPCT_LOG(trace) << getName() << ": " << *resultEntry;

            if(ResultCallback) {
               ResultCallback(this, resultEntry);
            }

            if(ResultsOutput) {
               if(timeStamp == 0) {
                  // NOTE:
                  // - The time stamp for this traceroute run is the first entry's send time!
                  //   This is necessary, in order to ensure that all entries use the same
                  //   time stamp for identification!
                  // - Also, entries of additional rounds are using this time stamp!
                  timeStamp = usSinceEpoch(resultEntry->sendTime(TXTimeStampType::TXTST_Application));
               }

               if(writeHeader) {

                  // ====== Current output format =================================
                  if(OutputFormat >= OFT_HiPerConTracer_Version2) {

                     puts("TBD");

                  }

                  // ====== Old output format =====================================
                  else {
                     ResultsOutput->insert(
                        str(boost::format("#T %s %s %x %d %x %d %x %x %x %d")
                           % SourceAddress.to_string()
                           % (*DestinationIterator).address().to_string()
                           % timeStamp
                           % round
                           % resultEntry->checksum()
                           % totalHops
                           % statusFlags
                           % (int64_t)pathHash
                           % (unsigned int)(*DestinationIterator).trafficClass()
                           % resultEntry->packetSize()
                     ));
                  }

                  writeHeader = false;
                  checksumCheck = resultEntry->checksum();
               }

               // ====== Current output format =================================
               if(OutputFormat >= OFT_HiPerConTracer_Version2) {

                  puts("TBD");

               }

               // ====== Old output format =====================================
               else {
                  unsigned int timeSource;
                  const ResultDuration rtt = resultEntry->obtainMostAccurateRTT(RXTimeStampType::RXTST_ReceptionSW,
                                                                                timeSource);

                  ResultsOutput->insert(
                     str(boost::format("\t%d %x %d %s %02x")
                        % resultEntry->hop()
                        % (unsigned int)resultEntry->status()
                        % std::chrono::duration_cast<std::chrono::microseconds>(rtt).count()
                        % resultEntry->destinationAddress().to_string()
                        % timeSource
                  ));
               }

               assert(resultEntry->checksum() == checksumCheck);
            }

            if( (resultEntry->status() == Success) ||
                (statusIsUnreachable(resultEntry->status())) ) {
               break;
            }
         }
      }
   }
}
