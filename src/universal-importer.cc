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
UniversalImporter::UniversalImporter(boost::asio::io_service&     ioService,
                                     const ImporterConfiguration& importerConfiguration,
                                     const DatabaseConfiguration& databaseConfiguration)
 : IOService(ioService),
   ImporterConfig(importerConfiguration),
   DatabaseConfig(databaseConfiguration),
   HasImportPathFilter(ImporterConfig.getImportPathFilter().size() > 0),
   ImportPathFilter("^(" + (ImporterConfig.getImportFilePath() / ")(").string() + ImporterConfig.getImportPathFilter() + ")(.*)$"),
   ImportPathFilterRegEx(ImportPathFilter),
   Signals(IOService, SIGINT, SIGTERM),
   StatusTimer(IOService),
   StatusTimerInterval(boost::posix_time::seconds(importerConfiguration.getStatusInterval())),
   GarbageCollectionTimer(IOService),
   GarbageCollectionTimerInterval(0, 0, importerConfiguration.getGarbageCollectionInterval(), 0),
   GarbageCollectionMaxAge(std::chrono::seconds(importerConfiguration.getGarbageCollectionMaxAge())),
   INotifyStream(IOService)
{
   INotifyFD = -1;
   StatusTimer.expires_from_now(StatusTimerInterval);
   StatusTimer.async_wait(std::bind(&UniversalImporter::handleStatusTimer, this,
                                    std::placeholders::_1));
   GarbageCollectionTimer.expires_from_now(GarbageCollectionTimerInterval);
   GarbageCollectionTimer.async_wait(std::bind(&UniversalImporter::handleGarbageCollectionTimer, this,
                                    std::placeholders::_1));
}


// ###### Destructor ########################################################
UniversalImporter::~UniversalImporter()
{
   stop();
}


