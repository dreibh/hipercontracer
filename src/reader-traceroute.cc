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

#include "conversions.h"
#include "reader-traceroute.h"
#include "logger.h"
#include "tools.h"


const std::string  TracerouteReader::Identification("Traceroute");
const std::regex   TracerouteReader::FileNameRegExp(
   // Format: Traceroute-(Protocol-|)[P#]<ID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.(hpct|results)(<.xz|.bz2|.gz|>)
   "^Traceroute-([A-Z]+-|)([#P])([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.(hpct|results)(\\.xz|\\.bz2|\\.gz|)$"
);
const unsigned int TracerouteReader::FileNameRegExpMatchSize = 9;   // Number if groups in regexp above


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
   if(match.size() == TracerouteReader::FileNameRegExpMatchSize) {
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
TracerouteReader::TracerouteReader(const ImporterConfiguration& importerConfiguration,
                                   const unsigned int           workers,
                                   const unsigned int           maxTransactionSize,
                                   const std::string&           table)
   : ReaderImplementation<TracerouteFileEntry>(importerConfiguration, workers, maxTransactionSize),
     Table(table)
{ }


// ###### Destructor ########################################################
TracerouteReader::~TracerouteReader()
{ }


// ###### Parse measurement ID ##############################################
unsigned long long TracerouteReader::parseMeasurementID(const std::string&           value,
                                                        const std::filesystem::path& dataFile)
{
   size_t index;
   try {
      const unsigned long long measurementID = std::stoull(value, &index, 10);
      if(index == value.size()) {
         return measurementID;
      }
   }
   catch(...) { }
   throw ResultsReaderDataErrorException("Bad measurement ID value " + value +
                                         " in input file " +
                                         relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
   throw ResultsReaderDataErrorException("Bad address " + value +
                                         " in input file " +
                                         relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
}


// ###### Parse time stamp ##################################################
ReaderTimePoint TracerouteReader::parseTimeStamp(const std::string&           value,
                                                 const ReaderTimePoint&       now,
                                                 const bool                   inNanoseconds,
                                                 const std::filesystem::path& dataFile)
{
   size_t index;
   const unsigned long long ts = std::stoull(value, &index, 16);
   if(index == value.size()) {
      const ReaderTimePoint timeStamp = (inNanoseconds == true) ? nanosecondsToTimePoint<ReaderTimePoint>(ts) :
                                                                  nanosecondsToTimePoint<ReaderTimePoint>(1000ULL * ts);
      if( (timeStamp < now - std::chrono::hours(10 * 365 * 24)) ||   /* 10 years in the past */
          (timeStamp > now + std::chrono::hours(24)) ) {             /* 1 day in the future  */
         std::cerr << "timeStamp=" << timePointToString<ReaderTimePoint>(timeStamp, 9) << " now=" <<  timePointToString<ReaderTimePoint>(now, 9) << "\n";
         throw ResultsReaderDataErrorException("Invalid time stamp value (too old, or in the future) " + value +
                                               " in input file " +
                                               relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
      }
      return timeStamp;
   }
   throw ResultsReaderDataErrorException("Bad time stamp format " + value +
                                         " in input file " +
                                         relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
      throw ResultsReaderDataErrorException("Bad round number " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
      throw ResultsReaderDataErrorException("Bad traffic class format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   if(trafficClass > 0xff) {
      throw ResultsReaderDataErrorException("Invalid traffic class value " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
      throw ResultsReaderDataErrorException("Bad packet size format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
      throw ResultsReaderDataErrorException("Bad response size format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return responseSize;
}


// ###### Parse checksum ####################################################
uint16_t TracerouteReader::parseChecksum(const std::string&           value,
                                         const std::filesystem::path& dataFile)
{
   size_t        index    = 0;
   unsigned long checksum = 0;
   try {
      checksum = std::stoul(value, &index, 16);
   } catch(...) { }
   if(index != value.size()) {
      throw ResultsReaderDataErrorException("Bad checksum format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   if(checksum > 0xffff) {
      throw ResultsReaderDataErrorException("Invalid checksum value " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return (uint16_t)checksum;
}


// ###### Parse port ########################################################
uint16_t TracerouteReader::parsePort(const std::string&           value,
                                     const std::filesystem::path& dataFile)
{
   size_t        index = 0;
   unsigned long port  = 0;
   try {
      port = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ResultsReaderDataErrorException("Bad port format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   if(port > 65535) {
      throw ResultsReaderDataErrorException("Invalid port value " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return (uint16_t)port;
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
      throw ResultsReaderDataErrorException("Bad status format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
      throw ResultsReaderDataErrorException("Bad path hash " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
      throw ResultsReaderDataErrorException("Bad total hops value " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   if( (totalHops < 1) || (totalHops > 255) ) {
      throw ResultsReaderDataErrorException("Invalid total hops value " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return totalHops;
}


// ###### Parse hop number ##################################################
unsigned int TracerouteReader::parseHopNumber(const std::string&           value,
                                              const std::filesystem::path& dataFile)
{
   size_t        index     = 0;
   unsigned long hopNumber = 0;
   try {
      hopNumber = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ResultsReaderDataErrorException("Bad hopNumber value " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   if( (hopNumber < 1) || (hopNumber > 255) ) {
      throw ResultsReaderDataErrorException("Invalid hopNumber value " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return hopNumber;
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
      throw ResultsReaderDataErrorException("Bad time source format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return timeSource;
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
      throw ResultsReaderDataErrorException("Bad nanoseconds format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
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
         << " (Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,HopNumber,TotalHops,PacketSize,ResponseSize,Checksum,SourcePort,DestinationPort,Status,PathHash,SendTimestamp,HopIP,TimeSource,Delay_AppSend,Delay_Queuing,Delay_AppReceive,RTT_App,RTT_SW,RTT_HW) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      statement << "{ \"" << Table <<  "\": [";
   }
   else {
      throw ResultsLogicException("Unknown output format");
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
         throw ResultsLogicException("Unknown output format");
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
   static const unsigned int TracerouteMaxColumns = 14;
   static const char         TracerouteDelimiter  = ' ';

   unsigned int              version         = 2;
   char                      protocol        = 0x00;
   unsigned int              measurementID   = 0;
   ReaderTimePoint           timeStamp;
   boost::asio::ip::address  sourceIP;
   boost::asio::ip::address  destinationIP;
   unsigned int              roundNumber     = 0;
   uint16_t                  checksum        = 0;
   uint16_t                  sourcePort      = 0;
   uint16_t                  destinationPort = 0;
   unsigned int              totalHops       = 0;
   unsigned int              statusFlags     = ~0U;
   long long                 pathHash        = 0;
   uint8_t                   trafficClass    = 0x00;
   unsigned int              packetSize      = 0;
   unsigned long long        oldTimeStamp;   // Just used for version 1 conversion!

   std::string inputLine;
   std::string tuple[TracerouteMaxColumns];
   const ReaderTimePoint now =
      ReaderClock::now() + ReaderClockOffsetFromSystemTime;
   while(std::getline(dataStream, inputLine)) {

      // ====== Format identifier ===========================================
      if(inputLine.substr(0, 2) == "#?") {
         // Nothing to do here!
         continue;
      }

      // ====== Conversion from old versions ================================
      if(inputLine.substr(0, 3) == "#T ") {
         version = 1;
      }
      if(version < 2) {
         inputLine = convertOldTracerouteLine(inputLine, oldTimeStamp);
      }

      // ====== Parse Traceroute line =======================================
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
         throw ResultsReaderDataErrorException("Too few columns in input file " +
                                               relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
      }

      // ====== Generate import statement ===================================
      if( (tuple[0].size() >= 3) && (tuple[0][0] == '#') && (tuple[0][1] == 'T') ) {
         if( (statusFlags != ~0U) && (backend & DatabaseBackendType::NoSQL_Generic) ) {
            statement << "]";
            statement.endRow();
            rows++;
         }

         protocol        = tuple[0][2];
         measurementID   = parseMeasurementID(tuple[1], dataFile);
         sourceIP        = parseAddress(tuple[2], dataFile);
         destinationIP   = parseAddress(tuple[3], dataFile);
         timeStamp       = parseTimeStamp(tuple[4], now, true, dataFile);
         roundNumber     = parseRoundNumber(tuple[5], dataFile);
         totalHops       = parseTotalHops(tuple[6], dataFile);
         trafficClass    = parseTrafficClass(tuple[7], dataFile);
         packetSize      = parsePacketSize(tuple[8], dataFile);
         checksum        = parseChecksum(tuple[9], dataFile);
         sourcePort      = parsePort(tuple[10], dataFile);
         destinationPort = parsePort(tuple[11], dataFile);
         statusFlags     = parseStatus(tuple[12], dataFile);
         pathHash        = parsePathHash(tuple[13], dataFile);

         if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement.beginRow();
            statement
               << "\"timestamp\":"       << timePointToNanoseconds<ReaderTimePoint>(timeStamp) << statement.sep()
               << "\"measurementID\":"   << measurementID                                      << statement.sep()
               << "\"sourceIP\":"        << statement.encodeAddress(sourceIP)                  << statement.sep()
               << "\"destinationIP\":"   << statement.encodeAddress(destinationIP)             << statement.sep()
               << "\"protocol\":"        << (unsigned int)protocol                             << statement.sep()
               << "\"trafficClass\":"    << (unsigned int)trafficClass                         << statement.sep()
               << "\"roundNumber\":"     << roundNumber                                        << statement.sep()
               << "\"packetSize\":"      << packetSize                                         << statement.sep()
               << "\"checksum\":"        << checksum                                           << statement.sep()
               << "\"sourcePort\":"      << sourcePort                                         << statement.sep()
               << "\"destinationPort\":" << destinationPort                                    << statement.sep()
               << "\"statusFlags\":"     << statusFlags                                        << statement.sep()
               << "\"totalHops\":"       << totalHops                                          << statement.sep()
               << "\"pathHash\":"        << pathHash                                           << statement.sep()
               << "\"hops\": [ ";
         }
      }
      else if( (tuple[0].size() >= 1) && (tuple[0][0] == '\t') ) {
         if(statusFlags == ~0U) {
            throw ResultsReaderDataErrorException("Hop data has no corresponding #T line");
         }

         const ReaderTimePoint          sendTimeStamp   = parseTimeStamp(tuple[0], now, true, dataFile);
         const unsigned int             hopNumber       = parseHopNumber(tuple[1], dataFile);
         const unsigned int             responseSize    = parseResponseSize(tuple[2], dataFile);
         const unsigned int             status          = parseStatus(tuple[3], dataFile, 10);
         unsigned int                   timeSource      = parseTimeSource(tuple[4], dataFile);
         const long long                delayAppSend    = parseNanoseconds(tuple[5], dataFile);
         const long long                delayQueuing    = parseNanoseconds(tuple[6], dataFile);
         const long long                delayAppReceive = parseNanoseconds(tuple[7], dataFile);
         const long long                rttApp          = parseNanoseconds(tuple[8], dataFile);
         const long long                rttSoftware     = parseNanoseconds(tuple[9], dataFile);
         const long long                rttHardware     = parseNanoseconds(tuple[10], dataFile);
         const boost::asio::ip::address hopIP           = parseAddress(tuple[11], dataFile);

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << timePointToNanoseconds<ReaderTimePoint>(timeStamp)     << statement.sep()
               << measurementID                                          << statement.sep()
               << statement.encodeAddress(sourceIP)                      << statement.sep()
               << statement.encodeAddress(destinationIP)                 << statement.sep()
               << (unsigned int)protocol                                 << statement.sep()
               << (unsigned int)trafficClass                             << statement.sep()
               << roundNumber                                            << statement.sep()
               << hopNumber                                              << statement.sep()
               << totalHops                                              << statement.sep()
               << packetSize                                             << statement.sep()
               << responseSize                                           << statement.sep()
               << checksum                                               << statement.sep()
               << sourcePort                                             << statement.sep()
               << destinationPort                                        << statement.sep()
               << (status | statusFlags)                                 << statement.sep()
               << pathHash                                               << statement.sep()
               << timePointToNanoseconds<ReaderTimePoint>(sendTimeStamp) << statement.sep()
               << statement.encodeAddress(hopIP)                         << statement.sep()

               << (long long)timeSource                                  << statement.sep()
               << delayAppSend                                           << statement.sep()
               << delayQueuing                                           << statement.sep()
               << delayAppReceive                                        << statement.sep()
               << rttApp                                                 << statement.sep()
               << rttSoftware                                            << statement.sep()
               << rttHardware;
            statement.endRow();
            rows++;
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement
               << ((hopNumber > 1) ? ", { " :" { ")

               << "\"sendTimestamp\":" << timePointToNanoseconds<ReaderTimePoint>(sendTimeStamp) << statement.sep()
               << "\"responseSize\":"  << responseSize                                           << statement.sep()
               << "\"hopIP\":"         << statement.encodeAddress(hopIP)                         << statement.sep()
               << "\"status\":"        << status                                                 << statement.sep()

               << "\"timeSource\":"    << (long long)timeSource                                  << statement.sep()
               << "\"delay.appSend\":" << delayAppSend                                           << statement.sep()
               << "\"delay.queuing\":" << delayQueuing                                           << statement.sep()
               << "\"delay.appRecv\":" << delayAppReceive                                        << statement.sep()
               << "\"rtt.app\":"       << rttApp                                                 << statement.sep()
               << "\"rtt.sw\":"        << rttSoftware                                            << statement.sep()
               << "\"rtt.hw\":"        << rttHardware

               << " }";
         }
         else {
            throw ResultsLogicException("Unknown output format");
         }
      }
      else {
         throw ResultsReaderDataErrorException("Unexpected input in input file " +
                                               relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
      }
   }
   if( (statusFlags != ~0U) && (backend & DatabaseBackendType::NoSQL_Generic) ) {
      statement << "]";
      statement.endRow();
      rows++;
   }
}
