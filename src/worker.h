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

#ifndef WORKER_H
#define WORKER_H

#include "databaseclient-base.h"
#include "importer-configuration.h"
#include "reader-base.h"

#include <atomic>
#include <thread>
#include <condition_variable>


class Worker
{
   public:
   Worker(const unsigned int           workerID,
          ReaderBase&                  reader,
          const ImporterConfiguration& importerConfiguration,
          const DatabaseConfiguration& databaseConfiguration,
          DatabaseClientBase&          databaseClient);
   ~Worker();

   void start(const bool quitWhenIdle = false);
   void join();
   void requestStop();
   void wakeUp();

   inline const std::string& getIdentification() const { return Identification; }


   private:
   void processFile(DatabaseClientBase&          databaseClient,
                    unsigned long long&          rows,
                    const std::filesystem::path& dataFile);
   void finishedFile(const std::filesystem::path& dataFile,
                     const bool                   success = true);
   void deleteImportedFile(const std::filesystem::path& dataFile);
   void moveImportedFile(const std::filesystem::path& dataFile,
                         const std::smatch            match,
                         const bool                   isGood);

   bool importFiles(const std::list<std::filesystem::path>& dataFileList);
   void run();

   std::atomic<bool>            StopRequested;
   const unsigned int           WorkerID;
   ReaderBase&                  Reader;
   DatabaseClientBase&          DatabaseClient;
   const ImporterConfiguration& ImporterConfig;
   const DatabaseConfiguration& DatabaseConfig;
   const std::string            Identification;
   std::thread                  Thread;
   std::mutex                   Mutex;
   std::condition_variable      Notification;
   bool                         QuitWhenIdle;
};

#endif
