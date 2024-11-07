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

#include "traceroute.h"
#include "assure.h"
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
#include <iostream>
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
                       const char*                      outputFormatName,
                       const OutputFormatVersionType    outputFormatVersion,
                       const unsigned int               iterations,
                       const bool                       removeDestinationAfterRun,
                       const boost::asio::ip::address&  sourceAddress,
                       const std::set<DestinationInfo>& destinationArray,
                       const TracerouteParameters&      parameters)
   : Service(resultsWriter, outputFormatName, outputFormatVersion, iterations),
     TracerouteInstanceName(std::string("Traceroute(") + sourceAddress.to_string() + std::string(")")),
     RemoveDestinationAfterRun(removeDestinationAfterRun),
     Parameters(parameters),
     IOService(),
     SourceAddress(sourceAddress),
     TimeoutTimer(IOService),
     IntervalTimer(IOService)
{
   assure(Parameters.Rounds >= 1);
   assure(Parameters.InitialMaxTTL >= 1);
   assure(Parameters.InitialMaxTTL <= Parameters.FinalMaxTTL);
   assure( (Parameters.InitialMaxTTL >= 1) &&
           (Parameters.InitialMaxTTL <= Parameters.FinalMaxTTL) );

   // ====== Some initialisations ===========================================
   IOModule = IOModuleBase::createIOModule(
                 moduleName,
                 IOService, ResultsMap, SourceAddress, Parameters.SourcePort, Parameters.DestinationPort,
                 std::bind(&Traceroute::newResult, this, std::placeholders::_1),
                 Parameters.PacketSize);
   if(IOModule == nullptr) {
      throw std::runtime_error("Unable to initialise IO module for " + moduleName);
   }
   IOModule->setName(TracerouteInstanceName);
   SeqNumber           = (unsigned short)(std::rand() & 0xffff);
   OutstandingRequests = 0;
   LastHop             = 0xffffffff;
   IterationNumber     = 0;
   MinTTL              = 1;
   MaxTTL              = Parameters.InitialMaxTTL;
   TargetChecksumArray = new uint32_t[Parameters.Rounds];
   assure(TargetChecksumArray != nullptr);
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

   if(ResultsOutput) {
      ResultsOutput->specifyOutputFormat(OutputFormatName, OutputFormatVersion);
   }
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
    return SourceAddress;
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


// ###### Prepare service start #############################################
bool Traceroute::prepare(const bool privileged)
{
   if(!Service::prepare(privileged)) {
      return false;
   }
   if(privileged == true)  {
      // The socket preparation requires privileges.
      return IOModule->prepareSocket();
   }
   return true;
}


// ###### Start service #####################################################
bool Traceroute::start()
{
   StopRequested.exchange(false);
   Thread = std::thread(&Traceroute::run, this);
   return true;
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
      for(unsigned int i = 0; i < Parameters.Rounds; i++) {
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
                            getInitialMaxTTL(*DestinationIterator) : Parameters.InitialMaxTTL;
   LastHop             = 0xffffffff;
   OutstandingRequests = 0;
   RunStartTimeStamp   = std::chrono::steady_clock::now();

   // Return whether end of the list is reached. Then, a rewind is necessary.
   return DestinationIterator == Destinations.end();
}


// ###### Run the measurement ###############################################
void Traceroute::run()
{
   prepareRun(true);
   sendRequests();
   IOService.run();
}


// ###### Randomly deviate interval with given deviation percentage #########
// The random value will be chosen out of:
// [interval - deviation * interval, interval + deviation * interval]
unsigned long long Traceroute::makeDeviation(const unsigned long long interval,
                                             const float              deviation)
{
   assure(deviation >= 0.0);
   assure(deviation <= 1.0);

   const long long          d     = (long long)(interval * deviation);
   const unsigned long long value =
      ((long long)interval - d) + (std::rand() % (2 * d + 1));
   return value;
}


// ###### Schedule timeout timer ############################################
void Traceroute::scheduleTimeoutEvent()
{
   TimeoutTimer.expires_from_now(boost::posix_time::milliseconds(Parameters.Expiration));
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
      const unsigned long long waitingDuration = makeDeviation(Parameters.Interval, Parameters.Deviation);
      const std::chrono::steady_clock::duration howLongToWait =
         (RunStartTimeStamp + std::chrono::milliseconds(waitingDuration)) - std::chrono::steady_clock::now();
      const long long millisecondsToWait =
         std::max(0LL, (long long)std::chrono::duration_cast<std::chrono::milliseconds>(howLongToWait).count());

      IntervalTimer.expires_from_now(boost::posix_time::milliseconds(millisecondsToWait));
      IntervalTimer.async_wait(std::bind(&Traceroute::handleIntervalEvent, this,
                                         std::placeholders::_1));
      HPCT_LOG(debug) << getName() << ": Waiting " << millisecondsToWait / 1000.0
                      << " s before iteration " << (IterationNumber + 1) << " ...";

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

   if(MaxTTL < Parameters.FinalMaxTTL) {
      MinTTL = MaxTTL + 1;
      MaxTTL = std::min(MaxTTL + Parameters.IncrementMaxTTL, Parameters.FinalMaxTTL);
      HPCT_LOG(debug) << getName() << ": Cannot reach " << *DestinationIterator
                      << " with TTL " << MinTTL - 1 << ", now trying TTLs "
                      << MinTTL << " to " << MaxTTL << " ...";
      return true;
   }
   return false;
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
      assure(MinTTL > 0);
      OutstandingRequests +=
         IOModule->sendRequest(destination,
                               MaxTTL, MinTTL, 0, Parameters.Rounds - 1,
                               SeqNumber, TargetChecksumArray);

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
      return std::min(found->second, Parameters.FinalMaxTTL);
   }
   return Parameters.InitialMaxTTL;
}


// ###### Received a new response ###########################################
void Traceroute::newResult(const ResultEntry* resultEntry)
{
   if(OutstandingRequests > 0) {
      OutstandingRequests--;
   }

   // ====== Found last hop? ================================================
   if(resultEntry->status() == Success) {
      LastHop = std::min(LastHop, resultEntry->hopNumber());
   }

   // ====== Check whether there are still outstanding requests =============
   if(OutstandingRequests == 0) {
      noMoreOutstandingRequests();
   }
}


// ###### Comparison function for results output ############################
int Traceroute::compareTracerouteResults(const ResultEntry* a, const ResultEntry* b)
{
   // Traceroute:
   // The results in ResultsMap are only for a single destination.
   // But there are different rounds.
   // => sort by: rounds / hop

   // ====== Level 1: Round =================================================
   if(a->roundNumber() < b->roundNumber()) {
      return true;
   }
   else if(a->roundNumber() == b->roundNumber()) {
      // ====== Level 2: Hop ================================================
      if(a->hopNumber() < b->hopNumber()) {
         return true;
      }
   }
   return false;
}


// ###### Process results ###################################################
void Traceroute::processResults()
{
   // ====== Sort results ===================================================
   std::vector<ResultEntry*> resultsVector =
      makeSortedResultsVector(&compareTracerouteResults);

   // ====== Handle the results of each round ===============================
   uint64_t timeStamp = 0;
   for(unsigned int round = 0; round < Parameters.Rounds; round++) {

      // ====== Count hops ==================================================
      unsigned int totalHops          = 0;
      unsigned int currentHop         = 0;
      bool         completeTraceroute = true;   // all hops have responded
      bool         destinationReached = false;  // destination has responded
      std::string pathString         = SourceAddress.to_string();
      for(ResultEntry* resultEntry : resultsVector) {
         if(resultEntry->roundNumber() == round) {
            assure(resultEntry->hopNumber() > totalHops);
            currentHop++;
            totalHops = resultEntry->hopNumber();

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
               resultEntry->expire(Parameters.Expiration);
               pathString += "-*";
               completeTraceroute = false;   // at least one hop has not sent a response :-(
            }

            // ====== Some other response (usually TTL exceeded) ============
            else {
               pathString += "-" + resultEntry->destinationAddress().to_string();
            }
         }
      }
      assure(currentHop == totalHops);

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
      for(ResultEntry* resultEntry : resultsVector) {
         if(resultEntry->roundNumber() == round) {
            HPCT_LOG(trace) << getName() << ": " << *resultEntry;

            if(ResultCallback) {
               ResultCallback(this, resultEntry);
            }
            writeTracerouteResultEntry(resultEntry, timeStamp, writeHeader,
                                       totalHops, statusFlags, pathHash,
                                       checksumCheck);

            if( (resultEntry->status() == Success) ||
                (statusIsUnreachable(resultEntry->status())) ) {
               // Done!
               break;
            }
         }
      }
   }
}


