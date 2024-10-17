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

#ifndef READER_BASE_H
#define READER_BASE_H

#include "databaseclient-base.h"
#include "importer-configuration.h"

#include "logger.h"
#include "tools.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <list>
#include <mutex>
#include <regex>
#include <string>

#include <boost/iostreams/filtering_stream.hpp>


enum ReaderPriority {
   Low  = 0,
   High = 1,
   Max  = High
};

typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemTimeDuration;

typedef std::chrono::high_resolution_clock   ReaderClock;
typedef std::chrono::time_point<ReaderClock> ReaderTimePoint;
typedef ReaderClock::duration                ReaderTimeDuration;

// Approximated offset to system time:
extern const ReaderTimeDuration ReaderClockOffsetFromSystemTime;


// ###### Reader base class #################################################
class ReaderBase
{
   public:
   ReaderBase(const ImporterConfiguration& importerConfiguration,
              const unsigned int           workers,
              const unsigned int           maxTransactionSize);
   virtual ~ReaderBase();

   inline const unsigned int getWorkers() const            { return Workers;            }
   inline const unsigned int getMaxTransactionSize() const { return MaxTransactionSize; }

   virtual const std::string& getIdentification() const = 0;
   virtual const std::regex&  getFileNameRegExp() const = 0;

   virtual int addFile(const std::filesystem::path& dataFile,
                       const std::smatch            match) = 0;
   virtual bool removeFile(const std::filesystem::path& dataFile,
                           const std::smatch            match) = 0;
   virtual unsigned int fetchFiles(std::list<std::filesystem::path>& dataFileList,
                                   const unsigned int                worker,
                                   const unsigned int                limit = 1) = 0;
   virtual std::filesystem::path getDirectoryHierarchy(const std::filesystem::path& dataFile,
                                                       const std::smatch            match) = 0;
   virtual void printStatus(std::ostream& os = std::cout) = 0;

   virtual void beginParsing(DatabaseClientBase& databaseClient,
                             unsigned long long& rows) = 0;
   virtual bool finishParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows) = 0;
   virtual void parseContents(DatabaseClientBase&                  databaseClient,
                              unsigned long long&                  rows,
                              const std::filesystem::path&         dataFile,
                              boost::iostreams::filtering_istream& dataStream) = 0;

   protected:
   const ImporterConfiguration& ImporterConfig;
   const unsigned int           Workers;
   const unsigned int           MaxTransactionSize;
   std::mutex                   Mutex;

   struct WorkerStatistics {
      unsigned long long Processed;
      unsigned long long OldProcessed;
   };
   WorkerStatistics* Statistics;
   SystemTimePoint   LastStatisticsUpdate;
};


// ###### Output operator ###################################################
inline std::ostream& operator<<(std::ostream& os, ReaderBase& reader)
{
   reader.printStatus(os);
   return os;
}


// ###### Reader type-specific base  class ##################################
template<typename ReaderInputFileEntry>
class ReaderImplementation : public ReaderBase
{
   public:
   ReaderImplementation(const ImporterConfiguration& importerConfiguration,
                        const unsigned int           workers,
                        const unsigned int           maxTransactionSize);
   virtual ~ReaderImplementation();

   virtual bool getReaderInputFileEntryForFile(const std::filesystem::path& dataFile, ReaderInputFileEntry& inputFileEntry) const;
   virtual int addFile(const std::filesystem::path& dataFile,
                       const std::smatch            match);
   virtual bool removeFile(const std::filesystem::path& dataFile,
                           const std::smatch            match);
   virtual unsigned int fetchFiles(std::list<std::filesystem::path>& dataFileList,
                                   const unsigned int                worker,
                                   const unsigned int                limit = 1);
   virtual std::filesystem::path getDirectoryHierarchy(const std::filesystem::path& dataFile,
                                                       const std::smatch            match);
   virtual void printStatus(std::ostream& os = std::cout);

