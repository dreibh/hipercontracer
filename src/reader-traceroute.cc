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

#include "reader-traceroute.h"
#include "importer-exception.h"
#include "logger.h"
#include "tools.h"


const std::string TracerouteReader::Identification = "Ping";
const std::regex  TracerouteReader::FileNameRegExp = std::regex(
   // Format: Ping-<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Ping-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const TracerouteReader::InputFileEntry& a,
               const TracerouteReader::InputFileEntry& b)
{
   // ====== Level 1: TimeStamp =============================================
   if(a.Source < b.Source) {
      return true;
   }
   else if(a.Source == b.Source) {
      // ====== Level 2: MeasurementID ======================================
      if(a.TimeStamp < b.TimeStamp) {
         return true;
      }
      else if(a.TimeStamp == b.TimeStamp) {
         // ====== Level 3: SeqNumber =======================================
         if(a.SeqNumber < b.SeqNumber) {
            return true;
         }
         else if(a.SeqNumber == b.SeqNumber) {
            // ====== Level 4: DataFile =====================================
            if(a.DataFile < b.DataFile) {
               return true;
            }
         }
      }
   }
   return false;
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const TracerouteReader::InputFileEntry& entry)
{
   os << "("
      << entry.Source << ", "
      << timePointToString<TracerouteReader::FileEntryTimePoint>(entry.TimeStamp) << ", "
      << entry.SeqNumber << ", "
      << entry.DataFile
      << ")";
   return os;
}


// ###### Constructor #######################################################
TracerouteReader::TracerouteReader(const unsigned int workers,
                                   const unsigned int maxTransactionSize,
                                   const std::string& table)
   : ReaderBase(workers, maxTransactionSize),
     Table(table)
{
   DataFileSet = new std::set<InputFileEntry>[Workers];
   assert(DataFileSet != nullptr);
}


// ###### Destructor ########################################################
TracerouteReader::~TracerouteReader()
{
   delete [] DataFileSet;
   DataFileSet = nullptr;
}


// ###### Get identification of reader ######################################
const std::string& TracerouteReader::getIdentification() const
{
   return Identification;
}


// ###### Get input file name regular expression ############################
const std::regex& TracerouteReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


// ###### Add input file to reader ##########################################
int TracerouteReader::addFile(const std::filesystem::path& dataFile,
                              const std::smatch            match)
{
   if(match.size() == 5) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[3].str(), timeStamp, "%Y%m%dT%H%M%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.Source    = match[2];
         inputFileEntry.TimeStamp = timeStamp;
         inputFileEntry.SeqNumber = atol(match[4].str().c_str());
         const std::size_t workerID = std::hash<std::string>{}(inputFileEntry.Source) % Workers;

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
bool TracerouteReader::removeFile(const std::filesystem::path& dataFile,
                                  const std::smatch            match)
{
   if(match.size() == 5) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[3].str(), timeStamp, "%Y%m%dT%H%M%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.Source    = match[2];
         inputFileEntry.TimeStamp = timeStamp;
         inputFileEntry.SeqNumber = atol(match[4].str().c_str());
         const std::size_t workerID = std::hash<std::string>{}(inputFileEntry.Source) % Workers;

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
unsigned int TracerouteReader::fetchFiles(std::list<std::filesystem::path>& dataFileList,
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
void TracerouteReader::beginParsing(DatabaseClientBase& databaseClient,
                                    unsigned long long& rows)
{
   rows = 0;

   // ====== Generate import statement ======================================
   const DatabaseBackendType backend = databaseClient.getBackend();
   if(backend & DatabaseBackendType::SQL_Generic) {
//       assert(databaseClient.statementIsEmpty());
//       databaseClient.getStatement()
//          << "INSERT INTO " << Table_measurement_generic_data
//          << "(ts, mi_id, seq, xml_data, crc, stats) VALUES \n";
   }
   else {
      throw ImporterLogicException("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool TracerouteReader::finishParsing(DatabaseClientBase& databaseClient,
                                     unsigned long long& rows)
{
   if(rows > 0) {
      // ====== Generate import statement ===================================
      const DatabaseBackendType backend = databaseClient.getBackend();
      if(backend & DatabaseBackendType::SQL_Generic) {
         if(rows > 0) {
//             databaseClient.getStatement()
//                << "\nON DUPLICATE KEY UPDATE stats=stats;\n";
//             databaseClient.executeStatement();
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
void TracerouteReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        boost::iostreams::filtering_istream& inputStream)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   assert(false);
//    static const unsigned int TracerouteColumns   = 4;
//    static const char         TracerouteDelimiter = '\t';
//
//    std::string inputLine;
//    std::string tuple[TracerouteColumns];
//    while(std::getline(inputStream, inputLine)) {
//       // ====== Parse line ==================================================
//       size_t columns = 0;
//       size_t start;
//       size_t end = 0;
//       while((start = inputLine.find_first_not_of(TracerouteDelimiter, end)) != std::string::npos) {
//          end = inputLine.find(TracerouteDelimiter, start);
//
//          if(columns == TracerouteColumns) {
//             throw ImporterReaderException("Too many columns in input file");
//          }
//          tuple[columns++] = inputLine.substr(start, end - start);
//       }
//       if(columns != TracerouteColumns) {
//          throw ImporterReaderException("Too few columns in input file");
//       }
//
//       // ====== Generate import statement ===================================
//       if(backend & DatabaseBackendType::SQL_Generic) {
//
//          if(rows > 0) {
//             databaseClient.getStatement() << ",\n";
//          }
//          databaseClient.getStatement()
//             << "("
//             << "'" << tuple[0] << "', "
//             << std::stoul(tuple[1]) << ", "
//             << std::stoul(tuple[2]) << ", "
//             << "'" << tuple[3] << "', CRC32(xml_data), 10 + mi_id MOD 10)";
//          rows++;
//       }
//       else {
//          throw ImporterLogicException("Unknown output format");
//       }
//    }
}


// ###### Print reader status ###############################################
void TracerouteReader::printStatus(std::ostream& os)
{
   os << "Traceroute:" << std::endl;
   for(unsigned int w = 0; w < Workers; w++) {
      os << " - Work Queue #" << w + 1 << ": " << DataFileSet[w].size() << std::endl;
      // for(const InputFileEntry& inputFileEntry : DataFileSet[w]) {
      //    os << "  - " <<  inputFileEntry << std::endl;
      // }
   }
}
