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
      throw ImporterReaderDataErrorException("Bad measurement ID value " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
   }
   return measurementID;
}


// ###### Parse time stamp ##################################################
boost::asio::ip::address TracerouteReader::parseAddress(const std::string&           value,
                                                        const std::filesystem::path& dataFile)
{
   try {
      const boost::asio::ip::address address = boost::asio::ip::make_address(value);
      return address;
   }
   catch(...) { }
   throw ImporterReaderDataErrorException("Bad address " + value +
                                          " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
}


// ###### Parse time stamp ##################################################
ReaderTimePoint TracerouteReader::parseTimeStamp(const std::string&           value,
                                                 const ReaderTimePoint&       now,
                                                 const bool                   inNanoseconds,
                                                 const std::filesystem::path& dataFile)
{
   size_t                   index;
   const unsigned long long ts = std::stoull(value, &index, 16);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad time stamp format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
   }
   const ReaderTimePoint timeStamp = (inNanoseconds == true) ? nanosecondsToTimePoint<ReaderTimePoint>(ts) :
                                                               nanosecondsToTimePoint<ReaderTimePoint>(1000ULL * ts);
   if( (timeStamp < now - std::chrono::hours(365 * 24)) ||   /* 1 year in the past  */
       (timeStamp > now + std::chrono::hours(24)) ) {        /* 1 day in the future */
      throw ImporterReaderDataErrorException("Invalid time stamp value " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad round number " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad traffic class format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
   }
   if(trafficClass > 0xff) {
      throw ImporterReaderDataErrorException("Invalid traffic class value " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad packet size format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad response size format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad checksum format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
   }
   if(checksum > 0xffff) {
      throw ImporterReaderDataErrorException("Invalid checksum value " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad status format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad path hash " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad total hops value " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
   }
   if( (totalHops < 1) || (totalHops > 255) ) {
      throw ImporterReaderDataErrorException("Invalid total hops value " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad time source format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad microseconds format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
      throw ImporterReaderDataErrorException("Bad nanoseconds format " + value +
                                             " in input file " + relativeTo(dataFile, Configuration.getImportFilePath()).string());
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
         << " (Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,HopNumber,TotalHops,PacketSize,ResponseSize,Checksum,Status,PathHash,SendTimestamp,HopIP,TimeSource,Delay_AppSend,Delay_Queuing,Delay_AppReceive,RTT_App,RTT_SW,RTT_HW) VALUES";
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
   static const unsigned int TracerouteMaxColumns = 12;
   static const char         TracerouteDelimiter  = ' ';

   unsigned int              version       = 0;
   char                      protocol      = 0x00;
   unsigned int              measurementID = 0;
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
      while((start = inputLine.find_first_not_of(TracerouteDelimiter, end)) != std::string::npos) {
         end = inputLine.find(TracerouteDelimiter, start);
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
      if( (tuple[0].size() >= 2) && (tuple[0][0] == '#') && (tuple[0][1] == 'T') ) {
         // ------ Obtain version -------------------------------------------
         if( (tuple[0].size() > 2) && (columns >= 12) ) {
            version  = 2;
            protocol = tuple[0][2];
         }
         else if( (tuple[0].size() == 2) && (columns >= 9) ) {
            version  = 1;
            protocol = 'i';
         }
         else {
            throw ImporterReaderDataErrorException("Unexpected syntax in input file " +
                                                   relativeTo(dataFile, Configuration.getImportFilePath()).string());
         }


         measurementID   = (version >= 2) ? parseMeasurementID(tuple[1], dataFile)        : 0;
         sourceIP        = (version >= 2) ? parseAddress(tuple[2], dataFile)              : parseAddress(tuple[1], dataFile);
         destinationIP   = (version >= 2) ? parseAddress(tuple[3], dataFile)              : parseAddress(tuple[2], dataFile);
         timeStamp       = (version >= 2) ? parseTimeStamp(tuple[4], now, true, dataFile) :  parseTimeStamp(tuple[3], now, false, dataFile);
         roundNumber     = (version >= 2) ? parseRoundNumber(tuple[5], dataFile)          : parseRoundNumber(tuple[4], dataFile);
         totalHops       = parseTotalHops(tuple[6], dataFile);
         trafficClass    = (version >= 2) ? parseTrafficClass(tuple[7], dataFile)         : 0x00;
         packetSize      = (version >= 2) ? parsePacketSize(tuple[8], dataFile)           : parsePacketSize(tuple[10], dataFile);
         checksum        = (version >= 2) ? parseChecksum(tuple[9], dataFile)             : parseChecksum(tuple[5], dataFile);
         statusFlags     = (version >= 2) ? parseStatus(tuple[10], dataFile)              :  parseStatus(tuple[7], dataFile);
         pathHash        = (version >= 2) ? parsePathHash(tuple[11], dataFile)            : parsePathHash(tuple[8], dataFile);

         if(version == 1) {
            if(columns >= 10) {   // TrafficClass was added in HiPerConTracer 1.4.0!
               trafficClass = parseTrafficClass(tuple[9], dataFile);
               if(columns >= 11) {   // PacketSize was added in HiPerConTracer 1.6.0!
                  packetSize = parsePacketSize(tuple[10], dataFile);
               }
            }
         }

         if( (statusFlags != ~0U) && (backend & DatabaseBackendType::NoSQL_Generic) ) {
            statement << "]";
            statement.endRow();
            rows++;
         }

         if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement.beginRow();
            statement
               << "\"timestamp\": "     << timePointToNanoseconds<ReaderTimePoint>(timeStamp) << statement.sep()
               << "\"measurementID\": " << measurementID                                       << statement.sep()
               << "\"source\": "        << statement.encodeAddress(sourceIP)                   << statement.sep()
               << "\"destination\": "   << statement.encodeAddress(destinationIP)              << statement.sep()
               << "\"round\": "         << roundNumber                                         << statement.sep()
               << "\"checksum\": "      << checksum                                            << statement.sep()
               << "\"pktsize\": "       << packetSize                                          << statement.sep()
               << "\"tc\": "            << (unsigned int)trafficClass                          << statement.sep()
               << "\"statusFlags\": "   << statusFlags                                         << statement.sep()
               << "\"totalHops\": "     << totalHops                                           << statement.sep()
               << "\"pathHash\": "      << pathHash                                            << statement.sep()
               << "\"hops\": [ ";
         }
      }
      else if( (tuple[0].size() >= 1) && (tuple[0][0] == '\t') ) {
         if(statusFlags == ~0U) {
            throw ImporterReaderDataErrorException("Hop data has no corresponding #T line");
         }

         const ReaderTimePoint          sendTimeStamp   = (version >= 2) ? parseTimeStamp(tuple[0], now, true, dataFile) : timeStamp;
         const unsigned int             hopNumber       = (version >= 2) ? parseTotalHops(tuple[1], dataFile)    : parseTotalHops(tuple[0], dataFile);
         const unsigned int             responseSize    = (version >= 2) ? parseResponseSize(tuple[2], dataFile) : 0;
         const unsigned int             status          = (version >= 2) ? parseStatus(tuple[3], dataFile, 10)   : parseStatus(tuple[2], dataFile);
         const boost::asio::ip::address hopIP           = (version >= 2) ? parseAddress(tuple[11], dataFile)     : parseAddress(tuple[4], dataFile);

         unsigned int                   timeSource      = (version >= 2) ? parseTimeSource(tuple[4], dataFile)   : 0x00000000;
         const long long                delayAppSend    = (version >= 2) ? parseNanoseconds(tuple[5], dataFile)  : -1;
         const long long                delayQueuing    = (version >= 2) ? parseNanoseconds(tuple[6], dataFile)  : -1;
         const long long                delayAppReceive = (version >= 2) ? parseNanoseconds(tuple[7], dataFile)  : -1;
         const long long                rttApp          = (version >= 2) ? parseNanoseconds(tuple[8], dataFile)  : parseMicroseconds(tuple[2], dataFile);
         const long long                rttHardware     = (version >= 2) ? parseNanoseconds(tuple[9], dataFile)  : -1;
         const long long                rttSoftware     = (version >= 2) ? parseNanoseconds(tuple[10], dataFile) : -1;

         if(version == 1) {
            if(columns >= 5) {   // TimeSource was added in HiPerConTracer 2.0.0!
               timeSource = parseTimeSource(tuple[4], dataFile);
            }
         }

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << statement.quote(timePointToString<ReaderTimePoint>(timeStamp, 9))     << statement.sep()
               << measurementID                                                         << statement.sep()
               << statement.encodeAddress(sourceIP)                                     << statement.sep()
               << statement.encodeAddress(destinationIP)                                << statement.sep()
               << (unsigned int)protocol                                                << statement.sep()
               << (unsigned int)trafficClass                                            << statement.sep()
               << roundNumber                                                           << statement.sep()
               << hopNumber                                                             << statement.sep()
               << totalHops                                                             << statement.sep()
               << packetSize                                                            << statement.sep()
               << responseSize                                                          << statement.sep()
               << checksum                                                              << statement.sep()
               << (status | statusFlags)                                                << statement.sep()
               << pathHash                                                              << statement.sep()
               << statement.quote(timePointToString<ReaderTimePoint>(sendTimeStamp, 9)) << statement.sep()
               << statement.encodeAddress(hopIP)                                        << statement.sep()

               << (long long)timeSource                                                 << statement.sep()
               << delayAppSend                                                          << statement.sep()
               << delayQueuing                                                          << statement.sep()
               << delayAppReceive                                                       << statement.sep()
               << rttApp                                                                << statement.sep()
               << rttSoftware                                                           << statement.sep()
               << rttHardware;
            statement.endRow();
            rows++;
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement
               << ((hopNumber > 1) ? ", { " : " { ")

               << statement.quote(timePointToString<ReaderTimePoint>(sendTimeStamp, 9)) << statement.sep()
               << "\"respsize\": "      << responseSize                                 << statement.sep()
               << "\"hop\": "           << statement.encodeAddress(hopIP)               << statement.sep()
               << "\"status\": "        << status                                       << statement.sep()

               << "\"timesource\": "    << timeSource                                   << statement.sep()
               << "\"delay.appsend\": " << delayAppSend                                 << statement.sep()
               << "\"delay.queuing\": " << delayQueuing                                 << statement.sep()
               << "\"delay.apprecv\": " << delayAppReceive                              << statement.sep()
               << "\"rtt.app\": "       << rttApp                                       << statement.sep()
               << "\"rtt.sw\": "        << rttSoftware                                  << statement.sep()
               << "\"rtt.hw\": "        << rttHardware

               << " }";
         }
         else {
            throw ImporterLogicException("Unknown output format");
         }
      }
      else {
         throw ImporterReaderDataErrorException("Unexpected input in input file " +
                                                relativeTo(dataFile, Configuration.getImportFilePath()).string());
      }
   }
   if( (statusFlags != ~0U) && (backend & DatabaseBackendType::NoSQL_Generic) ) {
      statement << "]";
      statement.endRow();
      rows++;
   }
}
