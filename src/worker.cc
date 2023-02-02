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

#include "worker.h"
#include "database-configuration.h"
#include "importer-exception.h"
#include "logger.h"
#include "tools.h"

#include <fstream>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/stream.hpp>


// ###### Constructor #######################################################
Worker::Worker(const unsigned int           workerID,
               ReaderBase&                  reader,
               DatabaseClientBase&          databaseClient,
               const DatabaseConfiguration& databaseConfiguration)

   : WorkerID(workerID),
     Reader(reader),
     DatabaseClient(databaseClient),
     Configuration(databaseConfiguration),
     Identification(Reader.getIdentification() + "/" + std::to_string(WorkerID))
{
   StopRequested.exchange(false);
}


// ###### Destructor ########################################################
Worker::~Worker()
{
   requestStop();
   join();
}


// ###### Start worker ######################################################
void Worker::start(const bool quitWhenIdle)
{
   StopRequested.exchange(false);
   QuitWhenIdle = quitWhenIdle;
   Thread = std::thread(&Worker::run, this);
}


// ###### Wait for finish of worker thread ##################################
void Worker::join()
{
   if(Thread.joinable()) {
      Thread.join();
   }
}


// ###### Stop worker #######################################################
void Worker::requestStop()
{
   StopRequested.exchange(true);
   wakeUp();
}


// ###### Wake up worker loop ###############################################
void Worker::wakeUp()
{
   Notification.notify_one();
}


// ###### Get list of input files ###########################################
void Worker::processFile(DatabaseClientBase&          databaseClient,
                         unsigned long long&          rows,
                         const std::filesystem::path& dataFile)
{
   // ====== Check file size ================================================
   std::error_code ec;
   const std::uintmax_t size = std::filesystem::file_size(dataFile, ec);
   if( (!ec) && (size == 0) ) {
      HPCT_LOG(warning) << getIdentification() << ": Empty input file "
                       << relative_to(dataFile, Configuration.getImportFilePath());
   }

   else {
      // ====== Prepare input stream ========================================
      boost::iostreams::filtering_istream inputStream;

      // ====== Open input file =============================================
#ifdef POSIX_FADV_SEQUENTIAL
      // With fadvise() to optimise caching:
      int handle = open(dataFile.string().c_str(), 0, O_RDONLY);
      if(handle < 0) {
         HPCT_LOG(warning) << getIdentification() << ": Unable to open input file "
                           << relative_to(dataFile, Configuration.getImportFilePath());
         return;
      }
      if(posix_fadvise(handle, 0, 0, POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED|POSIX_FADV_NOREUSE) < 0) {
         perror("posix_fadvise:");
         HPCT_LOG(warning) << "posix_fadvise() failed:" << strerror(errno);
      }

      boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> fstreambuffer(
         handle,
         boost::iostreams::file_descriptor_flags::close_handle);
      std::istream inputFile(&fstreambuffer);
#else
#warning Without fadvise()
      // Without fadvise():
      std::ifstream inputFile;
      inputFile.open(dataFile, std::ios_base::in | std::ios_base::binary);
#endif
      if(inputFile.good()) {
         if(dataFile.extension() == ".xz") {
            inputStream.push(boost::iostreams::lzma_decompressor());
         }
         else if(dataFile.extension() == ".bz2") {
            inputStream.push(boost::iostreams::bzip2_decompressor());
         }
         else if(dataFile.extension() == ".gz") {
            inputStream.push(boost::iostreams::gzip_decompressor());
         }
         inputStream.push(inputFile);

         // ====== Read contents ============================================
         Reader.parseContents(databaseClient, rows, dataFile, inputStream);
      }
      else {
         HPCT_LOG(warning) << getIdentification() << ": Unable to open input file "
                           << relative_to(dataFile, Configuration.getImportFilePath());
      }
   }
}


// ###### Remove empty directories ##########################################
void Worker::deleteEmptyDirectories(std::filesystem::path path)
{
   try {
      path = std::filesystem::canonical(std::filesystem::absolute(path));

      if(is_subdir_of(path, Configuration.getImportFilePath())) {
         if(path != Configuration.getImportFilePath()) {
            while(path.parent_path() != Configuration.getImportFilePath()) {
               std::filesystem::remove(path);
               HPCT_LOG(trace) << getIdentification() << ": Deleted empty directory "
                               << relative_to(path, Configuration.getImportFilePath());
               path = path.parent_path();
            }
         }
      }
   }
   catch(...) { }
}


