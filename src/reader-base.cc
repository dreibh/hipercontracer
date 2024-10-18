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

#include "reader-base.h"


// Approximated offset to system time:
// NOTE: This is an *approximation*, for checking whether a time time
//       appears to be resonable!
const ReaderTimeDuration ReaderClockOffsetFromSystemTime(
   std::chrono::nanoseconds(
      nsSinceEpoch<SystemTimePoint>(SystemClock::now()) -
      nsSinceEpoch<ReaderTimePoint>(ReaderClock::now())
   )
);


// ###### Constructor #######################################################
ReaderBase::ReaderBase(
   const ImporterConfiguration& importerConfiguration,
   const unsigned int           workers,
   const unsigned int           maxTransactionSize)
   : ImporterConfig(importerConfiguration),
     Workers(workers),
     MaxTransactionSize(maxTransactionSize)
{
   assert(Workers > 0);
   assert(MaxTransactionSize > 0);

   Statistics = new WorkerStatistics[Workers + 1];
   assert(Statistics != nullptr);
   for(unsigned int w = 0; w < Workers + 1; w++) {
      Statistics[w].Processed = Statistics[w].OldProcessed = 0;
   }
   LastStatisticsUpdate = SystemClock::now();
}


// ###### Destructor ########################################################
ReaderBase::~ReaderBase()
{
   delete [] Statistics;
   Statistics = nullptr;
}
