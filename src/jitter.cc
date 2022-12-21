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

#include "jitter.h"
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
     JitterInstanceName(std::string("Jitter(") + sourceAddress.to_string() + std::string(")"))
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

   // ====== Process results ================================================
   const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
   for(ResultEntry* resultEntry : resultsVector) {

      // ====== Time-out entries ============================================
      if( (resultEntry->status() == Unknown) &&
          (std::chrono::duration_cast<std::chrono::milliseconds>(now - resultEntry->sendTime(TXTimeStampType::TXTST_Application)).count() >= Expiration) ) {
         resultEntry->expire(Expiration);
      }

      puts("TBD!");
#if 0
      // ====== Print completed entries =====================================
      if(resultEntry->status() != Unknown) {
         HPCT_LOG(trace) << getName() << ": " << *resultEntry;

         if(ResultCallback) {
            ResultCallback(this, resultEntry);
         }

         if(ResultsOutput) {
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
               str(boost::format("#P%c %s %s %x %d %x %d %x %d %08x %d %d %d %d")
                  % (unsigned char)IOModule->getProtocolType()

                  % SourceAddress.to_string()
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
      }
#endif

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