// ###### Delete successfully imported file #################################
void Worker::deleteImportedFile(const std::filesystem::path& dataFile)
{
   try {
      std::filesystem::remove(dataFile);
      HPCT_LOG(trace) << getIdentification() << ": Deleted imported file "
                      << relative_to(dataFile, Configuration.getImportFilePath());
      deleteEmptyDirectories(dataFile.parent_path());
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Deleting imported file "
                        << relative_to(dataFile, Configuration.getImportFilePath()) << " failed: " << e.what();
   }
}


// ###### Move successfully imported file to good or bad files ##############
void Worker::moveImportedFile(const std::filesystem::path& dataFile,
                              const std::smatch            match,
                              const bool                   isGood)
{
   // ====== Construct destination path =====================================
   assert(is_subdir_of(dataFile, Configuration.getImportFilePath()));
   const std::filesystem::path subdirs =
      std::filesystem::relative(dataFile.parent_path(), Configuration.getImportFilePath()) /
      Reader.getDirectoryHierarchy(dataFile, match, Configuration.getImportMaxDepth() - 1);
   const std::filesystem::path targetPath =
      ((isGood == true) ? Configuration.getGoodFilePath() : Configuration.getBadFilePath()) / subdirs;

   // ====== Create destination directory and move file =====================
   try {
      std::filesystem::create_directories(targetPath);
      std::filesystem::rename(dataFile, targetPath / dataFile.filename());
      HPCT_LOG(debug) << getIdentification() << ": Moved " << ((isGood == true) ? "good" : "bad") <<  " file "
                      << relative_to(dataFile, Configuration.getImportFilePath());
      deleteEmptyDirectories(dataFile.parent_path());
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Moving " << ((isGood == true) ? "good" : "bad") <<  " file "
                        << relative_to(dataFile, Configuration.getImportFilePath())
                        << " to " << targetPath << " failed: " << e.what();
   }
}


// ###### Delete finished input file ########################################
void Worker::finishedFile(const std::filesystem::path& dataFile,
                          const bool                   success)
{
   // Need to extract the file name parts again, in order to find the entry:
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
   const bool         isMatching = std::regex_match(filename, match, Reader.getFileNameRegExp());
   assert(isMatching);

   // ====== File has been imported successfully ===============================
   if(success) {
      // ====== Delete imported file ===========================================
      if(Configuration.getImportMode() == ImportModeType::DeleteImportedFiles) {
         deleteImportedFile(dataFile);
      }
      // ====== Move imported file =============================================
      else if(Configuration.getImportMode() == ImportModeType::MoveImportedFiles) {
         moveImportedFile(dataFile, match, true);
      }
      // ====== Keep imported file where it is =================================
      else  if(Configuration.getImportMode() == ImportModeType::KeepImportedFiles) {
         // Nothing to do here!
      }
   }
   // ====== File is bad ====================================================
   else {
      moveImportedFile(dataFile, match, false);
   }

   // ====== Remove file from the reader ====================================
   const bool fileRemoved = Reader.removeFile(dataFile, match);
   assert(fileRemoved);
}


