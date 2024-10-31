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

#ifndef UNIVERSAL_IMPORTER_H
#define UNIVERSAL_IMPORTER_H

#include "databaseclient-base.h"
#include "importer-configuration.h"
#include "reader-base.h"

#include <boost/asio.hpp>
#include <boost/bimap.hpp>

#include <sys/inotify.h>


typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemDuration;

typedef std::chrono::file_clock              FileClock;
typedef FileClock::time_point                FileTimePoint;
typedef FileClock::duration                  FileDuration;


class Worker;

class UniversalImporter
{
   public:
   UniversalImporter(boost::asio::io_service&     ioService,
                     const ImporterConfiguration& importerConfiguration,
                     const DatabaseConfiguration& databaseConfiguration,
                     const unsigned int           statusTimerInterval            = 60,
                     const unsigned int           garbageCollectionTimerInterval = 33);
   ~UniversalImporter();

   void addReader(ReaderBase&          reader,
                  DatabaseClientBase** databaseClientArray,
                  const size_t         databaseClients);
   void removeReader(ReaderBase& reader);
   void lookForFiles();
   bool start(const bool quitWhenIdle = false);
   void stop();
   void waitForFinish();
   void run();

   friend std::ostream& operator<<(std::ostream&            os,
                                   const UniversalImporter& importer);

   private:
   void handleSignalEvent(const boost::system::error_code& errorCode,
                          const int                        signalNumber);
   void handleINotifyEvent(const boost::system::error_code& errorCode,
                           const std::size_t                length);
   unsigned long long lookForFiles(const std::filesystem::path& importFilePath,
                                   const unsigned int           currentDepth,
                                   const unsigned int           maxDepth);
   bool addFile(const std::filesystem::path& dataFile);
   bool removeFile(const std::filesystem::path& dataFile);
   void handleStatusTimer(const boost::system::error_code& errorCode);

   void addGarbageDirectory(const std::filesystem::path directory);
   void removeGarbageDirectory(const std::filesystem::path directory);
   void performGarbageDirectoryCleanUp(const SystemDuration& maxAge);
   void handleGarbageCollectionTimer(const boost::system::error_code& errorCode);

   struct WorkerMapping {
      ReaderBase*  Reader;
      unsigned int WorkerID;
   };
   friend bool operator<(const UniversalImporter::WorkerMapping& a,
                         const UniversalImporter::WorkerMapping& b);

   boost::asio::io_service&                 IOService;
   const ImporterConfiguration&             ImporterConfig;
   const DatabaseConfiguration&             DatabaseConfig;
   const bool                               HasImportPathFilter;
   const std::string                        ImportPathFilter;
   const std::regex                         ImportPathFilterRegEx;

   boost::asio::signal_set                  Signals;
   std::list<ReaderBase*>                   ReaderList;
   std::map<const WorkerMapping, Worker*>   WorkerMap;
   boost::asio::deadline_timer              StatusTimer;
   const boost::posix_time::seconds         StatusTimerInterval;
   std::map<const std::filesystem::path,
            SystemTimePoint>                GarbageDirectoryMap;
   boost::asio::deadline_timer              GarbageCollectionTimer;
   const boost::posix_time::seconds         GarbageCollectionTimerInterval;
   int                                      INotifyFD;
   boost::bimap<int, std::filesystem::path> INotifyWatchDescriptors;
   boost::asio::posix::stream_descriptor    INotifyStream;
   char                                     INotifyEventBuffer[65536 * sizeof(inotify_event)];
};

#endif
