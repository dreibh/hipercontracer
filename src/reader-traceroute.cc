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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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


const std::string TracerouteReader::Identification("Traceroute");
const std::regex  TracerouteReader::FileNameRegExp(
   // Format: Traceroute-(Protocol-|)[P#]<ID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results<EXT>
   "^Traceroute-([A-Z]+-|)([#P])([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
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
   if(match.size() == 7) {
      ReaderTimePoint timeStamp;
      if(stringToTimePoint<ReaderTimePoint>(match[5].str(), timeStamp, "%Y%m%dT%H%M%S")) {
         inputFileEntry.Source    = match[4];
         inputFileEntry.TimeStamp = timeStamp;
         inputFileEntry.SeqNumber = atol(match[6].str().c_str());
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


// ###### Parse measurement ID ##############################################
unsigned long long TracerouteReader::parseMeasurementID(const std::string&           value,
                                                        const std::filesystem::path& dataFile)
{
   size_t                   index;
   const unsigned long long measurementID = std::stoull(value, &index, 10);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad measurement ID value " + value);
   }
   return measurementID;
}


// ###### Parse time stamp ##################################################
boost::asio::ip::address TracerouteReader::parseAddress(const std::string&           value,
                                                        const std::filesystem::path& dataFile)
{
   const boost::asio::ip::address address = boost::asio::ip::make_address(value);
   return address;
}


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
      throw ImporterReaderDataErrorException("Invalid time stamp value " + value);
   }
   return timeStamp;
}


// ###### Parse round number ################################################
unsigned int TracerouteReader::parseRoundNumber(const std::string&           value,
                                                const std::filesystem::path& dataFile)
{
   size_t        index       = 0;
   unsigned long roundNumber = 0;
   try {
      roundNumber = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad round number " + value);
   }
   return roundNumber;
}


// ###### Parse traffic class ###############################################
uint8_t TracerouteReader::parseTrafficClass(const std::string&           value,
                                            const std::filesystem::path& dataFile)
{
   size_t        index        = 0;
   unsigned long trafficClass = 0;
   try {
      trafficClass = std::stoul(value, &index, 16);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad traffic class format " + value);
   }
   if(trafficClass > 0xff) {
      throw ImporterReaderDataErrorException("Invalid traffic class value " + value);
   }
   return (uint8_t)trafficClass;
}


// ###### Parse packet size #################################################
unsigned int TracerouteReader::parsePacketSize(const std::string&           value,
                                               const std::filesystem::path& dataFile)
{
   size_t        index      = 0;
   unsigned long packetSize = 0;
   try {
      packetSize = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad packet size format " + value);
   }
   return packetSize;
}


// ###### Parse response size #################################################
unsigned int TracerouteReader::parseResponseSize(const std::string&           value,
                                                const std::filesystem::path& dataFile)
{
   size_t        index      = 0;
   unsigned long responseSize = 0;
   try {
      responseSize = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad response size format " + value);
   }
   return responseSize;
}


// ###### Parse time stamp ##################################################
uint16_t TracerouteReader::parseChecksum(const std::string&           value,
                                         const std::filesystem::path& dataFile)
{
   size_t        index    = 0;
   unsigned long checksum = 0;
   try {
      checksum = std::stoul(value, &index, 16);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad checksum format " + value);
   }
   if(checksum > 0xffff) {
      throw ImporterReaderDataErrorException("Invalid checksum value " + value);
   }
   return (uint16_t)checksum;
}


// ###### Parse status ######################################################
unsigned int TracerouteReader::parseStatus(const std::string&           value,
                                           const std::filesystem::path& dataFile,
                                           const unsigned int           base)
{
   size_t        index  = 0;
   unsigned long status = 0;
   try {
      status = std::stoul(value, &index, base);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad status format " + value);
   }
   return status;
}


// ###### Parse path hash ###################################################
long long TracerouteReader::parsePathHash(const std::string&           value,
                                          const std::filesystem::path& dataFile)
{
   size_t   index    = 0;
   uint64_t pathHash = 0;
   try {
      pathHash = std::stoull(value, &index, 16);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad path hash " + value);
   }
   // Cast to signed long long as-is:
   return (long long)pathHash;
}