// ###### Import list of files ##############################################
bool Worker::importFiles(const std::list<std::filesystem::path>& dataFileList)
{
   const bool fastMode = (dataFileList.size() > 1);
   if(fastMode) {
      HPCT_LOG(debug) << getIdentification() << ": Trying to import "
                      << dataFileList.size() << " files in "
                      << ((fastMode == true) ? "fast" : "slow") << " mode ...";
   }
   // for(const std::filesystem::path& d : dataFileList) {
   //    std::cout << "- " << d << "\n";
   // }

   unsigned long long rows = 0;
   std::filesystem::path dataFile;
   try {
      // ====== Import multiple input files in one transaction ========
      DatabaseClient.startTransaction();
      Reader.beginParsing(DatabaseClient, rows);
      for(std::list<std::filesystem::path>::const_iterator iterator = dataFileList.begin();
          iterator != dataFileList.end(); iterator++) {
         dataFile = std::filesystem::path(*iterator);
         if(StopRequested) {
            break;
         }
         HPCT_LOG(trace) << getIdentification() << ": Parsing "
                           <<relative_to(dataFile, Configuration.getImportFilePath()) << " ...";
         processFile(DatabaseClient, rows, dataFile);
      }
      if(Reader.finishParsing(DatabaseClient, rows)) {
         DatabaseClient.commit();
         HPCT_LOG(debug) << getIdentification() << ": Committed " << rows << " rows";
      }
      else {
         DatabaseClient.rollback();
         HPCT_LOG(debug) << getIdentification() << ": Nothing to import!";
      }

      // ====== Delete/move/keep input files ================================
      HPCT_LOG(debug) << getIdentification() << ": Finishing " << dataFileList.size() << " input files ...";
      for(const std::filesystem::path& dataFile : dataFileList) {
         finishedFile(dataFile);
      }
      return true;
   }

   //  ====== Error in input data ===========================================
   catch(ImporterReaderDataErrorException& exception) {
      HPCT_LOG(warning) << getIdentification() << ": Import in "
                        << ((fastMode == true) ? "fast" : "slow") << " mode failed with reader data error: "
                        << exception.what();
      DatabaseClient.rollback();
      if( (!fastMode) && (dataFile != std::filesystem::path()) ) {
         finishedFile(dataFile, false);
      }
   }

   //  ====== Error in database data ========================================
   // NOTE: The database connection is still okay!
   catch(ImporterDatabaseDataErrorException& exception) {
      HPCT_LOG(warning) << getIdentification() << ": Import in "
                        << ((fastMode == true) ? "fast" : "slow") << " mode failed with database data error: "
                        << exception.what();
      DatabaseClient.rollback();
      if(!fastMode) {
         finishedFile(dataFile, false);
      }
   }

   //  ====== Error in database handling ====================================
   // NOTE: Requires reconnect to database!
   catch(ImporterDatabaseException& exception) {
      HPCT_LOG(warning) << getIdentification() << ": Import in "
                        << ((fastMode == true) ? "fast" : "slow") << " mode failed with database exception: "
                        << exception.what();
      DatabaseClient.rollback();
      HPCT_LOG(warning) << getIdentification() << ": Waiting " << Configuration.getReconnectDelay() << " ...";
      std::this_thread::sleep_for(std::chrono::seconds(Configuration.getReconnectDelay()));
      HPCT_LOG(warning) << getIdentification() << ": Trying reconnect ...";
      DatabaseClient.reconnect();
   }

   return false;
}


// ###### Worker loop #######################################################
void Worker::run()
{
   std::unique_lock lock(Mutex);

   while(!StopRequested) {
      // ====== Look for new input files ====================================
      HPCT_LOG(trace) << getIdentification() << ": Looking for new input files ...";

      // ====== Fast import: try to combine files ===========================
      std::list<std::filesystem::path> dataFileList;
      unsigned int files = Reader.fetchFiles(dataFileList, WorkerID, Reader.getMaxTransactionSize());
      while( (files > 0) && (!StopRequested) ) {
         // ====== Fast Mode ================================================
         if(!importFiles(dataFileList)) {

            // ====== Slow Mode =============================================
            if(files > 1) {
               HPCT_LOG(debug) << getIdentification() << ": Trying to import "
                               << dataFileList.size() << " files in slow mode ...";
               for(const std::filesystem::path& dataFile : dataFileList) {
                  std::list<std::filesystem::path> singleFileList;
                  if(StopRequested) {
                     break;
                  }
                  singleFileList.push_back(dataFile);
                  assert(singleFileList.size() == 1);
                  importFiles(singleFileList);
               }
            }
         }
         files = Reader.fetchFiles(dataFileList, WorkerID, Reader.getMaxTransactionSize());
      }

      // ====== Quit when idle? =============================================
      if( (files == 0) && (QuitWhenIdle) ) {
         HPCT_LOG(trace) << getIdentification() << "Idle -> done!";
         break;
      }

      // ====== Wait for new data ===========================================
      if(!StopRequested) {
         HPCT_LOG(trace) << getIdentification() << ": Sleeping ...";
         Notification.wait(lock);
         HPCT_LOG(trace) << getIdentification() << ": Wakeup!";
      }
   }
   HPCT_LOG(trace) << getIdentification() << ": Finished";
}
