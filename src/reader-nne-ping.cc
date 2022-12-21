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

#include "reader-nne-ping.h"
#include "importer-exception.h"
#include "logger.h"
#include "tools.h"

#include <boost/crc.hpp>


const std::string NorNetEdgePingReader::Identification = "NorNetEdgePing";
const std::regex  NorNetEdgePingReader::FileNameRegExp = std::regex(
   // Format: uping_<MeasurementID>.dat.<YYYY-MM-DD_HH-MM-SS>.xz
   "^uping_([0-9]+)\\.dat\\.([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9][0-9]-[0-9][0-9]-[0-9][0-9])\\.xz$"
);


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const NorNetEdgePingFileEntry& a, const NorNetEdgePingFileEntry& b)
{
   // ====== Level 1: TimeStamp =============================================
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   else if(a.TimeStamp == b.TimeStamp) {
      // ====== Level 2: MeasurementID ======================================
      if(a.MeasurementID < b.MeasurementID) {
         return true;
      }
      else if(a.MeasurementID == b.MeasurementID) {
         // ====== Level 3: DataFile ========================================
         if(a.DataFile < b.DataFile) {
            return true;
         }
      }
   }
   return false;
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const NorNetEdgePingFileEntry& entry)
{
   os << "("
      << timePointToString<ReaderTimePoint>(entry.TimeStamp) << ", "
      << entry.MeasurementID << ", "
      << entry.DataFile
      << ")";
   return os;
}


// ###### Make NorNetEdgePingFileEntry from file name  ######################
int makeInputFileEntry(const std::filesystem::path& dataFile,
                       const std::smatch            match,
                       NorNetEdgePingFileEntry&     inputFileEntry,
                       const unsigned int           workers)
{
   if(match.size() == 3) {
      ReaderTimePoint timeStamp;
      if(stringToTimePoint<ReaderTimePoint>(match[2].str(), timeStamp, "%Y-%m-%d_%H-%M-%S")) {
         inputFileEntry.TimeStamp     = timeStamp;
         inputFileEntry.MeasurementID = atol(match[1].str().c_str());
         inputFileEntry.DataFile      = dataFile;
         const int workerID = inputFileEntry.MeasurementID % workers;
         return workerID;
      }
   }
   return -1;
}


// ###### Get priority of NorNetEdgePingFileEntry ###########################
ReaderPriority getPriorityOfFileEntry(const NorNetEdgePingFileEntry& inputFileEntry)
{
   const ReaderTimePoint    now = nowInUTC<ReaderTimePoint>();
   const ReaderTimeDuration age = now - inputFileEntry.TimeStamp;
   const long long seconds = std::chrono::duration_cast<std::chrono::seconds>(age).count();
   if(seconds < 6 * 3600) {
      return ReaderPriority::High;
   }
   return ReaderPriority::Low;
}


// ###### Constructor #######################################################
NorNetEdgePingReader::NorNetEdgePingReader(const DatabaseConfiguration& databaseConfiguration,
                                           const unsigned int           workers,
                                           const unsigned int           maxTransactionSize,
                                           const std::string&           table_measurement_generic_data)
   : ReaderImplementation<NorNetEdgePingFileEntry>(databaseConfiguration, workers, maxTransactionSize),
     Table_measurement_generic_data(table_measurement_generic_data)
{ }


// ###### Destructor ########################################################
NorNetEdgePingReader::~NorNetEdgePingReader()
{ }


// ###### Begin parsing #####################################################
void NorNetEdgePingReader::beginParsing(DatabaseClientBase& databaseClient,
                                        unsigned long long& rows)
{
   rows = 0;

   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement = databaseClient.getStatement("measurement_generic_data", false, true);

   // ====== Generate import statement ======================================
   if(backend & DatabaseBackendType::SQL_Generic) {
      statement
         << "INSERT INTO " << Table_measurement_generic_data
         << "(ts, mi_id, seq, xml_data, crc, stats) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      statement << "{ \"" << Table_measurement_generic_data <<  "\": [";
   }
   else {
      throw ImporterLogicException("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool NorNetEdgePingReader::finishParsing(DatabaseClientBase& databaseClient,
                                         unsigned long long& rows)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement = databaseClient.getStatement("measurement_generic_data");
   assert(statement.getRows() == rows);

   if(rows > 0) {
      // ====== Generate import statement ===================================
      if(backend & DatabaseBackendType::SQL_Generic) {
         if(rows > 0) {
            statement << "\nON DUPLICATE KEY UPDATE stats=stats";
            databaseClient.executeUpdate(statement);
         }
         else {
            statement.clear();
         }
      }
      else if(backend & DatabaseBackendType::NoSQL_Generic) {
         if(rows > 0) {
            statement << " \n] }";
            databaseClient.executeUpdate(statement);
         }
         else {
            statement.clear();
         }
      }
      else {
         throw ImporterLogicException("Unknown output format");
      }
      return true;
   }
   return false;
}


// ###### Parse input file ##################################################
void NorNetEdgePingReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        const std::filesystem::path&         dataFile,
        boost::iostreams::filtering_istream& dataStream)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement              = databaseClient.getStatement("measurement_generic_data");
   static const unsigned int NorNetEdgePingColumns   = 4;
   static const char         NorNetEdgePingDelimiter = '\t';

   std::string inputLine;
   std::string tuple[NorNetEdgePingColumns];
   while(std::getline(dataStream, inputLine)) {
      // ====== Parse line ==================================================
      size_t columns = 0;
      size_t start;
      size_t end = 0;
      while((start = inputLine.find_first_not_of(NorNetEdgePingDelimiter, end)) != std::string::npos) {
         end = inputLine.find(NorNetEdgePingDelimiter, start);

         if(columns == NorNetEdgePingColumns) {
            throw ImporterReaderDataErrorException("Too many columns in input file " +
                                                   relative_to(dataFile, Configuration.getImportFilePath()).string());
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns != NorNetEdgePingColumns) {
         throw ImporterReaderDataErrorException("Too few columns in input file " +
                                                relative_to(dataFile, Configuration.getImportFilePath()).string());
      }

      // ====== Generate import statement ===================================
      if(backend & DatabaseBackendType::SQL_Generic) {
         statement.beginRow();
         statement
            << statement.quote(tuple[0]) << statement.sep()
            << std::stoul(tuple[1])      << statement.sep()
            << std::stoul(tuple[2])      << statement.sep()
            << statement.quote(tuple[3]) << statement.sep()
            << "CRC32(xml_data)"         << statement.sep()
            << "10 + mi_id MOD 10";
         statement.endRow();
         rows++;
      }
      else if(backend & DatabaseBackendType::NoSQL_Generic) {
         const unsigned int mi_id = std::stoul(tuple[1]);

         boost::crc_32_type crc32;
         crc32.process_bytes(tuple[3].data(), tuple[3].length());
         const uint32_t crc32sum = crc32.checksum();

         statement.beginRow();
         statement
            << "\"ts\": "       << statement.quote(tuple[0]) << statement.sep()
            << "\"mi_id\": "    <<  mi_id     << statement.sep()
            << "\"seq\": "      << std::stoul(tuple[2])      << statement.sep()
            << "\"xml_data\": " << statement.quote(tuple[3]) << statement.sep()
            << "\"crc\": "      << crc32sum                  << statement.sep()
            << "\"stats\": "    << 10 + (mi_id % 10);
         statement.endRow();
         rows++;
      }
      else {
         throw ImporterLogicException("Unknown output format");
      }
   }
}