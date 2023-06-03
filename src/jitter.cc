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

#include "jitter.h"
#include "jitter-rfc3550.h"
#include "tools.h"
#include "logger.h"

#include <functional>
#include <boost/format.hpp>


// ###### Constructor #######################################################
Jitter::Jitter(const std::string                moduleName,
               ResultsWriter*                   resultsWriter,
               const OutputFormatType           outputFormat,
               const unsigned int               iterations,
               const bool                       removeDestinationAfterRun,
               const boost::asio::ip::address&  sourceAddress,
               const std::set<DestinationInfo>& destinationArray,
               const bool                       recordRawResults,
               const unsigned long long         interval,
               const unsigned int               expiration,
               const unsigned int               rounds,
               const unsigned int               ttl,
               const unsigned int               packetSize,
               const uint16_t                   destinationPort)
   : Ping(moduleName,
          resultsWriter, outputFormat, iterations, removeDestinationAfterRun,
          sourceAddress, destinationArray,
          interval, expiration, rounds, ttl,
          packetSize, destinationPort),
     JitterInstanceName(std::string("Jitter(") + sourceAddress.to_string() + std::string(")")),
     RecordRawResults(recordRawResults)
{
   IOModule->setName(JitterInstanceName);
}


// ###### Destructor ########################################################
Jitter::~Jitter()
{
}


// ###### Start thread ######################################################
const std::string& Jitter::getName() const
{
   return JitterInstanceName;
}


