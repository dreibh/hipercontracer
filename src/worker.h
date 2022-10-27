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

#ifndef WORKER_H
#define WORKER_H

#include "databaseclient-base.h"
#include "reader-base.h"

#include <thread>
#include <condition_variable>


class Worker
{
   public:
   Worker(const unsigned int           workerID,
          ReaderBase&                  reader,
          DatabaseClientBase&          databaseClient,
          const std::filesystem::path& importFilePath,
          const std::filesystem::path& goodFilePath,
          const std::filesystem::path& badFilePath,
          const ImportModeType         importMode);
   ~Worker();

   void start();
   void requestStop();
   void wakeUp();

   inline const std::string& getIdentification() const { return Identification; }


   private:
   void processFile(DatabaseClientBase&          databaseClient,
                    unsigned long long&          rows,
                    const std::filesystem::path& dataFile);
   void finishedFile(const std::filesystem::path& dataFile,
                     const bool                   success = true);
   void deleteEmptyDirectories(std::filesystem::path path);
   void deleteImportedFile(const std::filesystem::path& dataFile);
   void moveImportedFile(const std::filesystem::path& dataFile);
   void moveBadFile(const std::filesystem::path& dataFile);
   bool importFiles(const std::list<std::filesystem::path>& dataFileList);
   void run();

   std::atomic<bool>            StopRequested;
   const unsigned int           WorkerID;
   ReaderBase&                  Reader;
   DatabaseClientBase&          DatabaseClient;
   const std::filesystem::path& ImportFilePath;
   const std::filesystem::path& GoodFilePath;
   const std::filesystem::path& BadFilePath;
   const ImportModeType         ImportMode;
   const std::string            Identification;
   std::thread                  Thread;
   std::mutex                   Mutex;
   std::condition_variable      Notification;
};

#endif
