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

#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>


// ###### Constructor #######################################################
Worker::Worker(const unsigned int           workerID,
               ReaderBase&                  reader,
               DatabaseClientBase&          databaseClient,
               const std::filesystem::path& importFilePath,
               const std::filesystem::path& goodFilePath,
               const std::filesystem::path& badFilePath,
               const ImportModeType         importMode)
   : WorkerID(workerID),
     Reader(reader),
     DatabaseClient(databaseClient),
     ImportFilePath(importFilePath),
     GoodFilePath(goodFilePath),
     BadFilePath(badFilePath),
     ImportMode(importMode),
     Identification(Reader.getIdentification() + "/" + std::to_string(WorkerID))
{
   StopRequested.exchange(false);
}


// ###### Destructor ########################################################
Worker::~Worker()
{
   requestStop();
   if(Thread.joinable()) {
      Thread.join();
   }
}


// ###### Start worker ######################################################
void Worker::start()
{
   StopRequested.exchange(false);
   Thread = std::thread(&Worker::run, this);
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
                       << relative_to(dataFile, ImportFilePath);
   }

   else {
      // ====== Prepare input stream ========================================
      std::ifstream                       inputFile;
      boost::iostreams::filtering_istream inputStream;
      inputFile.open(dataFile, std::ios_base::in | std::ios_base::binary);
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
                           << relative_to(dataFile, ImportFilePath);
      }
   }
}


// ###### Remove empty directories ##########################################
void Worker::deleteEmptyDirectories(std::filesystem::path path)
{
   assert(is_subdir_of(path, ImportFilePath));

   std::error_code ec;
   while(path.parent_path() != ImportFilePath) {
      std::filesystem::remove(path, ec);
      if(ec) {
         break;
      }
      HPCT_LOG(trace) << getIdentification() << ": Deleted empty directory "
                      << relative_to(path, ImportFilePath);
      path = path.parent_path();
   }
}


// ###### Delete successfully imported file #################################
void Worker::deleteImportedFile(const std::filesystem::path& dataFile)
{
   try {
      std::filesystem::remove(dataFile);
      HPCT_LOG(trace) << getIdentification() << ": Deleted imported file "
                      << relative_to(dataFile, ImportFilePath);
      deleteEmptyDirectories(dataFile.parent_path());
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Deleting imported file "
                        << relative_to(dataFile, ImportFilePath) << " failed: " << e.what();
   }
}


// ###### Move successfully imported file to good files #####################
void Worker::moveImportedFile(const std::filesystem::path& dataFile)
{
   assert(is_subdir_of(dataFile, ImportFilePath));
   const std::filesystem::path subdirs    = std::filesystem::relative(dataFile.parent_path(), ImportFilePath);
   const std::filesystem::path targetPath = GoodFilePath / subdirs;
   try {
      std::filesystem::create_directories(targetPath);
      std::filesystem::rename(dataFile, targetPath / dataFile.filename());
      HPCT_LOG(trace) << getIdentification() << ": Moved imported file "
                      << relative_to(dataFile, ImportFilePath);
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Moving imported file "
                        << relative_to(dataFile, ImportFilePath)
                        << " to " << targetPath << " failed: " << e.what();
   }
}


// ###### Move bad file to bad files ########################################
void Worker::moveBadFile(const std::filesystem::path& dataFile)
{
   assert(is_subdir_of(dataFile, ImportFilePath));
   const std::filesystem::path subdirs    = std::filesystem::relative(dataFile.parent_path(), ImportFilePath);
   const std::filesystem::path targetPath = BadFilePath / subdirs;
   try {
      std::filesystem::create_directories(targetPath);
      std::filesystem::rename(dataFile, targetPath / dataFile.filename());
      HPCT_LOG(trace) << getIdentification() << ": Moved bad file "
                      << relative_to(dataFile, ImportFilePath);
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Moving bad file "
                        << relative_to(dataFile, ImportFilePath)
                        << " to " << targetPath << " failed: " << e.what();
   }
}


// ###### Delete finished input file ########################################
void Worker::finishedFile(const std::filesystem::path& dataFile,
                          const bool                   success)
{
   // ====== File has been imported successfully ===============================
   if(success) {
      // ====== Delete imported file ===========================================
      if(ImportMode == ImportModeType::DeleteImportedFiles) {
         deleteImportedFile(dataFile);
      }
      // ====== Move imported file =============================================
      else if(ImportMode == ImportModeType::MoveImportedFiles) {
         moveImportedFile(dataFile);
      }
      // ====== Keep imported file where it is =================================
      else  if(ImportMode == ImportModeType::KeepImportedFiles) {
         // Nothing to do here!
      }
   }
   // ====== File is bad ====================================================
   else {
      moveBadFile(dataFile);
   }

   // ====== Remove file from the reader ====================================
   // Need to extract the file name parts again, in order to find the entry:
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
   assert(std::regex_match(filename, match, Reader.getFileNameRegExp()));
   assert(Reader.removeFile(dataFile, match) == 1);
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
   try {
      // ====== Import multiple input files in one transaction ========
      DatabaseClient.startTransaction();
      Reader.beginParsing(DatabaseClient, rows);
      for(const std::filesystem::path& dataFile : dataFileList) {
         if(StopRequested) {
            break;
         }
         try {
            HPCT_LOG(trace) << getIdentification() << ": Parsing "
                            <<relative_to(dataFile, ImportFilePath) << " ...";
            processFile(DatabaseClient, rows, dataFile);
         }
         catch(ImporterReaderDataErrorException& exception) {
            HPCT_LOG(warning) << getIdentification() << ": Import in "
                              << ((fastMode == true) ? "fast" : "slow") << " mode failed with reader data error: "
                              << exception.what();
            DatabaseClient.rollback();
            if(!fastMode) {
               finishedFile(dataFile, false);
            }
            return false;
         }
         catch(ImporterDatabaseDataErrorException& exception) {
            HPCT_LOG(warning) << getIdentification() << ": Import in "
                              << ((fastMode == true) ? "fast" : "slow") << " mode failed with database data error: "
                              << exception.what();
            DatabaseClient.rollback();
            if(!fastMode) {
               finishedFile(dataFile, false);
            }
            return false;
         }
      }
      if(Reader.finishParsing(DatabaseClient, rows)) {
         DatabaseClient.commit();
         HPCT_LOG(debug) << getIdentification() << ": Committed " << rows << " rows";
      }
      else {
         HPCT_LOG(debug) << getIdentification() << ": Nothing to import!";
      }

      // ====== Delete/move/keep input files ================================
      HPCT_LOG(debug) << getIdentification() << ": Finishing " << dataFileList.size() << " input files ...";
      for(const std::filesystem::path& dataFile : dataFileList) {
         finishedFile(dataFile);
      }
      return true;
   }
   catch(ImporterDatabaseException& exception) {
      HPCT_LOG(warning) << getIdentification() << ": Import in "
                        << ((fastMode == true) ? "fast" : "slow") << " mode failed: "
                        << exception.what();
      DatabaseClient.rollback();
      usleep(5000000);
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

      // ====== Wait for new data ===========================================
      if(!StopRequested) {
         HPCT_LOG(trace) << getIdentification() << ": Sleeping ...";
         Notification.wait(lock);
         HPCT_LOG(trace) << getIdentification() << ": Wakeup!";
      }
   }
}
