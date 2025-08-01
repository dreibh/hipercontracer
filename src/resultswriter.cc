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

#include "resultswriter.h"
#include "assure.h"
#include "compressortype.h"
#include "logger.h"
#include "tools.h"

#include <unistd.h>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>


// ###### Constructor #######################################################
ResultsWriter::ResultsWriter(const std::string&   programID,
                             const unsigned int   measurementID,
                             const std::string&   directory,
                             const std::string&   uniqueID,
                             const std::string&   prefix,
                             const unsigned int   transactionLength,
                             const unsigned int   timestampDepth,
                             const uid_t          uid,
                             const gid_t          gid,
                             const CompressorType compressor)
   : ProgramID(programID),
     MeasurementID(measurementID),
     Directory(directory),
     Prefix(prefix),
     TransactionLength(transactionLength),
     TimestampDepth(timestampDepth),
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
      std::filesystem::create_directory(Directory);
   }
   catch(std::exception const& e) {
      HPCT_LOG(error) << "Unable to prepare directories: " << e.what();
      return false;
   }
   return changeFile();
}


// ###### Change output file ################################################
bool ResultsWriter::changeFile(const bool createNewFile)
{
   // ====== Close current file =============================================
   try {
      Output.closeStream( (Inserts > 0) );
   }
   catch(std::exception const& e) {
      HPCT_LOG(error) << "Failed to close output file "
                      << TargetFileName << ": " << e.what();
   }

   // ====== Create new file ================================================
   Inserts = 0;
   SeqNumber++;
   if(createNewFile) {
      try {
         // ------ Prepare directory hierachy -------------------------------
         const std::string name = UniqueID +
            str(boost::format("-%09d.hpct%s")
                   % SeqNumber
                   % getExtensionForCompressor(Compressor));
         std::filesystem::path targetPath = Directory /
            makeDirectoryHierarchy<std::chrono::system_clock::time_point>(
               std::filesystem::path(), name, std::chrono::system_clock::now(),
               0, TimestampDepth);
         try {
            std::filesystem::create_directories(targetPath);
         }
         catch(std::filesystem::filesystem_error& e) {
            HPCT_LOG(warning) << "Creating directory hierarchy " << targetPath
                              << " failed: " << e.what();
            return false;
         }

         // ------ Open new output file -------------------------------------
         TargetFileName = targetPath / name;
         Output.openStream(TargetFileName);
         OutputCreationTime = std::chrono::steady_clock::now();
         return Output.good();
      }
      catch(std::exception const& e) {
         HPCT_LOG(error) << "Failed to create output file "
                         << TargetFileName << ": " << e.what();
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
      return changeFile();
   }
   return true;
}


// ###### Generate INSERT statement #########################################
void ResultsWriter::insert(const std::string& tuple)
{
   if(__builtin_expect(Inserts == 0, 0)) {
      if(!OutputFormatName.empty()) {
         // Write header
         Output << "#? HPCT "
                       << OutputFormatName    << " "
                       << OutputFormatVersion << " "
                       << ProgramID           << "\n";
      }
   }
   Output << tuple << "\n";
   Inserts++;
}


// ###### Prepare results writer ############################################
ResultsWriter* ResultsWriter::makeResultsWriter(
   std::set<ResultsWriter*>&       resultsWriterSet,
   const std::string&              programID,
   const unsigned int              measurementID,
   const boost::asio::ip::address& sourceAddress,
   const std::string&              resultsPrefix,
   const std::string&              resultsDirectory,
   const unsigned int              resultsTransactionLength,
   const unsigned int              resultsTimestampDepth,
   const uid_t                     uid,
   const gid_t                     gid,
   const CompressorType            compressor)
{
   if(!resultsDirectory.empty()) {
      std::string uniqueID =
         resultsPrefix + "-" +
         ((measurementID != 0) ?
            "#" + std::to_string(measurementID) :
            "P" + std::to_string(getpid())) + "-" +
         sourceAddress.to_string() + "-" +
         boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::universal_time());
      replace(uniqueID.begin(), uniqueID.end(), ' ', '-');

      ResultsWriter* resultsWriter =
         new ResultsWriter(programID, measurementID, resultsDirectory, uniqueID,
                           resultsPrefix, resultsTransactionLength, resultsTimestampDepth,
                           uid, gid, compressor);
      assure(resultsWriter != nullptr);
      resultsWriterSet.insert(resultsWriter);
      return resultsWriter;
   }
   return nullptr;
}
