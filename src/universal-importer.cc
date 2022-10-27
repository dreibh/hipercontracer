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

#include "universal-importer.h"
#include "database-configuration.h"
#include "logger.h"
#include "worker.h"



// ###### < operator for sorting ############################################
bool operator<(const UniversalImporter::WorkerMapping& a,
               const UniversalImporter::WorkerMapping& b) {
   // ====== Level 1: Reader ================================================
   if(a.Reader < b.Reader) {
      return true;
   }
   else if(a.Reader == b.Reader) {
      // ====== Level 2: WorkerID ===========================================
      if(a.WorkerID < b.WorkerID) {
         return true;
      }
   }
   return false;
}


// ###### Constructor #######################################################
UniversalImporter::UniversalImporter(boost::asio::io_service&     ioService,
                                     const std::filesystem::path& importFilePath,
                                     const std::filesystem::path& goodFilePath,
                                     const std::filesystem::path& badFilePath,
                                     const ImportModeType         importMode,
                                     const unsigned int           maxDepth)
 : IOService(ioService),
   ImportFilePath(std::filesystem::canonical(std::filesystem::absolute(importFilePath))),
   GoodFilePath(std::filesystem::canonical(std::filesystem::absolute(goodFilePath))),
   BadFilePath(std::filesystem::canonical(std::filesystem::absolute(badFilePath))),
   ImportMode(importMode),
   MaxDepth(maxDepth),
   Signals(IOService, SIGINT, SIGTERM),
   INotifyStream(IOService)
{
#ifdef __linux__
   INotifyFD = -1;
#endif
}


// ###### Destructor ########################################################
UniversalImporter::~UniversalImporter()
{
   stop();
}


