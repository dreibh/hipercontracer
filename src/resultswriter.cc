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
// Copyright (C) 2015-2019 by Thomas Dreibholz
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

#include <unistd.h>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
// #include <boost/process/environment.hpp>


// ###### Constructor #######################################################
ResultsWriter::ResultsWriter(const std::string& directory,
                             const std::string& uniqueID,
                             const std::string& formatName,
                             const unsigned int transactionLength,
                             const uid_t        uid,
                             const gid_t        gid)
   : Directory(directory),
     UniqueID(uniqueID),
     FormatName(formatName),
     TransactionLength(transactionLength),
     UID(uid),
     GID(gid)
{
   Inserts   = 0;
   SeqNumber = 0;
}


// ###### Destructor ########################################################
ResultsWriter::~ResultsWriter()
{
   changeFile(false);
}


// ###### Prepare directories ###############################################
bool ResultsWriter::prepare()
{
   try {
      const boost::filesystem::path tempDirectory = Directory / "tmp";
      boost::filesystem::create_directory(Directory);
      boost::filesystem::create_directory(tempDirectory);
      const int r1 = chown(Directory.string().c_str(), UID, GID);
      const int r2 = chown(tempDirectory.string().c_str(), UID, GID);
      if(r1 || r2) {
         HPCT_LOG(warning) << "Setting ownership of " << Directory << " and " << tempDirectory
                           << " to UID " << UID << ", GID " << GID << " failed: " << strerror(errno);
      }
   }
   catch(std::exception const& e) {
      HPCT_LOG(error) << "Unable to prepare directories - " << e.what();
      return(false);
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
            boost::filesystem::remove(TempFileName);
         }
         else {
            // file has contents -> move it!
            boost::filesystem::rename(TempFileName, TargetFileName);
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
         const std::string name = UniqueID + str(boost::format("-%09d.results.bz2") % SeqNumber);
         TempFileName   = Directory / "tmp" / name;
         TargetFileName = Directory / name;
         OutputFile.open(TempFileName.c_str(), std::ios_base::out | std::ios_base::binary);
         OutputStream.push(boost::iostreams::bzip2_compressor());
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
         return(false);
      }
   }
   return(true);
}


// ###### Start new transaction, if transaction length has been reached #####
bool ResultsWriter::mayStartNewTransaction()
{
   const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
   if(std::chrono::duration_cast<std::chrono::seconds>(now - OutputCreationTime).count() > TransactionLength) {
      return(changeFile());
   }
   return(true);
}


// ###### Generate INSERT statement #########################################
void ResultsWriter::insert(const std::string& tuple)
{
   OutputStream << tuple << std::endl;
   Inserts++;
}


// ###### Prepare results writer ############################################
ResultsWriter* ResultsWriter::makeResultsWriter(std::set<ResultsWriter*>&       resultsWriterSet,
                                                const boost::asio::ip::address& sourceAddress,
                                                const std::string&              resultsFormat,
                                                const std::string&              resultsDirectory,
                                                const unsigned int              resultsTransactionLength,
                                                const uid_t                     uid,
                                                const gid_t                     gid)
{
   ResultsWriter* resultsWriter = nullptr;
   if(!resultsDirectory.empty()) {
      std::string uniqueID =
         resultsFormat + "-" +
         str(boost::format("P%d") % getpid()) + "-" +   /* Better: boost::this_process::get_id() */
         sourceAddress.to_string() + "-" +
         boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::universal_time());
      replace(uniqueID.begin(), uniqueID.end(), ' ', '-');
      resultsWriter = new ResultsWriter(resultsDirectory, uniqueID, resultsFormat, resultsTransactionLength,
                                        uid, gid);
      if(resultsWriter->prepare() == false) {
         ::exit(1);
      }
      resultsWriterSet.insert(resultsWriter);
   }
   return(resultsWriter);
}
