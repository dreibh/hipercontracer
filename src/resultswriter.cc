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

#include "resultswriter.h"
#include "assure.h"
#include "compressortype.h"
#include "logger.h"
#include "tools.h"

#include <unistd.h>

#include <iostream>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>


// ###### Constructor #######################################################
ResultsWriter::ResultsWriter(const std::string&        programID,
                             const unsigned int        measurementID,
                             const std::string&        directory,
                             const std::string&        uniqueID,
                             const std::string&        prefix,
                             const unsigned int        transactionLength,
                             const unsigned int        timestampDepth,
                             const uid_t               uid,
                             const gid_t               gid,
                             const CompressorType      compressor,
                             const ResultsEncodingType encoding,
                             const bool                console)
   : ProgramID(programID),
     MeasurementID(measurementID),
     Directory(directory),
     Prefix(prefix),
     TransactionLength(transactionLength),
     TimestampDepth(timestampDepth),
     UID(uid),
     GID(gid),
     Compressor(compressor),
     Encoding(encoding),
     Console(console),
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
   if(Console) {
      // The console writer is shared by all services: open stdout only once!
      std::lock_guard<std::mutex> lock(Mutex);
      return (Output.is_complete()) ? true : Output.openStream(std::cout);
   }
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
      if( (Encoding == RET_JSON) && (Output.is_complete()) ) {
         // Terminate the JSON array. The console always gets a valid array.
         if(Inserts > 0) {
            Output << "\n]\n";
         }
         else if(Console) {
            Output << "[]\n";
         }
      }
      Output.closeStream( (Inserts > 0) || ((Console) && (Output.is_complete())) );
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
            str(boost::format("-%09d%s%s")
                   % SeqNumber
                   % ((Encoding == RET_JSON) ? ".json" : ".hpct")
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
   if(Console) {
      return true;   // The console output is never rotated.
   }
   const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
   if(std::chrono::duration_cast<std::chrono::seconds>(now - OutputCreationTime).count() > TransactionLength) {
      return changeFile();
   }
   return true;
}


// ###### Generate INSERT statement #########################################
void ResultsWriter::insert(const std::string& tuple)
{
   std::lock_guard<std::mutex> lock(Mutex);
   if(Encoding == RET_JSON) {
      // Each tuple is one JSON object, as element of a JSON array
      Output << ((Inserts == 0) ? "[\n" : ",\n") << tuple;
   }
   else {
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
   }
   if(Console) {
      Output.flush();
   }
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
   const CompressorType            compressor,
   const ResultsEncodingType       encoding,
   const bool                      console)
{
   if(console) {
      // All services share one console writer, so that stdout gets
      // one JSON array (and no interleaved lines)
      for(ResultsWriter* resultsWriter : resultsWriterSet) {
         if(resultsWriter->Console) {
            return resultsWriter;
         }
      }
      ResultsWriter* resultsWriter =
         new ResultsWriter(programID, measurementID, std::string(), std::string(),
                           resultsPrefix, 0, 0, uid, gid, compressor, encoding, true);
      assure(resultsWriter != nullptr);
      resultsWriterSet.insert(resultsWriter);
      return resultsWriter;
   }
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
                           uid, gid, compressor, encoding);
      assure(resultsWriter != nullptr);
      resultsWriterSet.insert(resultsWriter);
      return resultsWriter;
   }
   return nullptr;
}