   protected:
   std::set<ReaderInputFileEntry>* DataFileSet[ReaderPriority::Max + 1];
};


// ###### Constructor #######################################################
template<typename ReaderInputFileEntry>
ReaderImplementation<ReaderInputFileEntry>::ReaderImplementation(
   const ImporterConfiguration& importerConfiguration,
   const unsigned int           workers,
   const unsigned int           maxTransactionSize)
   : ReaderBase(importerConfiguration, workers, maxTransactionSize)
{
   for(int p = ReaderPriority::Max; p >= 0; p--) {
      DataFileSet[p] = new std::set<ReaderInputFileEntry>[Workers];
      assert(DataFileSet[p] != nullptr);
   }
}


// ###### Destructor ########################################################
template<typename ReaderInputFileEntry>
ReaderImplementation<ReaderInputFileEntry>::~ReaderImplementation()
{
   for(int p = ReaderPriority::Max; p >= 0; p--) {
      delete [] DataFileSet[p];
      DataFileSet[p] = nullptr;
   }
}


// ###### Get ReaderInputFileEntry for file #################################
template<typename ReaderInputFileEntry>
bool ReaderImplementation<ReaderInputFileEntry>::getReaderInputFileEntryForFile(
        const std::filesystem::path& dataFile,
        ReaderInputFileEntry&        inputFileEntry) const
{
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
   if(std::regex_match(filename, match, getFileNameRegExp())) {
      if(makeInputFileEntry(dataFile, match, inputFileEntry, 1) >= 0) {
         return true;
      }
   }
   return false;
}


// ###### Add input file to reader ##########################################
template<typename ReaderInputFileEntry>
int ReaderImplementation<ReaderInputFileEntry>::addFile(
       const std::filesystem::path& dataFile,
       const std::smatch            match)
{
   ReaderInputFileEntry inputFileEntry;
   const int workerID = makeInputFileEntry(dataFile, match, inputFileEntry, Workers);
   if(workerID >= 0) {
      std::unique_lock lock(Mutex);

      // ====== Get priority ================================================
      const ReaderPriority p = getPriorityOfFileEntry(inputFileEntry);

      // ====== Insert file entry into list =================================
      if(DataFileSet[p][workerID].insert(inputFileEntry).second) {
         HPCT_LOG(trace) << getIdentification() << ": Added input file "
                         << relativeTo(dataFile, ImporterConfig.getImportFilePath()) << " to reader";
         return workerID;
      }
   }
   return -1;
}


// ###### Remove input file from reader #####################################
template<typename ReaderInputFileEntry>
bool ReaderImplementation<ReaderInputFileEntry>::removeFile(
        const std::filesystem::path& dataFile,
        const std::smatch            match)
{
   ReaderInputFileEntry inputFileEntry;
   const int workerID = makeInputFileEntry(dataFile, match, inputFileEntry, Workers);
   if(workerID >= 0) {
      HPCT_LOG(trace) << getIdentification() << ": Removing input file "
                      << relativeTo(dataFile, ImporterConfig.getImportFilePath()) << " from reader";
      std::unique_lock lock(Mutex);

      for(int p = ReaderPriority::Max; p >= 0; p--) {
         if(DataFileSet[p][workerID].erase(inputFileEntry) == 1) {
            Statistics[workerID].Processed++;
            Statistics[Workers].Processed++;
            return true;
         }
      }
   }
   return false;
}


// ###### Fetch list of input files #########################################
template<typename ReaderInputFileEntry>
unsigned int ReaderImplementation<ReaderInputFileEntry>::fetchFiles(
                std::list<std::filesystem::path>& dataFileList,
                const unsigned int                worker,
                const unsigned int                limit)
{
   assert(worker < Workers);
   dataFileList.clear();

   std::unique_lock lock(Mutex);

   for(int p = ReaderPriority::Max; p >= 0; p--) {
      for(const ReaderInputFileEntry& inputFileEntry : DataFileSet[p][worker]) {
         // std::cout << dataFileList.size() << ": pri" << p << " " << inputFileEntry.DataFile << "\n";
         dataFileList.push_back(inputFileEntry.DataFile);
         if(dataFileList.size() >= limit) {
            break;
         }
      }
   }
   return dataFileList.size();
}


