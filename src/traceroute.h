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

#ifndef TRACEROUTE_H
#define TRACEROUTE_H

#include "iomodule-base.h"
#include "resultentry.h"
#include "resultswriter.h"
#include "service.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include <boost/asio.hpp>


class ICMPHeader;


struct TracerouteParameters
{
   unsigned long long Interval;
   unsigned int       Expiration;
   float              Deviation;
   unsigned int       Rounds;
   unsigned int       InitialMaxTTL;
   unsigned int       FinalMaxTTL ;
   unsigned int       IncrementMaxTTL;
   unsigned int       PacketSize;
   uint16_t           SourcePort;
   uint16_t           DestinationPort;
};


class Traceroute : public Service
{
   public:
   Traceroute(const std::string                moduleName,
              ResultsWriter*                   resultsWriter,
              const char*                      outputFormatName,
              const OutputFormatVersionType    outputFormatVersion,
              const unsigned int               iterations,
              const bool                       removeDestinationInfoAfterRun,
              const boost::asio::ip::address&  sourceAddress,
              const std::set<DestinationInfo>& destinationArray,
              const TracerouteParameters&      parameters);
   virtual ~Traceroute();

   virtual const boost::asio::ip::address& getSource();
   virtual bool addDestination(const DestinationInfo& destination);

   virtual const std::string& getName() const;
   virtual bool prepare(const bool privileged);
   virtual bool start();
   virtual void requestStop();
   virtual bool joinable();
   virtual void join();

   protected:
   virtual bool prepareRun(const bool newRound = false);
   void         run();
   virtual void scheduleTimeoutEvent();
   void         cancelTimeoutEvent();
   virtual void handleTimeoutEvent(const boost::system::error_code& errorCode);
   virtual void scheduleIntervalEvent();
   void         cancelIntervalEvent();
   virtual void handleIntervalEvent(const boost::system::error_code& errorCode);
   virtual void noMoreOutstandingRequests();
   virtual bool notReachedWithCurrentTTL();
   virtual void sendRequests();
   virtual void processResults();

   static unsigned long long makeDeviation(const unsigned long long interval,
                                           const float              deviation);
   unsigned int getInitialMaxTTL(const DestinationInfo&   destination) const;
   void         newResult(const ResultEntry* resultEntry);

   inline std::vector<ResultEntry*> makeSortedResultsVector(int (*compareResults)(const ResultEntry* a,
                                                                                  const ResultEntry* b)) const {
      std::vector<ResultEntry*> resultsVector;

      resultsVector.reserve(ResultsMap.size());
      for(std::map<unsigned short, ResultEntry*>::const_iterator iterator = ResultsMap.begin();
          iterator != ResultsMap.end(); iterator++) {
         resultsVector.push_back(iterator->second);
      }
      std::sort(resultsVector.begin(), resultsVector.end(), compareResults);

      return resultsVector;
   }

   static int compareTracerouteResults(const ResultEntry* a, const ResultEntry* b);
   void writeTracerouteResultEntry(const ResultEntry* resultEntry,
                                   uint64_t&          timeStamp,
                                   bool&              writeHeader,
                                   const unsigned int totalHops,
                                   const unsigned int statusFlags,
                                   const uint64_t     pathHash,
                                   uint16_t&          checksumCheck);

   const std::string                       TracerouteInstanceName;
   const bool                              RemoveDestinationAfterRun;
   const TracerouteParameters              Parameters;
   boost::asio::io_context                 IOContext;
   boost::asio::ip::address                SourceAddress;
   std::recursive_mutex                    DestinationMutex;
   std::set<DestinationInfo>               Destinations;
   std::set<DestinationInfo>::iterator     DestinationIterator;
   boost::asio::deadline_timer             TimeoutTimer;
   boost::asio::deadline_timer             IntervalTimer;

   IOModuleBase*                           IOModule;
   std::thread                             Thread;
   std::atomic<bool>                       StopRequested;
   unsigned int                            IterationNumber;
   uint16_t                                SeqNumber;
   unsigned int                            OutstandingRequests;
   unsigned int                            LastHop;
   std::map<unsigned short, ResultEntry*>  ResultsMap;
   std::map<DestinationInfo, unsigned int> TTLCache;
   unsigned int                            MinTTL;
   unsigned int                            MaxTTL;
   std::chrono::steady_clock::time_point   RunStartTimeStamp;
   uint32_t*                               TargetChecksumArray;

   private:
};

#endif
