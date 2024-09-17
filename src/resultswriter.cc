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
// Copyright (C) 2015-2024 by Thomas Dreibholz
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

#include "resultswriter.h"
#include "logger.h"
#include "tools.h"
#include "resultentry.h"

#include <unistd.h>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/process/environment.hpp>


// ###### Constructor #######################################################
ResultsWriter::ResultsWriter(const std::string&            programID,
                             const unsigned int            measurementID,
                             const std::string&            directory,
                             const std::string&            uniqueID,
                             const std::string&            prefix,
                             const unsigned int            transactionLength,
                             const uid_t                   uid,
                             const gid_t                   gid,
                             const ResultsWriterCompressor compressor)
   : ProgramID(programID),
     MeasurementID(measurementID),
     Directory(directory),
     Prefix(prefix),
     TransactionLength(transactionLength),
     UID(uid),
     GID(gid),
     Compressor(compressor),
     UniqueID(uniqueID)
{
   Inserts   = 0;
   SeqNumber = 0;
}


// ###### Destructor ########################################################
ResultsWriter::~ResultsWriter()
{
   changeFile(false);
}


// ###### Specifc output format #############################################
void ResultsWriter::specifyOutputFormat(const std::string& outputFormatName,
                                        const unsigned int outputFormatVersion)
{
   OutputFormatName    = outputFormatName;
   OutputFormatVersion = outputFormatVersion;
}


// ###### Prepare directories ###############################################
bool ResultsWriter::prepare()
{
   try {
      const std::filesystem::path tempDirectory = Directory / "tmp";
      std::filesystem::create_directory(Directory);
      std::filesystem::create_directory(tempDirectory);
      const int r1 = chown(Directory.string().c_str(), UID, GID);
      const int r2 = chown(tempDirectory.string().c_str(), UID, GID);
      if(r1 || r2) {
         HPCT_LOG(warning) << "Setting ownership of " << Directory << " and " << tempDirectory
                           << " to UID " << UID << ", GID " << GID << " failed: " << strerror(errno);
      }
   }
   catch(std::exception const& e) {
      HPCT_LOG(error) << "Unable to prepare directories - " << e.what();
      return false;
   }
   return(changeFile());
}


// ###### Change output file ################################################
bool ResultsWriter::changeFile(const bool createNewFile)
{
   // ====== Close current file =============================================
   if(OutputFile.is_open()) {
      OutputStream.reset();
      OutputFile.close();
      try {
         if(Inserts == 0) {
            // empty file -> just remove it!
            std::filesystem::remove(TempFileName);
         }
         else {
            // file has contents -> move it!
            std::filesystem::rename(TempFileName, TargetFileName);
         }
      }
      catch(std::exception const& e) {
         HPCT_LOG(warning) << "ResultsWriter::changeFile() - " << e.what();
      }
   }

   // ====== Create new file ================================================
   Inserts = 0;
   SeqNumber++;
   if(createNewFile) {
      try {
         const char* extension = "";
         switch(Compressor) {
            case XZ:
               extension = ".xz";
             break;
            case BZip2:
               extension = ".bz2";
             break;
            case GZip:
               extension = ".gz";
             break;
            default:
             break;
         }
         const std::string name = UniqueID + str(boost::format("-%09d.results%s") % SeqNumber % extension);
         std::filesystem::path targetPath =
            Directory /
               makeDirectoryHierarchy<ResultTimePoint>(std::filesystem::path(),
                                                       name, ResultClock::now(),
                                                       0, 5);
         std::cout << "N1=" <<   targetPath << "\n";
         TargetFileName = targetPath / name;
         std::cout << "N2=" <<   TargetFileName << "\n";
         TempFileName   = TargetFileName;
         TempFileName += ".tmp";
         try {
            std::cout << "CREATE=" <<   targetPath << "\n";
            std::filesystem::create_directories(TargetFileName);
         }
         catch(std::filesystem::filesystem_error& e) {
            HPCT_LOG(warning) << "Creating directory hierarchy for " << TargetFileName << " failed: " << e.what();
            return false;
         }
         OutputFile.open(TempFileName.c_str(), std::ios_base::out | std::ios_base::binary);
         switch(Compressor) {
            case XZ:
               OutputStream.push(boost::iostreams::lzma_compressor());
             break;
            case BZip2:
               OutputStream.push(boost::iostreams::bzip2_compressor());
             break;
            case GZip:
               OutputStream.push(boost::iostreams::gzip_compressor());
             break;
            default:
             break;
         }
         OutputStream.push(OutputFile);
         OutputCreationTime = std::chrono::steady_clock::now();
         if( (OutputStream.good()) && (chown(TempFileName.c_str(), UID, GID) != 0) ) {
            HPCT_LOG(warning) << "Setting ownership of " << TempFileName
                              << " to UID " << UID << ", GID " << GID
                              << " failed: " << strerror(errno);
         }
         return(OutputStream.good());
      }
      catch(std::exception const& e) {
         HPCT_LOG(error) << "ResultsWriter::changeFile() - " << e.what();
         return false;
      }
   }
   return true;
}


// ###### Start new transaction, if transaction length has been reached #####
bool ResultsWriter::mayStartNewTransaction()
{
   const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
   if(std::chrono::duration_cast<std::chrono::seconds>(now - OutputCreationTime).count() > TransactionLength) {
      return(changeFile());
   }
   return true;
}


// ###### Generate INSERT statement #########################################
void ResultsWriter::insert(const std::string& tuple)
{
   if(__builtin_expect(Inserts == 0, 0)) {
      if(!OutputFormatName.empty()) {
         // Write header
         OutputStream << "#? HPCT "
                     << OutputFormatName    << " "
                     << OutputFormatVersion << " "
                     << ProgramID           << "\n";
      }
   }
   OutputStream << tuple << "\n";
   Inserts++;
}


// ###### Prepare results writer ############################################
ResultsWriter* ResultsWriter::makeResultsWriter(std::set<ResultsWriter*>&       resultsWriterSet,
                                                const std::string&              programID,
                                                const unsigned int              measurementID,
                                                const boost::asio::ip::address& sourceAddress,
                                                const std::string&              resultsPrefix,
                                                const std::string&              resultsDirectory,
                                                const unsigned int              resultsTransactionLength,
                                                const uid_t                     uid,
                                                const gid_t                     gid,
                                                const ResultsWriterCompressor   compressor)
{
   if(!resultsDirectory.empty()) {
      std::string uniqueID =
         resultsPrefix + "-" +
         ((measurementID != 0) ?
            "#" + std::to_string(measurementID) :
            "P" + std::to_string(boost::this_process::get_id())) + "-" +
         sourceAddress.to_string() + "-" +
         boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::universal_time());
      replace(uniqueID.begin(), uniqueID.end(), ' ', '-');

      ResultsWriter* resultsWriter =
         new ResultsWriter(programID, measurementID, resultsDirectory, uniqueID,
                           resultsPrefix, resultsTransactionLength,
                           uid, gid, compressor);
      if(resultsWriter->prepare() == true) {
         resultsWriterSet.insert(resultsWriter);
         return(resultsWriter);
      }
      delete resultsWriter;
   }
   return nullptr;
}
