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
// Copyright (C) 2015-2021 by Thomas Dreibholz
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

#ifndef PING_H
#define PING_H

#include "traceroute.h"


class Ping : public Traceroute
{
   public:
   Ping(ResultsWriter*                   resultsWriter,
        const unsigned int               iterations,
        const bool                       removeDestinationAfterRun,
        const boost::asio::ip::address&  sourceAddress,
        const std::set<DestinationInfo>& destinationArray,
        const unsigned long long         interval   =  1000,
        const unsigned int               expiration = 10000,
        const unsigned int               ttl        =    64,
        const unsigned int               packetSize =     0);
   virtual ~Ping();

   virtual const std::string& getName() const;

   protected:
   virtual bool prepareRun(const bool newRound = false);
   virtual void scheduleTimeoutEvent();
   virtual void noMoreOutstandingRequests();
   virtual bool notReachedWithCurrentTTL();
   virtual void processResults();
   virtual void sendRequests();

   private:
   static int comparePingResults(const ResultEntry* a, const ResultEntry* b);

   const std::string PingInstanceName;
};

#endif
