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
// Copyright (C) 2015-2026 by Thomas Dreibholz
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
#include "tools.h"
#include "worker.h"

#include <boost/filesystem/operations.hpp>


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
UniversalImporter::UniversalImporter(boost::asio::io_context&     ioContext,
                                     const ImporterConfiguration& importerConfiguration,
                                     const DatabaseConfiguration& databaseConfiguration)
 : IOContext(ioContext),
   ImporterConfig(importerConfiguration),
   DatabaseConfig(databaseConfiguration),
   HasImportPathFilter(ImporterConfig.getImportPathFilter().size() > 0),
   ImportPathFilter("^(" + (ImporterConfig.getImportFilePath() / ")(").string() + ImporterConfig.getImportPathFilter() + ")(.*)$"),
   ImportPathFilterRegEx(ImportPathFilter),
   Signals(IOContext, SIGINT, SIGTERM),
   StatusTimer(IOContext),
   StatusTimerInterval(std::chrono::seconds(importerConfiguration.getStatusInterval())),
   GarbageCollectionTimer(IOContext),
   GarbageCollectionTimerInterval(std::chrono::seconds(importerConfiguration.getGarbageCollectionInterval())),
   GarbageCollectionMaxAge(std::chrono::seconds(importerConfiguration.getGarbageCollectionMaxAge())),
   WatchStream(IOContext)
{
   WatchFD = -1;
   StatusTimer.expires_at(std::chrono::steady_clock::now() + +
                          StatusTimerInterval);
   StatusTimer.async_wait(std::bind(&UniversalImporter::handleStatusTimer, this,
                                    std::placeholders::_1));
   GarbageCollectionTimer.expires_at(std::chrono::steady_clock::now() +
                                     GarbageCollectionTimerInterval);
   GarbageCollectionTimer.async_wait(std::bind(&UniversalImporter::handleGarbageCollectionTimer, this,
                                    std::placeholders::_1));
}


// ###### Destructor ########################################################
UniversalImporter::~UniversalImporter()
{
   stop();
}