// ###### Start importer ####################################################
bool UniversalImporter::start(const bool quitWhenIdle)
{
   // ====== Intercept signals ==============================================
   Signals.async_wait(std::bind(&UniversalImporter::handleSignalEvent, this,
                                std::placeholders::_1,
                                std::placeholders::_2));

   // ====== Set up INotify =================================================
   INotifyFD = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
   assert(INotifyFD > 0);
   INotifyStream.assign(INotifyFD);
   int wd = inotify_add_watch(INotifyFD, ImporterConfig.getImportFilePath().c_str(),
                              IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
   if(wd < 0) {
      HPCT_LOG(error) << "Adding INotify watch for " << ImporterConfig.getImportFilePath()
                      << " failed: " << strerror(errno);
      return false;
   }
   INotifyWatchDescriptors.insert(boost::bimap<int, std::filesystem::path>::value_type(
                                  wd, ImporterConfig.getImportFilePath()));

   INotifyStream.async_read_some(boost::asio::buffer(&INotifyEventBuffer, sizeof(INotifyEventBuffer)),
                                 std::bind(&UniversalImporter::handleINotifyEvent, this,
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
      INotifyStream.cancel();
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

   // ====== Remove INotify =================================================
   if(INotifyFD >= 0) {
      boost::bimap<int, std::filesystem::path>::iterator iterator = INotifyWatchDescriptors.begin();
      while(iterator != INotifyWatchDescriptors.end()) {
         inotify_rm_watch(INotifyFD, iterator->left);
         removeLastWriteTimePoint(iterator->right);
         INotifyWatchDescriptors.erase(iterator);
         iterator = INotifyWatchDescriptors.begin();
      }
      close(INotifyFD);
      INotifyFD = -1;
   }
   assert(INotifyWatchDescriptors.size() == INotifyWatchLastWrite.size());   // == 0!

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
      IOService.stop();
   }
}


// ###### Handle signal #####################################################
void UniversalImporter::handleINotifyEvent(const boost::system::error_code& errorCode,
                                           const std::size_t                length)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      // ====== Handle events ===============================================
      unsigned long p = 0;
      while(p < length) {
         const inotify_event* event = (const inotify_event*)&INotifyEventBuffer[p];
         boost::bimap<int, std::filesystem::path>::left_map::const_iterator found = INotifyWatchDescriptors.left.find(event->wd);
         if(found != INotifyWatchDescriptors.left.end()) {
            if(event->name[0] != '.') {   // Ignore hidden file or directory (starting with '.').
               const std::filesystem::path& directory = found->second;

               // ====== Event for directory ================================
               if(event->mask & IN_ISDIR) {
                  const std::filesystem::path dataDirectory = directory / std::string(event->name);
                  if(event->mask & IN_CREATE) {
                     HPCT_LOG(trace) << "INotify event for new directory: " << dataDirectory;
                     const int wd = inotify_add_watch(INotifyFD, dataDirectory.c_str(),
                                                      IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
                     if(wd >= 0) {
                        INotifyWatchDescriptors.insert(boost::bimap<int, std::filesystem::path>::value_type(wd, dataDirectory));
                        addOrUpdateLastWriteTimePoint(dataDirectory);

                        // A directory traversal is necessary in this new
                        // directory, since files/directories may have been
                        // created before adding the INotify watch!
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
                        HPCT_LOG(error) << "Adding INotify watch for " << dataDirectory
                                       << " failed: " << strerror(errno);
                     }
                  }
                  else if(event->mask & IN_DELETE) {
                     HPCT_LOG(trace) << "INotify event for deleted directory: " << dataDirectory;
                     boost::bimap<int, std::filesystem::path>::right_map::const_iterator wdToDelete = INotifyWatchDescriptors.right.find(dataDirectory);
                     if(wdToDelete != INotifyWatchDescriptors.right.end()) {
                        removeLastWriteTimePoint(dataDirectory);
                        INotifyWatchDescriptors.left.erase(wdToDelete->second);
                     }
                  }
               }

               // ====== Event for file =====================================
               else {
                  const std::filesystem::path dataFile = directory / std::string(event->name);
                  if(event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                     HPCT_LOG(trace) << "INotify event for new file " << dataFile;
                     addFile(dataFile);
                  }
                  else if(event->mask & IN_DELETE) {
                     HPCT_LOG(trace) << "INotify event for deleted file " << dataFile;
                     removeFile(dataFile);
                  }
               }
            }
         }
         p += sizeof(inotify_event) + event->len;
      }

      // ====== Wait for more events ========================================
      INotifyStream.async_read_some(boost::asio::buffer(&INotifyEventBuffer, sizeof(INotifyEventBuffer)),
                                    std::bind(&UniversalImporter::handleINotifyEvent, this,
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
         // ------ Create INotify watch -------------------------------------
         const int wd = inotify_add_watch(INotifyFD, dirEntry.path().c_str(),
                                          IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
         if(wd >= 0) {
            INotifyWatchDescriptors.insert(boost::bimap<int, std::filesystem::path>::value_type(wd, dirEntry.path()));
            addOrUpdateLastWriteTimePoint(dirEntry.path());
         }
         else {
            HPCT_LOG(error) << "Adding INotify watch for " << dirEntry.path()
                            << " failed: " << strerror(errno);
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
         INotifyWatchLastWrite.find(directory);
      if(found != INotifyWatchLastWrite.end()) {
         found->second = lastWriteTimePoint;
      }
      else {
         INotifyWatchLastWrite.insert(std::pair<const std::filesystem::path, SystemTimePoint>(
                              directory, lastWriteTimePoint));
      }
   }
}


// ###### Remove directory from garbage collector ###########################
void UniversalImporter::removeLastWriteTimePoint(const std::filesystem::path directory)
{
   std::map<const std::filesystem::path, SystemTimePoint>::iterator found =
      INotifyWatchLastWrite.find(directory);
   if(found != INotifyWatchLastWrite.end()) {
      INotifyWatchLastWrite.erase(found);
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
      INotifyWatchLastWrite.rbegin();
   while(iterator != INotifyWatchLastWrite.rend()) {
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
            // after the INotify notification of the directory removal!
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
      StatusTimer.expires_from_now(StatusTimerInterval);
      StatusTimer.async_wait(std::bind(&UniversalImporter::handleStatusTimer, this,
                                       std::placeholders::_1));
   }
}


// ###### Perform gargabe collection ########################################
void UniversalImporter::handleGarbageCollectionTimer(const boost::system::error_code& errorCode)
{
   if(!errorCode) {
      performDirectoryCleanUp();
      GarbageCollectionTimer.expires_from_now(GarbageCollectionTimerInterval);
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
