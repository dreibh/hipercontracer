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

#ifndef UNIVERSAL_IMPORTER_H
#define UNIVERSAL_IMPORTER_H

#include <boost/asio.hpp>

#ifdef __linux__
#include <sys/inotify.h>
#endif

#include "databaseclient-base.h"
#include "reader-base.h"


class Worker;

class UniversalImporter
{
   public:
   UniversalImporter(boost::asio::io_service&     ioService,
                     const DatabaseConfiguration& databaseConfiguration,
                     const unsigned int           statusTimerInterval = 60);
   ~UniversalImporter();

   void addReader(ReaderBase&          reader,
                  DatabaseClientBase** databaseClientArray,
                  const size_t         databaseClients);
   void removeReader(ReaderBase& reader);
   void lookForFiles(const std::string& importFilePathFilter = std::string());
   bool start(const std::string& importFilePathFilter = std::string());
   void stop();
   void run();

   friend std::ostream& operator<<(std::ostream& os, const UniversalImporter& importer);

   private:
   void handleSignalEvent(const boost::system::error_code& ec,
                          const int                        signalNumber);
#ifdef __linux__
   void handleINotifyEvent(const boost::system::error_code& ec,
                           const std::size_t                length);
#endif
   unsigned long long lookForFiles(const std::filesystem::path& importFilePath,
                                   const unsigned int           currentDepth,
                                   const unsigned int           maxDepth,
                                   const std::regex&            directoryFilterRegExp);
   bool addFile(const std::filesystem::path& dataFile);
   bool removeFile(const std::filesystem::path& dataFile);
   void handleStatusTimer(const boost::system::error_code& errorCode);

   struct WorkerMapping {
      ReaderBase*  Reader;
      unsigned int WorkerID;
   };
   friend bool operator<(const UniversalImporter::WorkerMapping& a,
                         const UniversalImporter::WorkerMapping& b);

   boost::asio::io_service&               IOService;
   const DatabaseConfiguration&           Configuration;

   boost::asio::signal_set                Signals;
   std::list<ReaderBase*>                 ReaderList;
   std::map<const WorkerMapping, Worker*> WorkerMap;
   boost::asio::deadline_timer            StatusTimer;
   const boost::posix_time::seconds       StatusTimerInterval;
#ifdef __linux__
   int                                    INotifyFD;
   std::map<int, std::filesystem::path>   INotifyWatchDescriptors;
   boost::asio::posix::stream_descriptor  INotifyStream;
   char                                   INotifyEventBuffer[65536 * sizeof(inotify_event)];
#endif
};

#endif