// ###### Process results ###################################################
void Jitter::processResults()
{
   // ====== Sort results ===================================================
   std::vector<ResultEntry*> resultsVector =
      makeSortedResultsVector(&comparePingResults);
   // The vector is sorted by destination/round in comparePingResults()!

   // ====== Process results ================================================
   const ResultTimePoint                       now        = ResultClock::now();
   std::vector<ResultEntry*>::iterator         iterator   = resultsVector.begin();
   std::vector<ResultEntry*>::const_iterator   start      = resultsVector.begin();
   bool                                        isComplete = true;
   for(   ; iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;

      // ====== Time-out entries ============================================
      if( (resultEntry->status() == Unknown) &&
          (std::chrono::duration_cast<std::chrono::milliseconds>(now - resultEntry->sendTime(TXTimeStampType::TXTST_Application)).count() >= Expiration) ) {
         resultEntry->expire(Expiration);
      }

      // If there is still an entry with unknown status, this block cannot
      // be processed by the jitter calculation, yet.
      if(resultEntry->status() == Unknown) {
         isComplete = false;
      }

      if(resultEntry->roundNumber() == 0) {
         // New block -> try to process previous block, then start new one:
         if( (isComplete) && (start != resultsVector.begin()) ) {
            computeJitter(start, iterator);
            start      = iterator;
            isComplete = true;
         }
      }
   }
   if(isComplete) {
      computeJitter(start, iterator);
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


// ###### Compute jitter, according to RFC 3550 #############################
void Jitter::computeJitter(const std::vector<ResultEntry*>::const_iterator& start,
                           const std::vector<ResultEntry*>::const_iterator& end)
{
   const ResultEntry* referenceEntry = nullptr;
   JitterRFC3550      jitterQueuing;
   JitterRFC3550      jitterAppSend;
   JitterRFC3550      jitterAppReceive;
   JitterRFC3550      jitterApplication;
   JitterRFC3550      jitterSoftware;
   JitterRFC3550      jitterHardware;
   unsigned int       timeSource;
   unsigned int       timeSourceApplication;
   unsigned int       timeSourceSoftware;
   unsigned int       timeSourceHardware;
   unsigned int       timeSourceAppSend;
   unsigned int       timeSourceAppReceive;
   unsigned int       timeSourceQueuing;
   ResultTimePoint    sendTime;
   ResultTimePoint    receiveTime;
   unsigned short     roundNumber = 0;

   HPCT_LOG(trace) << getName() << ": computeJitter()";
   for(std::vector<ResultEntry*>::const_iterator iterator = start; iterator != end; iterator++) {
      const ResultEntry* resultEntry = *iterator;
      assert(resultEntry->roundNumber() == roundNumber);
      roundNumber++;

      HPCT_LOG(trace) << getName() << ": " << *resultEntry;
      if(ResultCallback) {
         ResultCallback(this, resultEntry);
      }

      // ====== Compute jitter ==============================================
      if(resultEntry->status() == Success) {
         if(resultEntry->obtainSchedulingSendTime(timeSourceQueuing, sendTime, receiveTime)) {
            // NOTE: For queuing: sendTime = schedulingTime ; receiveTime = actual send time!
            jitterQueuing.process(timeSourceQueuing,
                                  nsSinceEpoch<ResultTimePoint>(sendTime),
                                  nsSinceEpoch<ResultTimePoint>(receiveTime));
         }

         if(resultEntry->obtainApplicationSendSchedulingTime(timeSourceAppSend, sendTime, receiveTime)) {
            // NOTE: For queuing: sendTime = schedulingTime ; receiveTime = actual send time!
            jitterAppSend.process(timeSourceAppSend,
                                  nsSinceEpoch<ResultTimePoint>(sendTime),
                                  nsSinceEpoch<ResultTimePoint>(receiveTime));
         }
         if(resultEntry->obtainReceptionApplicationReceiveTime(timeSourceAppReceive, sendTime, receiveTime)) {
            // NOTE: For queuing: sendTime = schedulingTime ; receiveTime = actual send time!
            jitterAppReceive.process(timeSourceAppReceive,
                                     nsSinceEpoch<ResultTimePoint>(sendTime),
                                     nsSinceEpoch<ResultTimePoint>(receiveTime));
         }

         if(resultEntry->obtainSendReceiveTime(RXTimeStampType::RXTST_Application, timeSourceApplication, sendTime, receiveTime)) {
            jitterApplication.process(timeSourceApplication,
                                      nsSinceEpoch<ResultTimePoint>(sendTime),
                                      nsSinceEpoch<ResultTimePoint>(receiveTime));
         }
         if(resultEntry->obtainSendReceiveTime(RXTimeStampType::RXTST_ReceptionSW, timeSourceSoftware, sendTime, receiveTime)) {
            jitterSoftware.process(timeSourceSoftware,
                                   nsSinceEpoch<ResultTimePoint>(sendTime),
                                   nsSinceEpoch<ResultTimePoint>(receiveTime));
         }
         if(resultEntry->obtainSendReceiveTime(RXTimeStampType::RXTST_ReceptionHW, timeSourceHardware, sendTime, receiveTime)) {
            jitterHardware.process(timeSourceHardware,
                                   nsSinceEpoch<ResultTimePoint>(sendTime),
                                   nsSinceEpoch<ResultTimePoint>(receiveTime));
         }
      }

      // ====== Set pointer to reference entry ==============================
      // The reference entry points to basic configuration values. It is the
      // first successful entry (if one is successufl), or otherwise the first
      // failed entry.
      if( (referenceEntry == nullptr) ||
          (referenceEntry->status() == Success) ) {
         referenceEntry = resultEntry;
         if(referenceEntry->status() == Success) {
            timeSource = (timeSourceApplication << 24) |
                         (timeSourceQueuing     << 16) |
                         (timeSourceSoftware    << 8)  |
                         timeSourceHardware;
         }
      }
   }

   if(referenceEntry) {
      // ====== Record Jitter entry =========================================
      writeJitterResultEntry(referenceEntry,    timeSource,
                             jitterQueuing,     jitterAppSend,  jitterAppReceive,
                             jitterApplication, jitterSoftware, jitterHardware);

      // ====== Record raw Ping results as well =============================
      if(RecordRawResults) {
         for(std::vector<ResultEntry*>::const_iterator iterator = start; iterator != end; iterator++) {
            const ResultEntry* resultEntry = *iterator;
            writePingResultEntry(resultEntry, "\t");
         }
      }
   }

   // ====== Remove completed entries =======================================
   for(std::vector<ResultEntry*>::const_iterator iterator = start; iterator != end; iterator++) {
      const ResultEntry* resultEntry    = *iterator;
      const std::size_t  elementsErased = ResultsMap.erase(resultEntry->seqNumber());
      assert(elementsErased == 1);
      delete resultEntry;
      if(OutstandingRequests > 0) {
         OutstandingRequests--;
      }
   }
}


// ###### Write Jitter result entry to output file ############################
void Jitter::writeJitterResultEntry(const ResultEntry*   referenceEntry,
                                    const unsigned int   timeSource,
                                    const JitterRFC3550& jitterQueuing,
                                    const JitterRFC3550& jitterAppSend,
                                    const JitterRFC3550& jitterAppReceive,
                                    const JitterRFC3550& jitterApplication,
                                    const JitterRFC3550& jitterSoftware,
                                    const JitterRFC3550& jitterHardware)
{
   HPCT_LOG(trace) << getName() << ": "
                   << referenceEntry->destinationAddress()
                   << "\tA:" << jitterApplication.packets() << "/" << jitterApplication.jitter() << "/" << jitterApplication.meanLatency()
                   << "\tS:" << jitterSoftware.packets() << "/" << jitterSoftware.jitter() << "/" << jitterSoftware.meanLatency()
                   << "\tH:" << jitterHardware.packets() << "/" << jitterHardware.jitter() << "/" << jitterHardware.meanLatency();

   if(ResultsOutput) {
      const unsigned long long sendTimeStamp = nsSinceEpoch<ResultTimePoint>(
         referenceEntry->sendTime(TXTimeStampType::TXTST_Application));

      ResultsOutput->insert(
         str(boost::format("#J%c %d %s %s %x %d %x %d %x %d %08x %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d")
            % (unsigned char)IOModule->getProtocolType()

            % ResultsOutput->measurementID()
            % referenceEntry->sourceAddress().to_string()
            % referenceEntry->destinationAddress().to_string()
            % sendTimeStamp
            % referenceEntry->roundNumber()

            % (unsigned int)referenceEntry->destination().trafficClass()
            % referenceEntry->packetSize()
            % referenceEntry->checksum()
            % referenceEntry->status()

            % timeSource

            % jitterAppSend.packets()
            % jitterAppSend.jitter()
            % jitterAppSend.meanLatency()

            % jitterQueuing.packets()
            % jitterQueuing.jitter()
            % jitterQueuing.meanLatency()

            % jitterAppReceive.packets()
            % jitterAppReceive.jitter()
            % jitterAppReceive.meanLatency()

            % jitterApplication.packets()
            % jitterApplication.jitter()
            % jitterApplication.meanLatency()

            % jitterSoftware.packets()
            % jitterSoftware.jitter()
            % jitterSoftware.meanLatency()

            % jitterHardware.packets()
            % jitterHardware.jitter()
            % jitterHardware.meanLatency()
         ));
   }
}