// ###### Add directory watch ###############################################
int UniversalImporter::addDirectoryWatch(const std::filesystem::path& directoryPath)
{
#if defined(__sun__)
   static int sunWatchIdCounter = 1;
   int handle = sunWatchIdCounter++;

   file_obj_t fileObject{};
   fileObject.fo_name = strdup(directoryPath.c_str());
   struct stat dirStat;
   if(stat(fileObject.fo_name, &dirStat) == 0) {
      fileObject.fo_atime = dirStat.st_atim;
      fileObject.fo_mtime = dirStat.st_mtim;
      fileObject.fo_ctime = dirStat.st_ctim;
   }

   SolarisFileObjects[handle] = fileObject;
   if(port_associate(WatchFD, PORT_SOURCE_FILE, (uintptr_t)&SolarisFileObjects[handle],
                     FILE_MODIFIED | FILE_ATTRIB, (void*)(uintptr_t)handle) < 0) {
      free(fileObject.fo_name);
      SolarisFileObjects.erase(handle);
      return -1;
   }
   return handle;

#elif defined(__APPLE__)
   const int dirFD = open(directoryPath.c_str(), O_EVTONLY | O_CLOEXEC);
   if(dirFD < 0) {
      return -1;
   }

   struct kevent kEvent;
   EV_SET(&kEvent, dirFD, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
          NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_DELETE | NOTE_LINK, 0, NULL);

   if(kevent(WatchFD, &kEvent, 1, NULL, 0, NULL) < 0) {
      close(dirFD);
      return -1;
   }
   return dirFD;

#else
   return inotify_add_watch(WatchFD, directoryPath.c_str(),
                            IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
#endif
}


// ###### Remove Directory Watch (Platform Abstraction) #####################
void UniversalImporter::removeDirectoryWatch(int                          watchHandle,
                                             const std::filesystem::path& directoryPath)
{
#if defined(__sun__)
   std::map<int, file_obj_t>::iterator iterator = SolarisFileObjects.find(watchHandle);
   if(iterator != SolarisFileObjects.end()) {
      port_dissociate(WatchFD, PORT_SOURCE_FILE, (uintptr_t)&iterator->second);
      free(iterator->second.fo_name);
      SolarisFileObjects.erase(iterator);
   }

#elif defined(__APPLE__)
   close(watchHandle);

#else
   inotify_rm_watch(WatchFD, watchHandle);
#endif
}


// ###### Start importer ####################################################
bool UniversalImporter::start(const bool quitWhenIdle)
{
   // ====== Intercept signals ==============================================
   Signals.async_wait(std::bind(&UniversalImporter::handleSignalEvent, this,
                                std::placeholders::_1,
                                std::placeholders::_2));

   // ====== Set up Watch =================================================
#if defined(__sun__)
   WatchFD = port_create();
   assert(WatchFD > 0);
   fcntl(WatchFD, F_SETFD, FD_CLOEXEC);
#elif defined(__APPLE__)
   WatchFD = kqueue();
   assert(WatchFD > 0);
   fcntl(WatchFD, F_SETFD, FD_CLOEXEC);
#else
   WatchFD = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
   assert(WatchFD > 0);
#endif

   WatchStream.assign(WatchFD);
   const int wd = addDirectoryWatch(ImporterConfig.getImportFilePath());
   if(wd < 0) {
      HPCT_LOG(error) << "Adding watch for " << ImporterConfig.getImportFilePath()
                      << " failed: " << strerror(errno);
      return false;
   }
   WatchDescriptors.insert(boost::bimap<int, std::filesystem::path>::value_type(
                                  wd, ImporterConfig.getImportFilePath()));

   WatchStream.async_read_some(boost::asio::buffer(&WatchEventBuffer, sizeof(WatchEventBuffer)),
                                 std::bind(&UniversalImporter::handleWatchEvent, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));

   // ====== Look for files =================================================
   HPCT_LOG(info) << "Performing initial directory traversal to look for input files ...";
   lookForFiles();
   HPCT_LOG(info) << "Importer status after initial directory traversal:\n" << *this;

   // ====== Start workers ==================================================
   HPCT_LOG(info) << "Starting " << WorkerMap.size() << " worker threads ...";
   for(std::map<const WorkerMapping, Worker*>::iterator workerMappingIterator = WorkerMap.begin();
       workerMappingIterator != WorkerMap.end(); workerMappingIterator++) {
      Worker* worker = workerMappingIterator->second;
      worker->start(quitWhenIdle);
   }

   // ====== Quit when idle? ================================================
   if(quitWhenIdle) {
      WatchStream.cancel();
      StatusTimer.cancel();
      GarbageCollectionTimer.cancel();
      Signals.cancel();
   }

   return true;
}


// ###### Stop importer #####################################################
void UniversalImporter::stop()
{
   StatusTimer.cancel();

   // ====== Remove Watch =================================================
   if(WatchFD >= 0) {
      boost::bimap<int, std::filesystem::path>::iterator iterator = WatchDescriptors.begin();
      while(iterator != WatchDescriptors.end()) {
         removeDirectoryWatch(iterator->left, iterator->right);
         removeLastWriteTimePoint(iterator->right);
         WatchDescriptors.erase(iterator);
         iterator = WatchDescriptors.begin();
      }
      close(WatchFD);
      WatchFD = -1;
   }
   assert(WatchDescriptors.size() == WatchLastWrite.size());   // == 0!

   // ====== Remove readers =================================================
   for(std::list<ReaderBase*>::iterator readerIterator = ReaderList.begin(); readerIterator != ReaderList.end(); ) {
      removeReader(**readerIterator);
      readerIterator = ReaderList.begin();
   }
}


// ###### Wait for worker threads to be finished ############################
void UniversalImporter::waitForFinish()
{
   // NOTE: To finish, the worker threads must have a stop criteria, i.e.
   //       quitWhenIdle == true!
   for(std::map<const WorkerMapping, Worker*>::iterator workerMappingIterator = WorkerMap.begin();
       workerMappingIterator != WorkerMap.end(); workerMappingIterator++) {
      Worker* worker = workerMappingIterator->second;
      worker->join();
   }
   HPCT_LOG(info) << "Importer final status:\n" << *this;
   stop();
}


// ###### Handle signal #####################################################
void UniversalImporter::handleSignalEvent(const boost::system::error_code& errorCode,
                                          const int                        signalNumber)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      puts("\n*** Shutting down! ***\n");   // Avoids a false positive from Helgrind.
      IOContext.stop();
   }
}