// ###### Parse total number of hops ########################################
unsigned int TracerouteReader::parseTotalHops(const std::string&           value,
                                              const std::filesystem::path& dataFile)
{
   size_t        index     = 0;
   unsigned long totalHops = 0;
   try {
      totalHops = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad total hops value " + value);
   }
   if( (totalHops < 1) || (totalHops > 255) ) {
      throw ImporterReaderDataErrorException("Invalid total hops value " + value);
   }
   return totalHops;
}


// ###### Parse packet size #################################################
unsigned int TracerouteReader::parseTimeSource(const std::string&           value,
                                               const std::filesystem::path& dataFile)
{
   size_t       index      = 0;
   unsigned int timeSource = 0;
   try {
      timeSource = std::stoul(value, &index, 16);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad time source format " + value);
   }
   return timeSource;
}


// ###### Parse microseconds ################################################
long long TracerouteReader::parseMicroseconds(const std::string&           value,
                                              const std::filesystem::path& dataFile)
{
   size_t        index = 0;
   unsigned long us    = 0;
   try {
      us = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad microseconds format " + value);
   }
   return 1000LL * us;
}


// ###### Parse nanoseconds ################################################
long long TracerouteReader::parseNanoseconds(const std::string&           value,
                                             const std::filesystem::path& dataFile)
{
   size_t        index = 0;
   unsigned long ns    = 0;
   try {
      ns = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad nanoseconds format " + value);
   }
   return ns;
}


