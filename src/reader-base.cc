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
// Copyright (C) 2015-2024 by Thomas Dreibholz
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
   LastStatisticsUpdate = ReaderClock::now();
}


// ###### Destructor ########################################################
ReaderBase::~ReaderBase()
{
   delete [] Statistics;
   Statistics = nullptr;
}


// ###### Make directory hierarchy from a file, relative to importer path ###
// Example:
// dataFile="4133/udpping/2024-06-12/uping_10382.dat.2024-06-12_13-10-22.xz"
// timestamp=(2024-06-12 13:10:22)
// directoryLevels=0 timestampLevels=5 -> "2024/06/11/15:00"
// directoryLevels=0 timestampLevels=4 -> "2024/06/11/15:00"
// directoryLevels=0 timestampLevels=3 -> "2024/06/11"
// directoryLevels=0 timestampLevels=2 -> "2024/06"
// directoryLevels=0 timestampLevels=1 -> "2024"
// directoryLevels=0 timestampLevels=0 -> ""
// directoryLevels=1 timestampLevels=5 -> "4133/2024/06/11/15:00"
// directoryLevels=1 timestampLevels=4 -> "4133/2024/06/11/15:00"
// directoryLevels=1 timestampLevels=3 -> "4133/2024/06/11"
// directoryLevels=1 timestampLevels=2 -> "4133/2024/06"
// directoryLevels=1 timestampLevels=1 -> "4133/2024"
// directoryLevels=1 timestampLevels=0 -> "4133"
// directoryLevels=2 timestampLevels=5 -> "4133/udpping/2024/06/11/15:00"
// directoryLevels=2 timestampLevels=4 -> "4133/udpping/2024/06/11/15:00"
// directoryLevels=2 timestampLevels=3 -> "4133/udpping/2024/06/11"
// directoryLevels=2 timestampLevels=2 -> "4133/udpping/2024/06"
// directoryLevels=2 timestampLevels=1 -> "4133/udpping/2024"
// directoryLevels=2 timestampLevels=0 -> "4133/udpping"
std::filesystem::path ReaderBase::makeDirectoryHierarchy(const std::filesystem::path& dataFile,
                                                         const ReaderTimePoint&       timeStamp) const
{
   const int             directoryLevels = ImporterConfig.getMoveDirectoryDepth();
   const int             timestampLevels = ImporterConfig.getMoveTimestampDepth();
   std::filesystem::path hierarchy;

   // FIXME!
   std::filesystem::path ABS("/home/nornetpp/src/nne-server-utils/universal-importer/src/TestDB/data/");


   // Get the relative directory the file is located in:
   std::error_code             ec;
   const std::filesystem::path relPath =
      std::filesystem::relative(dataFile.parent_path(), ImporterConfig.getImportFilePath(), ec);

   if( (!ec) /* && (relPath .has_parent_path())  */ ) {
      unsigned int d = 0;
      for(std::filesystem::path::const_iterator iterator = relPath.begin(); iterator != relPath.end(); iterator++) {
         if(d++ >= directoryLevels) {
            break;
         }
         else if( (d == 1) && ((*iterator) == ".") ) {
            // First directory is ".": there is no hierarchy -> stop here.
            break;
         }
         // std::cout << "i=" << (*iterator) << "\t";
         hierarchy = hierarchy / (*iterator);
      }
   }

   if(timestampLevels > 0) {
      const char* format = nullptr;
      if(timestampLevels >= 5) {
         format = "%Y/%m/%d/%H:%M";
      }
      else if(timestampLevels == 4) {
         format = "%Y/%m/%d/%H:00";
      }
      else if(timestampLevels == 3) {
         format = "%Y/%m/%d";
      }
      else if(timestampLevels == 2) {
         format = "%Y/%m";
      }
      else if(timestampLevels == 1) {
         format = "%Y";
      }
      hierarchy = hierarchy / timePointToString<ReaderTimePoint>(timeStamp, 0, format);
   }

   // std::cout << dataFile << " -> " << hierarchy << "\n";
   return hierarchy;
}