// ###### Write Traceroute result entry to output file ######################
void Traceroute::writeTracerouteResultEntry(const ResultEntry* resultEntry,
                                            uint64_t&          timeStamp,
                                            bool&              writeHeader,
                                            const unsigned int totalHops,
                                            const unsigned int statusFlags,
                                            const uint64_t     pathHash,
                                            uint16_t&          checksumCheck)
{
   if(timeStamp == 0) {
      // NOTE:
      // - The time stamp for this traceroute run is the first entry's send time!
      //   This is necessary, in order to ensure that all entries use the same
      //   time stamp for identification!
      // - Also, entries of additional rounds are using this time stamp!
      timeStamp = nsSinceEpoch<ResultTimePoint>(
         resultEntry->sendTime(TXTimeStampType::TXTST_Application));
   }

   if(ResultsOutput) {

      if(writeHeader) {
         // ====== Current output format =================================
         if(OutputFormatVersion >= OFT_HiPerConTracer_Version2) {
            ResultsOutput->insert(
               str(boost::format("#T%c %d %s %s %x %d %d %x %d %x %d %d %x %x")
                  % (unsigned char)IOModule->getProtocolType()

                  % ResultsOutput->measurementID()
                  % resultEntry->sourceAddress().to_string()
                  % resultEntry->destinationAddress().to_string()
                  % timeStamp
                  % resultEntry->roundNumber()

                  % totalHops

                  % (unsigned int)(*DestinationIterator).trafficClass()
                  % resultEntry->packetSize()
                  % resultEntry->checksum()
                  % resultEntry->sourcePort()
                  % resultEntry->destinationPort()
                  % statusFlags

                  % (int64_t)pathHash
            ));
         }

         // ====== Old output format =====================================
         else {
            ResultsOutput->insert(
               str(boost::format("#T %s %s %x %d %x %d %x %x %x %d")
                  % resultEntry->sourceAddress().to_string()
                  % resultEntry->destinationAddress().to_string()
                  % (timeStamp / 1000)
                  % resultEntry->roundNumber()
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
      if(OutputFormatVersion >= OFT_HiPerConTracer_Version2) {
         unsigned int   timeSource;
         ResultDuration rttApplication;
         ResultDuration rttSoftware;
         ResultDuration rttHardware;
         ResultDuration delayAppSend;
         ResultDuration delayAppReceive;
         ResultDuration delayQueuing;

         resultEntry->obtainResultsValues(timeSource,
                                          rttApplication, rttSoftware, rttHardware,
                                          delayQueuing, delayAppSend, delayAppReceive);
         const unsigned long long sendTimeStamp = nsSinceEpoch<ResultTimePoint>(
            resultEntry->sendTime(TXTimeStampType::TXTST_Application));

         ResultsOutput->insert(
            str(boost::format("\t%x %d %d %d %08x %d %d %d %d %d %d %s")
               % sendTimeStamp
               % resultEntry->hopNumber()
               % resultEntry->responseSize()
               % (unsigned int)resultEntry->status()

               % timeSource
               % std::chrono::duration_cast<std::chrono::nanoseconds>(delayAppSend).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(delayQueuing).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(delayAppReceive).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(rttApplication).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(rttSoftware).count()
               % std::chrono::duration_cast<std::chrono::nanoseconds>(rttHardware).count()

               % resultEntry->hopAddress().to_string()
         ));

      }

      // ====== Old output format =====================================
      else {
         unsigned int timeSource;
         const ResultDuration rtt = resultEntry->obtainMostAccurateRTT(RXTimeStampType::RXTST_ReceptionSW,
                                                                       timeSource);

         ResultsOutput->insert(
            str(boost::format("\t%d %x %d %s %02x")   /* status is hex here! */
               % resultEntry->hopNumber()
               % (unsigned int)resultEntry->status()
               % std::chrono::duration_cast<std::chrono::microseconds>(rtt).count()
               % resultEntry->hopAddress().to_string()
               % timeSource
         ));
      }

      assure(resultEntry->checksum() == checksumCheck);
   }

   // ====== Write to stdout ================================================
   // This output is made when no results file is written. Then, the user
   // should get a useful (i.e. reduced, readable) stdout output.
   else {
      unsigned int   timeSource;
      ResultDuration rttApplication;
      ResultDuration rttSoftware;
      ResultDuration rttHardware;
      ResultDuration delayAppSend;
      ResultDuration delayAppReceive;
      ResultDuration delayQueuing;

      resultEntry->obtainResultsValues(timeSource,
                                       rttApplication, rttSoftware, rttHardware,
                                       delayQueuing, delayAppSend, delayAppReceive);

      const long long usSend        = std::chrono::duration_cast<std::chrono::nanoseconds>(delayAppSend).count();
      const long long usQueuing     = std::chrono::duration_cast<std::chrono::nanoseconds>(delayQueuing).count();
      const long long usReceive     = std::chrono::duration_cast<std::chrono::nanoseconds>(delayAppReceive).count();
      const long long nsApplication = std::chrono::duration_cast<std::chrono::nanoseconds>(rttApplication).count();
      const long long nsSoftware    = std::chrono::duration_cast<std::chrono::nanoseconds>(rttSoftware).count();
      const long long nsHardware    = std::chrono::duration_cast<std::chrono::nanoseconds>(rttHardware).count();
      const std::string s  = (usSend < 0)     ? "-----" : str(boost::format("%3.0fµs") % (usSend     / 1000.0));
      const std::string q  = (usQueuing < 0)  ? "-----" : str(boost::format("%3.0fµs") % (usQueuing  / 1000.0));
      const std::string r  = (usReceive < 0)  ? "-----" : str(boost::format("%3.0fµs") % (usReceive  / 1000.0));
      const std::string ap = (resultEntry->status() == Timeout) ?
                                "TIMEOUT" :
                                str(boost::format("%3.3fms") % (nsApplication / 1000000.0));
      const std::string sw = (nsSoftware < 0) ? "---" : str(boost::format("%3.3fms") % (nsSoftware / 1000000.0));
      const std::string hw = (nsHardware < 0) ? "---" : str(boost::format("%3.3fms") % (nsHardware / 1000000.0));

      if(writeHeader) {
         std::cout << boost::format("\e[34m%s: Traceroute %s\t%s %s\e[0m\n")
                        % timePointToString<ResultTimePoint>(resultEntry->sendTime(TXTimeStampType::TXTST_Application), 3)
                        % IOModule->getProtocolName()
                        % resultEntry->sourceAddress().to_string()
                        % resultEntry->destinationAddress().to_string();
         writeHeader = false;
      }

      std::cout << boost::format("%s%2d: %-40s\t%-16s\ts:%s q:%s r:%s\tA:%-9s S:%-9s H:%-9s\e[0m\n")
                      % getStatusColor(resultEntry->status())
                      % resultEntry->hopNumber()
                      % resultEntry->hopAddress().to_string()
                      % getStatusName(resultEntry->status())
                      % s
                      % q
                      % r
                      % ap
                      % sw
                      % hw;
   }
}
