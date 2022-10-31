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


const std::string NorNetEdgePingReader::Identification = "UDPPing";
const std::regex  NorNetEdgePingReader::FileNameRegExp = std::regex(
   // Format: uping_<MeasurementID>.dat.<YYYY-MM-DD_HH-MM-SS>.xz
   "^uping_([0-9]+)\\.dat\\.([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9][0-9]-[0-9][0-9]-[0-9][0-9])\\.xz$"
);


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const NorNetEdgePingReader::InputFileEntry& a,
               const NorNetEdgePingReader::InputFileEntry& b)
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
std::ostream& operator<<(std::ostream& os, const NorNetEdgePingReader::InputFileEntry& entry)
{
   os << "("
      << timePointToString<NorNetEdgePingReader::FileEntryTimePoint>(entry.TimeStamp) << ", "
      << entry.MeasurementID << ", "
      << entry.DataFile
      << ")";
   return os;
}


// ###### Constructor #######################################################
NorNetEdgePingReader::NorNetEdgePingReader(const unsigned int workers,
                                           const unsigned int maxTransactionSize,
                                           const std::string& table_measurement_generic_data)
   : ReaderBase(workers, maxTransactionSize),
     Table_measurement_generic_data(table_measurement_generic_data)
{
   DataFileSet = new std::set<InputFileEntry>[Workers];
   assert(DataFileSet != nullptr);
}


// ###### Destructor ########################################################
NorNetEdgePingReader::~NorNetEdgePingReader()
{
   delete [] DataFileSet;
   DataFileSet = nullptr;
}


// ###### Get identification of reader ######################################
const std::string& NorNetEdgePingReader::getIdentification() const
{
   return Identification;
}


// ###### Get input file name regular expression ############################
const std::regex& NorNetEdgePingReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


// ###### Add input file to reader ##########################################
int NorNetEdgePingReader::addFile(const std::filesystem::path& dataFile,
                                  const std::smatch            match)
{
//  static int n=0;n++;
//  if(n>20) {
//   puts("??????");
//   return -1;  // ??????
//  }

   if(match.size() == 3) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[2].str(), timeStamp, "%Y-%m-%d_%H-%M-%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.TimeStamp     = timeStamp;
         inputFileEntry.MeasurementID = atol(match[1].str().c_str());
         inputFileEntry.DataFile      = dataFile;
         const int workerID = inputFileEntry.MeasurementID % Workers;

         std::unique_lock lock(Mutex);
         if(DataFileSet[workerID].insert(inputFileEntry).second) {
            HPCT_LOG(trace) << Identification << ": Added data file " << dataFile << " to reader";
            TotalFiles++;
            return workerID;
         }
      }
      else {
         HPCT_LOG(warning) << Identification << ": Bad time stamp " << match[2].str();
      }
   }
   return -1;
}


// ###### Remove input file from reader #####################################
bool NorNetEdgePingReader::removeFile(const std::filesystem::path& dataFile,
                                      const std::smatch            match)
{
   if(match.size() == 3) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[2].str(), timeStamp, "%Y-%m-%d_%H-%M-%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.TimeStamp     = timeStamp;
         inputFileEntry.MeasurementID = atol(match[1].str().c_str());
         inputFileEntry.DataFile      = dataFile;
         const int workerID = inputFileEntry.MeasurementID % Workers;

         HPCT_LOG(trace) << Identification << ": Removing data file " << dataFile << " from reader";
         std::unique_lock lock(Mutex);
         if(DataFileSet[workerID].erase(inputFileEntry) == 1) {
            assert(TotalFiles > 0);
            TotalFiles--;
            return true;
         }
      }
   }
   return false;
}


// ###### Fetch list of input files #########################################
unsigned int NorNetEdgePingReader::fetchFiles(std::list<std::filesystem::path>& dataFileList,
                                              const unsigned int                worker,
                                              const unsigned int                limit)
{
   assert(worker < Workers);
   dataFileList.clear();

   std::unique_lock lock(Mutex);

   for(const InputFileEntry& inputFileEntry : DataFileSet[worker]) {
      dataFileList.push_back(inputFileEntry.DataFile);
      if(dataFileList.size() >= limit) {
         break;
      }
   }
   return dataFileList.size();
}


// ###### Begin parsing #####################################################
void NorNetEdgePingReader::beginParsing(DatabaseClientBase& databaseClient,
                                        unsigned long long& rows)
{
   rows = 0;

   // ====== Generate import statement ======================================
   const DatabaseBackendType backend = databaseClient.getBackend();
   if(backend & DatabaseBackendType::SQL_Generic) {
      databaseClient.clearStatement();
      databaseClient.getStatement()
         << "INSERT INTO " << Table_measurement_generic_data
         << "(ts, mi_id, seq, xml_data, crc, stats) VALUES \n";
   }
   else {
      throw ImporterLogicException("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool NorNetEdgePingReader::finishParsing(DatabaseClientBase& databaseClient,
                                         unsigned long long& rows)
{
   if(rows > 0) {
      // ====== Generate import statement ===================================
      const DatabaseBackendType backend = databaseClient.getBackend();
      if(backend & DatabaseBackendType::SQL_Generic) {
         if(rows > 0) {
            databaseClient.getStatement()
               << "\nON DUPLICATE KEY UPDATE stats=stats;\n";
            databaseClient.executeStatement();
         }
         else {
            databaseClient.clearStatement();
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
            throw ImporterReaderDataErrorException("Too many columns in input file " + dataFile.string());
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns != NorNetEdgePingColumns) {
         throw ImporterReaderDataErrorException("Too few columns in input file " + dataFile.string());
      }

      // ====== Generate import statement ===================================
      if(backend & DatabaseBackendType::SQL_Generic) {

         if(rows > 0) {
            databaseClient.getStatement() << ",\n";
         }
         databaseClient.getStatement()
            << "("
            << "'" << tuple[0] << "', "
            << std::stoul(tuple[1]) << ", "
            << std::stoul(tuple[2]) << ", "
            << "'" << tuple[3] << "', CRC32(xml_data), 10 + mi_id MOD 10)";
         rows++;
      }
      else {
         throw ImporterLogicException("Unknown output format");
      }
   }
}


// ###### Print reader status ###############################################
void NorNetEdgePingReader::printStatus(std::ostream& os)
{
   os << "NorNetEdgePing:" << std::endl;
   for(unsigned int w = 0; w < Workers; w++) {
      os << " - Work Queue #" << w + 1 << ": " << DataFileSet[w].size() << std::endl;
      // for(const InputFileEntry& inputFileEntry : DataFileSet[w]) {
      //    os << "  - " <<  inputFileEntry << std::endl;
      // }
   }
}