// ###### Handle signal #####################################################
void UniversalImporter::handleWatchEvent(const boost::system::error_code& errorCode,
                                           const std::size_t                length)
{
   if(errorCode != boost::asio::error::operation_aborted) {

#if defined(__sun__)
      // ====== Solaris FEN Handling ========================================
      port_event_t events[16];
      uint_t       numEvents     = 16;
      timespec     nullTimestamp = { 0, 0 };
      if(port_getn(WatchFD, events, 16, &numEvents, &nullTimestamp) == 0) {
         for(uint_t i = 0; i < numEvents; i++) {
            if(events[i].portev_source == PORT_SOURCE_FILE) {
               int handle = (int)(uintptr_t)events[i].portev_user;
               boost::bimap<int, std::filesystem::path>::left_map::const_iterator found =
                  WatchDescriptors.left.find(handle);
               if(found != WatchDescriptors.left.end()) {
                  const std::filesystem::path& directory = found->second;

                  // Solaris FEN is one-shot => re-associate immediately:
                  std::map<int, file_obj_t>::iterator fileObjectIterator = SolarisFileObjects.find(handle);
                  if(fileObjectIterator != SolarisFileObjects.end()) {
                     struct stat s;
                     if(stat(fileObjectIterator->second.fo_name, &s) == 0) {
                        fileObjectIterator->second.fo_atime = s.st_atim;
                        fileObjectIterator->second.fo_mtime = s.st_mtim;
                        fileObjectIterator->second.fo_ctime = s.st_ctim;
                     }
                     port_associate(WatchFD, PORT_SOURCE_FILE, (uintptr_t)&fileObjectIterator->second,
                                    FILE_MODIFIED | FILE_ATTRIB, (void*)(uintptr_t)handle);
                  }

                  // Rescan directory for modified/added files:
                  const unsigned int currentDepth = subDirectoryOf(directory, ImporterConfig.getImportFilePath());
                  lookForFiles(directory, currentDepth > 0 ? currentDepth : 1, ImporterConfig.getImportMaxDepth());
               }
            }
         }
      }

#elif defined(__APPLE__)
      // ====== Apple kqueue Handling =======================================
      struct kevent   events[16];
      struct timespec nullTimestamp = { 0, 0 };
      int numEvents = kevent(WatchFD, NULL, 0, events, 16, &nullTimestamp);

      for(int i = 0; i < numEvents; i++) {
         int dirFD = (int)events[i].ident;
         boost::bimap<int, std::filesystem::path>::left_map::const_iterator found =
            WatchDescriptors.left.find(dirFD);
         if(found != WatchDescriptors.left.end()) {
            const std::filesystem::path& directory = found->second;
            // MacOS kqueue does not provide individual child filenames for
            // directory changes => execute directory traversal:
            const unsigned int currentDepth = subDirectoryOf(directory, ImporterConfig.getImportFilePath());
            lookForFiles(directory, currentDepth > 0 ? currentDepth : 1, ImporterConfig.getImportMaxDepth());
         }
      }

#else
      unsigned long p = 0;
      while(p < length) {
         const inotify_event* event = (const inotify_event*)&WatchEventBuffer[p];
         boost::bimap<int, std::filesystem::path>::left_map::const_iterator found = WatchDescriptors.left.find(event->wd);
         if(found != WatchDescriptors.left.end()) {
            if(event->name[0] != '.') {   // Ignore hidden file or directory (starting with '.').
               const std::filesystem::path& directory = found->second;

               // ====== Event for directory ================================
               if(event->mask & IN_ISDIR) {
                  const std::filesystem::path dataDirectory = directory / std::string(event->name);
                  if(event->mask & IN_CREATE) {
                     HPCT_LOG(trace) << "Watch event for new directory: " << dataDirectory;
                     const int wd = inotify_add_watch(WatchFD, dataDirectory.c_str(),
                                                      IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
                     if(wd >= 0) {
                        WatchDescriptors.insert(boost::bimap<int, std::filesystem::path>::value_type(wd, dataDirectory));
                        addOrUpdateLastWriteTimePoint(dataDirectory);

                        // A directory traversal is necessary in this new
                        // directory, since files/directories may have been
                        // created before adding the watch!
                        const unsigned int currentDepth = subDirectoryOf(dataDirectory, ImporterConfig.getImportFilePath());
                        if(currentDepth > 0) {
                           HPCT_LOG(debug) << "Looking for input files in new directory " << dataDirectory
                                          << " (depth " << 1 + currentDepth << " of " << ImporterConfig.getImportMaxDepth()
                                          << ", filter " << ImportPathFilter << ") ...";
                           lookForFiles(dataDirectory,
                                       1 + currentDepth, ImporterConfig.getImportMaxDepth());
                        }
                        else {
                           HPCT_LOG(error) << "Not a subdirectory of the import path: " << dataDirectory;
                        }
                     }
                     else {
                        HPCT_LOG(error) << "Adding watch for " << dataDirectory
                                       << " failed: " << strerror(errno);
                     }
                  }
                  else if(event->mask & IN_DELETE) {
                     HPCT_LOG(trace) << "Watch event for deleted directory: " << dataDirectory;
                     boost::bimap<int, std::filesystem::path>::right_map::const_iterator wdToDelete = WatchDescriptors.right.find(dataDirectory);
                     if(wdToDelete != WatchDescriptors.right.end()) {
                        removeLastWriteTimePoint(dataDirectory);
                        removeDirectoryWatch(wdToDelete->second, dataDirectory);
                        WatchDescriptors.left.erase(wdToDelete->second);
                     }
                  }
               }

               // ====== Event for file =====================================
               else {
                  const std::filesystem::path dataFile = directory / std::string(event->name);
                  if(event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                     HPCT_LOG(trace) << "Watch event for new file " << dataFile;
                     addFile(dataFile);
                  }
                  else if(event->mask & IN_DELETE) {
                     HPCT_LOG(trace) << "Watch event for deleted file " << dataFile;
                     removeFile(dataFile);
                  }
               }
            }
         }
         p += sizeof(inotify_event) + event->len;
      }
#endif

      // ====== Wait for more events ========================================
      WatchStream.async_read_some(boost::asio::buffer(&WatchEventBuffer, sizeof(WatchEventBuffer)),
                                    std::bind(&UniversalImporter::handleWatchEvent, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
   }
}


// ###### Add reader ########################################################
void UniversalImporter::addReader(ReaderBase&          reader,
                                  DatabaseClientBase** databaseClientArray,
                                  const size_t         databaseClients)
{
   ReaderList.push_back(&reader);
   for(unsigned int w = 0; w < databaseClients; w++) {
      Worker* worker = new Worker(w, reader,
                                  ImporterConfig, DatabaseConfig,
                                  *databaseClientArray[w]);
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
   HPCT_LOG(info) << "Looking for input files in directory " << ImporterConfig.getImportFilePath()
                  << " (filter \"" << ImportPathFilter << "\") ...";
   lookForFiles(ImporterConfig.getImportFilePath(),
                1, ImporterConfig.getImportMaxDepth());
}


// ###### Look for input files (limited directory traversal) ################
unsigned long long UniversalImporter::lookForFiles(const std::filesystem::path& importFilePath,
                                                   const unsigned int           currentDepth,
                                                   const unsigned int           maxDepth)
{
   std::smatch match;
   unsigned long long n = 0;
   for(const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(importFilePath)) {

      // ====== Filter name =================================================
      // Optimisation: only check if there actually is a filter!
      if(HasImportPathFilter) {
         const std::string d = (dirEntry.path() / "").string();
         if(!std::regex_match(d, match, ImportPathFilterRegEx)) {
            HPCT_LOG(info) << "Skipping " << d;
            continue;
         }
      }

      // ====== Add file ====================================================
      if(dirEntry.is_regular_file()) {
         addFile(dirEntry.path());
         n++;
      }

      // ====== Add directory ===============================================
      else if(dirEntry.is_directory()) {
         // Check if directory is already watched to prevent FD leaks:
         boost::bimap<int, std::filesystem::path>::right_map::const_iterator existingWatch =
            WatchDescriptors.right.find(dirEntry.path());
         if(existingWatch == WatchDescriptors.right.end()) {
            const int wd = addDirectoryWatch(dirEntry.path());
            if(wd >= 0) {
               WatchDescriptors.insert(boost::bimap<int, std::filesystem::path>::value_type(wd, dirEntry.path()));
               addOrUpdateLastWriteTimePoint(dirEntry.path());
            }
            else {
               HPCT_LOG(error) << "Adding watch for " << dirEntry.path()
                               << " failed: " << strerror(errno);
            }
         }

         // ------ Recursive directory traversal ----------------------------
         if(currentDepth < maxDepth) {
            const unsigned long long m = lookForFiles(dirEntry.path(), currentDepth + 1, maxDepth);
            n += m;
         }
      }
   }
   return n;
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


// ###### Remove input file #################################################
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


// ###### Get time point of last write to directory or file #################
bool UniversalImporter::getLastWriteTimePoint(const std::filesystem::path path,
                                              SystemTimePoint&            lastWriteTimePoint)
{
    try {
       const time_t lastWriteTimeT = boost::filesystem::last_write_time(boost::filesystem::path(path));
       lastWriteTimePoint = std::chrono::system_clock::from_time_t(lastWriteTimeT);
       return true;
    }
    catch(...) { }
    return false;
}


// ###### Add directory to garbage collector ################################
void UniversalImporter::addOrUpdateLastWriteTimePoint(const std::filesystem::path directory)
{
   assert(directory != ImporterConfig.getImportFilePath());

   SystemTimePoint lastWriteTimePoint;
   if(getLastWriteTimePoint(directory, lastWriteTimePoint)) {
      std::map<const std::filesystem::path, SystemTimePoint>::iterator found =
         WatchLastWrite.find(directory);
      if(found != WatchLastWrite.end()) {
         found->second = lastWriteTimePoint;
      }
      else {
         WatchLastWrite.insert(std::pair<const std::filesystem::path, SystemTimePoint>(
                              directory, lastWriteTimePoint));
      }
   }
}


// ###### Remove directory from garbage collector ###########################
void UniversalImporter::removeLastWriteTimePoint(const std::filesystem::path directory)
{
   std::map<const std::filesystem::path, SystemTimePoint>::iterator found =
      WatchLastWrite.find(directory);
   if(found != WatchLastWrite.end()) {
      WatchLastWrite.erase(found);
   }
}


// ###### Perform directory garbage collection ##############################
void UniversalImporter::performDirectoryCleanUp()
{
   const SystemTimePoint now       = SystemClock::now();
   const SystemTimePoint threshold = now - GarbageCollectionMaxAge;
   HPCT_LOG(debug) << "Performing directory clean-up of directories older than "
                   << timePointToString<SystemTimePoint>(threshold);

   size_t n = 0;
   std::map<const std::filesystem::path, SystemTimePoint>::reverse_iterator iterator =
      WatchLastWrite.rbegin();
   while(iterator != WatchLastWrite.rend()) {
      const std::filesystem::path& directory = iterator->first;

      // ====== Check directory =============================================
      if(iterator->second < threshold) {
         // ====== Update last write time ===================================
         SystemTimePoint lastWriteTimePoint;
         if(getLastWriteTimePoint(directory, lastWriteTimePoint)) {
            if(iterator->second != lastWriteTimePoint) {
               iterator->second = lastWriteTimePoint;
            }
         }
      }
      HPCT_LOG(trace) << "Directory " << relativeTo(directory, ImporterConfig.getImportFilePath())
                      << ": last activity was "
                      << std::chrono::duration_cast<std::chrono::seconds>(now - iterator->second).count() << " s ago";

      // ====== Out of date -> remove directory =============================
      if(iterator->second < threshold) {
         std::error_code ec;
         std::filesystem::remove(directory, ec);
         if(!ec) {
            n++;
            HPCT_LOG(trace) << "Deleted empty directory "
                            << relativeTo(directory, ImporterConfig.getImportFilePath())
                            << ", last activity was "
                            << std::chrono::duration_cast<std::chrono::seconds>(now - iterator->second).count() << " s ago";
            // NOTE: No need to erase the iterator here. It will be removed
            // after the Watch notification of the directory removal!
         }
         else {
            HPCT_LOG(trace) << "Still in-use directory "
                            << relativeTo(directory, ImporterConfig.getImportFilePath());
            // No need to try again too soon!
            iterator->second = now;
         }
      }

      iterator++;
   }

   if(n > 0) {
      HPCT_LOG(trace) << "Cleaned up " << n << " directories";
   }
}


// ###### Show status #######################################################
void UniversalImporter::handleStatusTimer(const boost::system::error_code& errorCode)
{
   if(!errorCode) {
      HPCT_LOG(info) << "Importer status:\n" << *this;
      StatusTimer.expires_at(std::chrono::steady_clock::now() + StatusTimerInterval);
      StatusTimer.async_wait(std::bind(&UniversalImporter::handleStatusTimer, this,
                                       std::placeholders::_1));
   }
}


// ###### Perform gargabe collection ########################################
void UniversalImporter::handleGarbageCollectionTimer(const boost::system::error_code& errorCode)
{
   if(!errorCode) {
      performDirectoryCleanUp();
      GarbageCollectionTimer.expires_at(std::chrono::steady_clock::now() + GarbageCollectionTimerInterval);
      GarbageCollectionTimer.async_wait(std::bind(&UniversalImporter::handleGarbageCollectionTimer, this,
                                        std::placeholders::_1));
   }
}


// ###### << operator #######################################################
std::ostream& operator<<(std::ostream& os, const UniversalImporter& importer)
{
   bool first = true;
   for(ReaderBase* reader : importer.ReaderList) {
      if(first)  {
         first = false;
      }
      else {
         os << "\n";
      }
      os << *reader;
   }
   return os;
}
