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

#ifndef JITTER_H
#define JITTER_H

#include "ping.h"


class JitterRFC3550;

class Jitter : public Ping
{
   public:
   Jitter(const std::string                moduleName,
          ResultsWriter*                   resultsWriter,
          const char*                      outputFormatName,
          const OutputFormatVersionType    outputFormatVersion,
          const unsigned int               iterations,
          const bool                       removeDestinationAfterRun,
          const boost::asio::ip::address&  sourceAddress,
          const std::set<DestinationInfo>& destinationArray,
          const TracerouteParameters&      parameters,
          const bool                       recordRawResults = false);
   virtual ~Jitter();

   virtual const std::string& getName() const;

   protected:
   virtual void processResults();

   void computeJitter(const std::vector<ResultEntry*>::const_iterator& start,
                      const std::vector<ResultEntry*>::const_iterator& end);
   void writeJitterResultEntry(const ResultEntry* referenceEntry,
                               const unsigned int timeSource,
                               const JitterRFC3550& jitterQueuing,
                               const JitterRFC3550& jitterAppSend,
                               const JitterRFC3550& jitterAppReceive,
                               const JitterRFC3550& jitterApplication,
                               const JitterRFC3550& jitterSoftware,
                               const JitterRFC3550& jitterHardware);

   private:
   const std::string JitterInstanceName;
   const bool        RecordRawResults;
};

#endif