// ###### Make directory hierarchy from ReaderInputFileEntry ################
template<typename ReaderInputFileEntry>
std::filesystem::path ReaderImplementation<ReaderInputFileEntry>::getDirectoryHierarchy(
   const std::filesystem::path& dataFile,
   const std::smatch            match)
{
   if( (ImporterConfig.getMoveDirectoryDepth() > 0) ||
       (ImporterConfig.getMoveTimestampDepth() > 0) ) {
      ReaderInputFileEntry inputFileEntry;
      const int workerID = makeInputFileEntry(dataFile, match, inputFileEntry, 1);
      if(workerID >= 0) {
         const ReaderTimePoint& timeStamp = inputFileEntry.TimeStamp;
         return makeDirectoryHierarchy(ImporterConfig.getImportFilePath(),
                                       dataFile, timeStamp,
                                       ImporterConfig.getMoveDirectoryDepth(),
                                       ImporterConfig.getMoveTimestampDepth());
      }
   }
   return std::filesystem::path();
}


// ###### Print reader status ###############################################
template<typename ReaderInputFileEntry>
void ReaderImplementation<ReaderInputFileEntry>::printStatus(std::ostream& os)
{
   std::unique_lock lock(Mutex);

   // ====== Prepare total statistics =======================================
   unsigned long long totalWaiting   = 0;
   unsigned long long totalProcessed = 0;
   for(unsigned int w = 0; w < Workers; w++) {
      for(int p = ReaderPriority::Max; p >= 0; p--) {
         totalWaiting +=  DataFileSet[p][w].size();
      }
      totalProcessed += Statistics[w].Processed;
   }
   assert(Statistics[Workers].Processed == totalProcessed);

   const SystemTimePoint now = SystemClock::now();
   const double duration =
      std::chrono::duration_cast<std::chrono::microseconds>(now - LastStatisticsUpdate).count() / 1000000.0;
   const double fps = (Statistics[Workers].Processed - Statistics[Workers].OldProcessed) / duration;
   LastStatisticsUpdate = now;

   const SystemTimePoint estimatedFinishTime = now +
      std::chrono::seconds((unsigned long long)ceil(totalWaiting / fps));


   // ====== Print total statistics =========================================
   os << getIdentification() << ": "
      << Statistics[Workers].Processed - Statistics[Workers].OldProcessed << " total progressed in "
      << (unsigned int)ceil(duration * 1000.0) << " ms, "
      << totalWaiting << " total in queue; ";
   if(totalWaiting > 0) {
      os << "estimated completion at "
         << timePointToString<SystemTimePoint>(estimatedFinishTime, 0, "%Y-%m-%d %H:%M:%S %Z", false) << "\n";
   }
   else {
      os << "idle\n";
   }
   Statistics[Workers].OldProcessed = Statistics[Workers].Processed;

   // ====== Print per-worker statistics ====================================
   for(unsigned int w = 0; w < Workers; w++) {
      if(w > 0) {
         os << "\n";
      }
      os << " - Worker Queue #" << w + 1 << ": "
         << Statistics[w].Processed - Statistics[w].OldProcessed << " progressed, ";
      Statistics[w].OldProcessed = Statistics[w].Processed;
      for(int p = ReaderPriority::Max; p >= 0; p--) {
         os << DataFileSet[p][w].size() << " (pri" << p << ")"
            << ((p > 0) ? " / " : " in queue");

         // os <<"\n";
         // for(const ReaderInputFileEntry& inputFileEntry : DataFileSet[p][w]) {
         //    os << "  - " <<  inputFileEntry << "\n";
         // }
      }
   }
}

#endif