// ###### Start importer ####################################################
bool UniversalImporter::start()
{
   // ====== Intercept signals ==============================================
   Signals.async_wait(std::bind(&UniversalImporter::handleSignalEvent, this,
                                std::placeholders::_1,
                                std::placeholders::_2));

   // ====== Set up INotify =================================================
#ifdef __linux__
   INotifyFD = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
   assert(INotifyFD > 0);
   INotifyStream.assign(INotifyFD);
   int wd = inotify_add_watch(INotifyFD, ImportFilePath.c_str(),
                              IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
   if(wd < 0) {
      HPCT_LOG(error) << "Unable to configure inotify: " <<strerror(errno);
      return false;
   }
   INotifyWatchDescriptors.insert(wd);

   INotifyStream.async_read_some(boost::asio::buffer(&INotifyEventBuffer, sizeof(INotifyEventBuffer)),
                                 std::bind(&UniversalImporter::handleINotifyEvent, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
#else
#error FIXME!
#endif

   // ====== Look for files =================================================
   HPCT_LOG(info) << "Looking for input files ...";
   lookForFiles();
   printStatus();

   // ====== Start workers ==================================================
   HPCT_LOG(info) << "Starting " << WorkerMap.size() << " worker threads ...";
   for(std::map<const WorkerMapping, Worker*>::iterator workerMappingIterator = WorkerMap.begin();
       workerMappingIterator != WorkerMap.end(); workerMappingIterator++) {
      Worker* worker = workerMappingIterator->second;
      worker->start();
   }


   return true;
}


// ###### Stop importer #####################################################
void UniversalImporter::stop()
{
   // ====== Remove INotify =================================================
#ifdef __linux__
   if(INotifyFD >= 0) {
      std::set<int>::iterator iterator = INotifyWatchDescriptors.begin();
      while(iterator != INotifyWatchDescriptors.end()) {
         inotify_rm_watch(INotifyFD, *iterator);
         INotifyWatchDescriptors.erase(iterator);
         iterator = INotifyWatchDescriptors.begin();
      }
      close(INotifyFD);
      INotifyFD = -1;
   }
#endif

   // ====== Remove readers =================================================
   for(std::list<ReaderBase*>::iterator readerIterator = ReaderList.begin(); readerIterator != ReaderList.end(); ) {
      removeReader(**readerIterator);
      readerIterator = ReaderList.begin();
   }
}


// ###### Handle signal #####################################################
void UniversalImporter::handleSignalEvent(const boost::system::error_code& ec,
                                          const int                        signalNumber)
{
   if(ec != boost::asio::error::operation_aborted) {
      puts("\n*** Shutting down! ***\n");   // Avoids a false positive from Helgrind.
      IOService.stop();
   }
}


#ifdef __linux
// ###### Handle signal #####################################################
void UniversalImporter::handleINotifyEvent(const boost::system::error_code& ec,
                                           const std::size_t                length)
{
   // ====== Handle events ==================================================
   unsigned long p = 0;
   while(p < length) {
      const inotify_event* event = (const inotify_event*)&INotifyEventBuffer[p];

      // ====== Event for directory =========================================
      if(event->mask & IN_ISDIR) {
         if(event->mask & IN_CREATE) {
            const std::filesystem::path importFilePath = ImportFilePath / event->name;
            HPCT_LOG(trace) << "INotify for new data directory: " << importFilePath;
            const int wd = inotify_add_watch(INotifyFD, importFilePath.c_str(),
                                             IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
            INotifyWatchDescriptors.insert(wd);
         }
         else if(event->mask & IN_DELETE) {
            const std::filesystem::path importFilePath = ImportFilePath / event->name;
            HPCT_LOG(trace) << "INotify for deleted data directory: " << importFilePath;
            INotifyWatchDescriptors.erase(event->wd);
         }
      }

      // ====== Event for file ==============================================
      else {
         if(event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
            const std::filesystem::path dataFile(event->name);
            HPCT_LOG(trace) << "INotify event for new file " << dataFile;
            addFile(dataFile);
         }
         else if(event->mask & IN_DELETE) {
            const std::filesystem::path dataFile(event->name);
            HPCT_LOG(trace) << "INotify event for deleted file " << dataFile;
            removeFile(dataFile);
         }
      }

      p += sizeof(inotify_event) + event->len;
   }

   // ====== Wait for more events ===========================================
   INotifyStream.async_read_some(boost::asio::buffer(&INotifyEventBuffer, sizeof(INotifyEventBuffer)),
                                 std::bind(&UniversalImporter::handleINotifyEvent, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
}
#endif


// ###### Add reader ########################################################
void UniversalImporter::addReader(ReaderBase&          reader,
                                  DatabaseClientBase** databaseClientArray,
                                  const size_t         databaseClients)
{
   ReaderList.push_back(&reader);
   for(unsigned int w = 0; w < databaseClients; w++) {
      Worker* worker = new Worker(w, reader, *databaseClientArray[w],
                                  ImportFilePath, GoodFilePath, BadFilePath,
                                  ImportMode);
      assert(worker != nullptr);
      WorkerMapping workerMapping;
      workerMapping.Reader   = &reader;
      workerMapping.WorkerID = w;
      WorkerMap.insert(std::pair<const WorkerMapping, Worker*>(workerMapping, worker));
   }
}


// ###### Remove reader #####################################################
void UniversalImporter::removeReader(ReaderBase& reader)
{
   // ====== Remove Reader from reader list =================================
   for(std::list<ReaderBase*>::iterator readerIterator = ReaderList.begin();
       readerIterator != ReaderList.end();
       readerIterator++) {
      if(*readerIterator == &reader) {
         ReaderList.erase(readerIterator);
         break;
      }
   }

   // ====== Remove worker mapping ==========================================
   for(std::map<const WorkerMapping, Worker*>::iterator workerMappingIterator = WorkerMap.begin();
       workerMappingIterator != WorkerMap.end(); ) {
      if(workerMappingIterator->first.Reader == &reader) {
         delete workerMappingIterator->second;
         workerMappingIterator = WorkerMap.erase(workerMappingIterator);
      }
      else {
         workerMappingIterator++;
      }
   }
}


// ###### Look for input files (full directory traversal) ###################
void UniversalImporter::lookForFiles()
{
   lookForFiles(ImportFilePath, MaxDepth);
}


// ###### Look for input files (limited directory traversal) ################
void UniversalImporter::lookForFiles(const std::filesystem::path& importFilePath,
                                     const unsigned int           maxDepth)
{
   for(const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(importFilePath)) {
      if(dirEntry.is_regular_file()) {
         addFile(dirEntry.path());
      }
      else if(dirEntry.is_directory()) {
#ifdef __linux
         const int wd = inotify_add_watch(INotifyFD, dirEntry.path().c_str(),
                                          IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
         INotifyWatchDescriptors.insert(wd);
#endif
         if(maxDepth > 1) {
            lookForFiles(dirEntry, maxDepth - 1);
         }
      }
   }
}


// ###### Add input file ####################################################
bool UniversalImporter::addFile(const std::filesystem::path& dataFile)
{
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
   for(ReaderBase* reader : ReaderList) {
      if(std::regex_match(filename, match, reader->getFileNameRegExp())) {
         const int worker = reader->addFile(dataFile, match);
         if(worker >= 0) {
            WorkerMapping workerMapping;
            workerMapping.Reader   = reader;
            workerMapping.WorkerID = worker;
            std::map<const WorkerMapping, Worker*>::iterator found = WorkerMap.find(workerMapping);
            if(found != WorkerMap.end()) {
               Worker* worker = found->second;
               worker->wakeUp();
               return true;
            }
         }
      }
   }
   return false;
}


// ###### Add input file ####################################################
bool UniversalImporter::removeFile(const std::filesystem::path& dataFile)
{
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
   for(ReaderBase* reader : ReaderList) {
      if(std::regex_match(filename, match, reader->getFileNameRegExp())) {
         if(reader->removeFile(dataFile, match)) {
            return true;
         }
         break;
      }
   }
   return false;
}


// ###### Print importer status #############################################
void UniversalImporter::printStatus(std::ostream& os)
{
   for(ReaderBase* reader : ReaderList) {
      reader->printStatus(os);
   }
}
