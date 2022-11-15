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


const std::string TracerouteReader::Identification = "Traceroute";
const std::regex  TracerouteReader::FileNameRegExp = std::regex(
   // Format: Traceroute-P<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Traceroute-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const TracerouteFileEntry& a, const TracerouteFileEntry& b)
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
std::ostream& operator<<(std::ostream& os, const TracerouteFileEntry& entry)
{
   os << "("
      << entry.Source << ", "
      << timePointToString<ReaderTimePoint>(entry.TimeStamp, 6) << ", "
      << entry.SeqNumber << ", "
      << entry.DataFile
      << ")";
   return os;
}


// ###### Make TracerouteFileEntry from file name  ##########################
int makeInputFileEntry(const std::filesystem::path& dataFile,
                       const std::smatch            match,
                       TracerouteFileEntry&         inputFileEntry,
                       const unsigned int           workers)
{
   if(match.size() == 5) {
      ReaderTimePoint timeStamp;
      if(stringToTimePoint<ReaderTimePoint>(match[3].str(), timeStamp, "%Y%m%dT%H%M%S")) {
         inputFileEntry.Source    = match[2];
         inputFileEntry.TimeStamp = timeStamp;
         inputFileEntry.SeqNumber = atol(match[4].str().c_str());
         inputFileEntry.DataFile  = dataFile;
         const std::size_t workerID = std::hash<std::string>{}(inputFileEntry.Source) % workers;
         // std::cout << inputFileEntry.Source << "\t" << timePointToString<ReaderTimePoint>(inputFileEntry.TimeStamp, 6) << "\t" << inputFileEntry.SeqNumber << "\t" << inputFileEntry.DataFile << " -> " << workerID << "\n";
         return workerID;
      }
   }
   return -1;
}


// ###### Get priority of TracerouteFileEntry ###############################
ReaderPriority getPriorityOfFileEntry(const TracerouteFileEntry& inputFileEntry)
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
TracerouteReader::TracerouteReader(const DatabaseConfiguration& databaseConfiguration,
                                   const unsigned int           workers,
                                   const unsigned int           maxTransactionSize,
                                   const std::string&           table)
   : ReaderImplementation<TracerouteFileEntry>(databaseConfiguration, workers, maxTransactionSize),
     Table(table)
{ }


// ###### Destructor ########################################################
TracerouteReader::~TracerouteReader()
{ }


// ###### Parse time stamp ##################################################
ReaderTimePoint TracerouteReader::parseTimeStamp(const std::string&           value,
                                                 const ReaderTimePoint&       now,
                                                 const std::filesystem::path& dataFile)
{
   size_t                   index;
   const unsigned long long ts = std::stoull(value, &index, 16);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad time stamp format " + value);
   }
   const ReaderTimePoint timeStamp = microsecondsToTimePoint<ReaderTimePoint>(ts);
   if( (timeStamp < now - std::chrono::hours(365 * 24)) ||   /* 1 year in the past  */
       (timeStamp > now + std::chrono::hours(24)) ) {        /* 1 day in the future */
      throw ImporterReaderDataErrorException("Bad time stamp value " + value);
   }
   return timeStamp;
}


// ###### Parse time stamp ##################################################
boost::asio::ip::address TracerouteReader::parseAddress(const std::string&           value,
                                                        const DatabaseBackendType    backend,
                                                        const std::filesystem::path& dataFile)
{
   const boost::asio::ip::address address = boost::asio::ip::make_address(value);
   if( (backend & DatabaseBackendType::SQL_MariaDB) == DatabaseBackendType::SQL_MariaDB ) {
      if(address.is_v4()) {
         // MySQL/MariaDB only has INET6 datatype. Make IPv4 addresses mapped.
         return boost::asio::ip::make_address_v6(boost::asio::ip::v4_mapped, address.to_v4());
      }
   }
   return address;
}


// ###### Parse time stamp ##################################################
uint16_t TracerouteReader::parseChecksum(const std::string&           value,
                                         const std::filesystem::path& dataFile)
{
   size_t              index;
   const unsigned long checksum = std::stoul(value, &index, 16);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad checksum format " + value);
   }
   if(checksum > 0xffff) {
      throw ImporterReaderDataErrorException("Bad checksum value " + value);
   }
   return (uint16_t)checksum;
}


// ###### Parse status ######################################################
unsigned int TracerouteReader::parseStatus(const std::string&           value,
                                           const std::filesystem::path& dataFile)
{
   size_t              index;
   const unsigned long status = std::stoul(value, &index, 10);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad status format " + value);
   }
   return status;
}


// ###### Parse RTT #########################################################
unsigned int TracerouteReader::parseRTT(const std::string&           value,
                                        const std::filesystem::path& dataFile)
{
   size_t              index;
   const unsigned long rtt = std::stoul(value, &index, 10);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad RTT format " + value);
   }
   return rtt;
}


// ###### Parse packet size #################################################
unsigned int TracerouteReader::parsePacketSize(const std::string&           value,
                                               const std::filesystem::path& dataFile)
{
   size_t              index;
   const unsigned long packetSize = std::stoul(value, &index, 10);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad packet size format " + value);
   }
   return packetSize;
}


// ###### Parse traffic class ###############################################
uint8_t TracerouteReader::parseTrafficClass(const std::string&           value,
                                            const std::filesystem::path& dataFile)
{
   size_t              index;
   const unsigned long trafficClass = std::stoul(value, &index, 16);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad traffic class format " + value);
   }
   if(trafficClass > 0xffff) {
      throw ImporterReaderDataErrorException("Bad traffic class value " + value);
   }
   return (uint8_t)trafficClass;
}


// ###### Convert IP address into bytes string format #######################
std::string TracerouteReader::addressToBytesString(const boost::asio::ip::address& address)
{
   std::stringstream byteString;
   if(address.is_v4()) {
      const boost::asio::ip::address_v4::bytes_type b = address.to_v4().to_bytes();
      for(unsigned int i = 0; i < 4; i++) {
         byteString << "\\x" << std::hex << (unsigned int)b[i];
      }
   }
   else {
      const boost::asio::ip::address_v6::bytes_type b = address.to_v6().to_bytes();
      for(unsigned int i = 0; i < 16; i++) {
         byteString << "\\x" << std::hex << (unsigned int)b[i];
      }
   }
   return byteString.str();
}


// ###### Begin parsing #####################################################
void TracerouteReader::beginParsing(DatabaseClientBase& databaseClient,
                                    unsigned long long& rows)
{
   rows = 0;

   // ====== Generate import statement ======================================
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement              = databaseClient.getStatement("Traceroute", false, true);
   if(backend & DatabaseBackendType::SQL_Generic) {
//       statement
//          << "INSERT INTO " << Table
//          << " (TimeStamp, FromIP, ToIP, Checksum, PktSize, TC, Status, RTT) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      statement << "db['" << Table << "'].insert(";
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
      const DatabaseBackendType backend   = databaseClient.getBackend();
      Statement&                statement = databaseClient.getStatement("Traceroute");
      if(backend & DatabaseBackendType::SQL_Generic) {
         if(rows > 0) {
            databaseClient.executeUpdate(statement);
         }
         else {
            statement.clear();
         }
      }
      else if(backend & DatabaseBackendType::NoSQL_Generic) {
         if(rows > 0) {
            statement << ")";
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
void TracerouteReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        const std::filesystem::path&         dataFile,
        boost::iostreams::filtering_istream& dataStream)
{
   assert(false);
//    const DatabaseBackendType backend = databaseClient.getBackend();
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