// ###### Begin parsing #####################################################
void TracerouteReader::beginParsing(DatabaseClientBase& databaseClient,
                                    unsigned long long& rows)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement              = databaseClient.getStatement("Traceroute", false, true);

   rows = 0;

   // ====== Generate import statement ======================================
   if(backend & DatabaseBackendType::SQL_Generic) {
      statement
         << "INSERT INTO " << Table
         << " (TimeStamp,FromIP,ToIP,Round,Checksum,PktSize,TC,HopNumber,TotalHops,Status,RTT,HopIP,PathHash) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      statement << "{ \"" << Table <<  "\": [";
   }
   else {
      throw ImporterLogicException("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool TracerouteReader::finishParsing(DatabaseClientBase& databaseClient,
                                     unsigned long long& rows)
{
   const DatabaseBackendType backend   = databaseClient.getBackend();
   Statement&                statement = databaseClient.getStatement("Traceroute");
   assert(statement.getRows() == rows);

   if(rows > 0) {
      // ====== Generate import statement ===================================
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
void TracerouteReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        const std::filesystem::path&         dataFile,
        boost::iostreams::filtering_istream& dataStream)
{
   Statement&                statement = databaseClient.getStatement("Traceroute");
   const DatabaseBackendType backend   = databaseClient.getBackend();
   static const unsigned int TracerouteMinColumns = 4;
   static const unsigned int TracerouteMaxColumns = 11;
   static const char         PingDelimiter  = ' ';

   ReaderTimePoint           timeStamp;
   boost::asio::ip::address  sourceIP;
   boost::asio::ip::address  destinationIP;
   unsigned int              roundNumber  = 0;
   uint16_t                  checksum     = 0;
   unsigned int              totalHops    = 0;
   unsigned int              statusFlags  = ~0U;
   long long                 pathHash     = 0;
   uint8_t                   trafficClass = 0x00;
   unsigned int              packetSize   = 0;

   std::string inputLine;
   std::string tuple[TracerouteMaxColumns];
   const ReaderTimePoint now = ReaderClock::now();
   while(std::getline(dataStream, inputLine)) {
      // ====== Parse line ==================================================
      size_t columns = 0;
      size_t start;
      size_t end = 0;
      while((start = inputLine.find_first_not_of(PingDelimiter, end)) != std::string::npos) {
         end = inputLine.find(PingDelimiter, start);
         if(columns == TracerouteMaxColumns) {
            // Skip additional columns
            break;
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns < TracerouteMinColumns) {
         throw ImporterReaderDataErrorException("Too few columns in input file " + dataFile.string());
      }

      // ====== Generate import statement ===================================
      if((columns >= 9) && (tuple[0] == "#T"))  {
         if( (statusFlags != ~0U) && (backend & DatabaseBackendType::NoSQL_Generic) ) {
            statement << "]";
            statement.endRow();
            rows++;
         }
         timeStamp     = parseTimeStamp(tuple[3], now, dataFile);
         sourceIP      = parseAddress(tuple[1], backend, dataFile);
         destinationIP = parseAddress(tuple[2], backend, dataFile);
         roundNumber   = parseRoundNumber(tuple[4], dataFile);
         checksum      = parseChecksum(tuple[5], dataFile);
         totalHops     = parseTotalHops(tuple[6], dataFile);
         statusFlags   = parseStatus(tuple[7], dataFile);
         pathHash      = parsePathHash(tuple[8], dataFile);
         trafficClass  = 0x0;
         packetSize    = 0;
         if(columns >= 10) {   // TrafficClass was added in HiPerConTracer 1.4.0!
            trafficClass = parseTrafficClass(tuple[9], dataFile);
            if(columns >= 11) {   // PacketSize was added in HiPerConTracer 1.6.0!
               packetSize = parsePacketSize(tuple[10], dataFile);
            }
         }

         if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement.beginRow();
            statement
               << "\"timestamp\": "   << timePointToMicroseconds<ReaderTimePoint>(timeStamp) << statement.sep()
               << "\"source\": "      << statement.encodeAddress(sourceIP)                   << statement.sep()
               << "\"destination\": " << statement.encodeAddress(destinationIP)              << statement.sep()
               << "\"round\": "       << roundNumber                                         << statement.sep()
               << "\"checksum\": "    << checksum                                            << statement.sep()
               << "\"pktsize\": "     << packetSize                                          << statement.sep()
               << "\"tc\": "          << (unsigned int)trafficClass                          << statement.sep()
               << "\"statusFlags\": " << statusFlags                                         << statement.sep()
               << "\"totalHops\": "   << totalHops                                           << statement.sep()
               << "\"pathHash\": "    << pathHash                                            << statement.sep()
               << "\"hops\": [ ";
         }
      }
      else if(tuple[0] == "\t")  {
         if(statusFlags == ~0U) {
            throw ImporterReaderDataErrorException("Hop data has no corresponding #T line");
         }
         const unsigned int hopNumber   = parseTotalHops(tuple[1], dataFile);
         const unsigned int status      = parseStatus(tuple[2], dataFile);
         const unsigned int rtt         = parseRTT(tuple[3], dataFile);
         boost::asio::ip::address hopIP = parseAddress(tuple[4], backend, dataFile);

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << statement.quote(timePointToString<ReaderTimePoint>(timeStamp, 6)) << statement.sep()
               << statement.encodeAddress(sourceIP)      << statement.sep()
               << statement.encodeAddress(destinationIP) << statement.sep()
               << roundNumber                            << statement.sep()
               << checksum                               << statement.sep()
               << packetSize                             << statement.sep()
               << (unsigned int)trafficClass             << statement.sep()
               << hopNumber                              << statement.sep()
               << totalHops                              << statement.sep()
               << (status | statusFlags)                 << statement.sep()
               << rtt                                    << statement.sep()
               << statement.encodeAddress(hopIP)         << statement.sep()
               << pathHash;
            statement.endRow();
            rows++;
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement
               << ((hopNumber > 1) ? ", { " : " { ")
               << "\"hop\": "    << statement.encodeAddress(hopIP) << statement.sep()
               << "\"status\": " << status                         << statement.sep()
               << "\"rtt\": "    << rtt
               << " }";
         }
         else {
            throw ImporterLogicException("Unknown output format");
         }
      }
      else {
         throw ImporterReaderDataErrorException("Unexpected input in input file " + dataFile.string());
      }
   }
   if( (statusFlags != ~0U) && (backend & DatabaseBackendType::NoSQL_Generic) ) {
      statement << "]";
      statement.endRow();
      rows++;
   }
}
