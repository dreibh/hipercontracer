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


enum OutputFormatType
{
   OFT_HiPerConTracer_Version1 = 1,
   OFT_HiPerConTracer_Version2 = 2,

   OFT_Min = OFT_HiPerConTracer_Version1,
   OFT_Max = OFT_HiPerConTracer_Version2
};


class Traceroute : public Service
{
   public:
   Traceroute(const std::string                moduleName,
              ResultsWriter*                   resultsWriter,
              const OutputFormatType           outputFormat,
              const unsigned int               iterations,
              const bool                       removeDestinationInfoAfterRun,
              const boost::asio::ip::address&  sourceAddress,
              const std::set<DestinationInfo>& destinationArray,
              const unsigned long long         interval        = 30*60000ULL,
              const unsigned int               expiration      = 3000,
              const unsigned int               rounds          = 1,
              const unsigned int               initialMaxTTL   = 5,
              const unsigned int               finalMaxTTL     = 35,
              const unsigned int               incrementMaxTTL = 2,
              const unsigned int               packetSize      = 0,
              const uint16_t                   destinationPort = 7);
   virtual ~Traceroute();

   virtual const boost::asio::ip::address& getSource();
   virtual bool addDestination(const DestinationInfo& destination);

   virtual const std::string& getName() const;
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

   unsigned int getInitialMaxTTL(const DestinationInfo&   destination) const;
   void         newResult(const ResultEntry* resultEntry);

   const std::string                       TracerouteInstanceName;
   ResultsWriter*                          ResultsOutput;
   const OutputFormatType                  OutputFormat;
   const unsigned int                      Iterations;
   const bool                              RemoveDestinationAfterRun;
   const unsigned long long                Interval;
   const unsigned int                      Expiration;
   const unsigned int                      Rounds;
   const unsigned int                      InitialMaxTTL;
   const unsigned int                      FinalMaxTTL;
   const unsigned int                      IncrementMaxTTL;
   boost::asio::io_service                 IOService;
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
   static int compareTracerouteResults(const ResultEntry* a, const ResultEntry* b);
};

#endif
