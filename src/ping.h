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

#ifndef PING_H
#define PING_H

#include "traceroute.h"


class Ping : public Traceroute
{
   public:
   Ping(const std::string                moduleName,
        ResultsWriter*                   resultsWriter,
        const char*                      outputFormatName,
        const OutputFormatVersionType    outputFormatVersion,
        const unsigned int               iterations,
        const bool                       removeDestinationAfterRun,
        const boost::asio::ip::address&  sourceAddress,
        const std::set<DestinationInfo>& destinationArray,
        const TracerouteParameters&      parameters);
   virtual ~Ping();

   virtual const std::string& getName() const;

   protected:
   virtual bool prepareRun(const bool newRound = false);
   virtual void scheduleTimeoutEvent();
   virtual void noMoreOutstandingRequests();
   virtual bool notReachedWithCurrentTTL();
   virtual void sendRequests();
   virtual void processResults();

   static int comparePingResults(const ResultEntry* a, const ResultEntry* b);
   void writePingResultEntry(const ResultEntry* resultEntry,
                             const char*        indentation = "");

   private:
   const std::string PingInstanceName;
};

#endif
