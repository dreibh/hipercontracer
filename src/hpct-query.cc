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

#include "database-configuration.h"
#include "databaseclient-base.h"
#include "logger.h"
#include "outputstream.h"
#include "package-version.h"
#include "tools.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>


typedef std::chrono::high_resolution_clock ResultClock;
typedef ResultClock::time_point            ResultTimePoint;
typedef ResultClock::duration              ResultDuration;

static const std::string                   ProgramID = std::string("HPCT-Query/") + HPCT_VERSION;


// ###### Add WHERE clause to SELECT statement ##############################
static void addSQLWhere(Statement&               statement,
                        const std::string&       timeStampField,
                        const unsigned long long fromTimeStamp,
                        const unsigned long long toTimeStamp,
                        const unsigned int       fromMeasurementID,
                        const unsigned int       toMeasurementID,
                        const bool               timestampIsTimeStampType = false)
{
   if( (fromTimeStamp > 0) || (toTimeStamp > 0) || (fromMeasurementID > 0) || (toMeasurementID > 0) ) {
      statement << " WHERE";
      bool needsAnd = false;
      if(fromTimeStamp > 0) {
         statement << " (" << timeStampField << " >= ";
         if(!timestampIsTimeStampType) {
            statement << fromTimeStamp;
         }
         else {
            statement << "'" << timePointToString<ResultTimePoint>(nanosecondsToTimePoint<ResultTimePoint>(fromTimeStamp), 9) << "'";
         }
         statement << ")";
         needsAnd = true;
      }
      if(toTimeStamp > 0) {
         statement << ((needsAnd == true) ? " AND" : "") << " (" << timeStampField << " < ";
         if(!timestampIsTimeStampType) {
            statement << toTimeStamp;
         }
         else {
            statement << "'" << timePointToString<ResultTimePoint>(nanosecondsToTimePoint<ResultTimePoint>(toTimeStamp) , 9) << "'";
         }
         statement << ")";
         needsAnd = true;
      }
      if(fromMeasurementID > 0) {
         statement << ((needsAnd == true) ? " AND" : "") << " (MeasurementID >= " << fromMeasurementID << ")";
         needsAnd = true;
      }
      if(toMeasurementID > 0) {
         statement << ((needsAnd == true) ? " AND" : "") << " (MeasurementID <= " << toMeasurementID << ")";
         // needsAnd = true;
      }
   }
}


// ###### Add filter clause to query statement ##############################
static void addNoSQLFilter(Statement&               statement,
                           const std::string&       timeStampField,
                           const unsigned long long fromTimeStamp,
                           const unsigned long long toTimeStamp,
                           const unsigned int       fromMeasurementID,
                           const unsigned int       toMeasurementID)
{
   if( (fromTimeStamp > 0) || (toTimeStamp > 0) || (fromMeasurementID > 0) || (toMeasurementID > 0) ) {
      unsigned int filters = 0;
      std::string  filterArray[4];
      if(fromTimeStamp > 0) {
         filterArray[filters++] = "\"" + timeStampField + "\": { \"$gte\": " + std::to_string(fromTimeStamp) + " }";
      }
      if(toTimeStamp > 0) {
         filterArray[filters++] = "\"" + timeStampField + "\": { \"$lt\": " + std::to_string(toTimeStamp) + " }";
      }
      if(fromMeasurementID > 0) {
         filterArray[filters++] = "\"measurementID\": { \"$gte\": " + std::to_string(fromMeasurementID) + " }";
      }
      if(toMeasurementID > 0) {
         filterArray[filters++] = "\"measurementID\": { \"$lt\": " + std::to_string(toMeasurementID) + " }";
      }
      assert(filters > 0);

      if(filters == 1) {
         statement << filterArray[0];
      }
      else {
         statement << "\"$and\": [ ";
         for(unsigned int i = 0;i < filters; i++) {
            statement << "{ " << filterArray[i]
                      << ((i < filters - 1) ? "}, " : "} ");
         }
         statement << "]";
      }
   }
}


// ###### Remap measurement ID using address ################################
static inline unsigned int mapMeasurementID(
   const unsigned long long                          measurementID,
   std::map<boost::asio::ip::address, unsigned int>& addressToMeasurementID,
   const boost::asio::ip::address&                   address)
{
   if(measurementID == 0) {
      std::map<boost::asio::ip::address, unsigned int>::iterator found = addressToMeasurementID.find(address);
      if(found != addressToMeasurementID.end()) {
         return found->second;
      }
   }
   return measurementID;
}



// ##########################################################################
// #### Macros                                                           ####
// ##########################################################################

/* NOTE:
 * The output statements are defined as macros here, since they are called
 * multiple times in time-critical loops. This ensures that the code is the
 * same in all cases.
 */


// ###### Generic ###########################################################
#define OUTPUT_ITEM(outputType, timeStampVariable)                             \
   if( (!deduplication) || (lastTimeStamp != timeStampVariable) ||             \
       (dedupLastItem != outputString) ) {                                     \
      if(__builtin_expect(lines == 0, 0)) {                                    \
         outputStream << "#? HPCT " << outputType << " " << ProgramID << "\n"; \
      }                                                                        \
      outputStream << outputString;                                            \
      if(__builtin_expect(timeStampVariable < lastTimeStamp, 0)) {             \
         HPCT_LOG(fatal) << "Sorting order violation!";                        \
         abort();                                                              \
      }                                                                        \
      lines++;                                                                 \
      lastTimeStamp = timeStampVariable;                                       \
      if(deduplication) {                                                      \
         dedupLastItem = outputString;                                         \
      }                                                                        \
      dedupInProgress = false;                                                 \
   }                                                                           \
   else {                                                                      \
      dedupDuplicatesRemoved++;                                                \
      dedupInProgress = true;                                                  \
   }


// ###### Ping ##############################################################
#define OUTPUT_PING_V2(timeStampVariable) {                                 \
   const std::string outputString = str(boost::format                       \
      ("#P%c %d %s %s %x %d %x %d %d %x %d %d %d %08x %d %d %d %d %d %d\n") \
         % protocol                            \
                                               \
         % measurementID                       \
         % sourceIP.to_string()                \
         % destinationIP.to_string()           \
         % sendTimeStamp                       \
         % burstSeq                            \
                                               \
         % (unsigned int)trafficClass          \
         % packetSize                          \
         % responseSize                        \
         % checksum                            \
         % sourcePort                          \
         % destinationPort                     \
         % status                              \
                                               \
         % timeSource                          \
         % delayAppSend                        \
         % delayQueuing                        \
         % delayAppReceive                     \
         % rttApplication                      \
         % rttSoftware                         \
         % rttHardware                         \
      );                                       \
   OUTPUT_ITEM("Ping 2", timeStampVariable);   \
}


// ###### Traceroute ########################################################
#define OUTPUT_TRACEROUTE_HEADER_V2(timeStampVariable) {   \
   const std::string outputString = str(boost::format      \
      ("#T%c %d %s %s %x %d %d %x %d %x %d %d %x %x\n")    \
         % protocol                                        \
                                                           \
         % measurementID                                   \
         % sourceIP.to_string()                            \
         % destinationIP.to_string()                       \
         % timeStamp                                       \
         % roundNumber                                     \
                                                           \
         % totalHops                                       \
                                                           \
         % (unsigned int)trafficClass                      \
         % packetSize                                      \
         % checksum                                        \
         % sourcePort                                      \
         % destinationPort                                 \
         % statusFlags                                     \
                                                           \
         % pathHash                                        \
      );                                                   \
   OUTPUT_ITEM("Traceroute 2", timeStampVariable);         \
}

#define OUTPUT_TRACEROUTE_HOP_V2()                         \
   if(!dedupInProgress) {                                  \
      outputStream <<  str(boost::format                   \
         ("\t%x %d %d %d %08x %d %d %d %d %d %d %s\n")     \
            % sendTimeStamp                                \
            % hopNumber                                    \
            % responseSize                                 \
            % (unsigned int)(status & 0xff)                \
                                                           \
            % timeSource                                   \
            % delayAppSend                                 \
            % delayQueuing                                 \
            % delayAppReceive                              \
            % rttApplication                               \
            % rttSoftware                                  \
            % rttHardware                                  \
                                                           \
            % hopIP.to_string()                            \
         );                                                \
      lines++;                                             \
   }


#if 0
// ###### Jitter ############################################################
#define OUTPUT_JITTER_V2(timeStampVariable) {              \
   const std::string outputString = str(boost::format      \
      ("#J%c %d %s %s %x %d %x %d %x %d %d %d %08x %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n") \
         % protocol                                        \
         % measurementID                                   \
         % sourceIP.to_string()                            \
         % destinationIP.to_string()                       \
         % timeStamp                                       \
         % roundNumber                                     \
         % (unsigned int)trafficClass                      \
         % packetSize                                      \
         % checksum                                        \
         % sourcePort                                      \
         % destinationPort                                 \
         % status                                          \
         % timeSource                                      \
         % jitterType                                      \
                                                           \
         % appSendPackets                                  \
         % appSendMeanLatency                              \
         % appSendJitter                                   \
                                                           \
         % queuingPackets                                  \
         % queuingMeanLatency                              \
         % queuingJitter                                   \
                                                           \
         % appReceivePackets                               \
         % appReceiveMeanLatency                           \
         % appReceiveJitter                                \
                                                           \
         % applicationPackets                              \
         % applicationMeanRTT                              \
         % applicationJitter                               \
                                                           \
         % softwarePackets                                 \
         % softwareMeanRTT                                 \
         % softwareJitter                                  \
                                                           \
         % hardwarePackets                                 \
         % hardwareMeanRTT                                 \
         % hardwareJitter                                  \
      );                                                   \
   OUTPUT_ITEM("Jitter 2", timeStampVariable);             \
}
#endif



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   unsigned int                                     logLevel;
   bool                                             logColor;
   std::filesystem::path                            logFile;
   std::filesystem::path                            databaseConfigurationFile;
   std::filesystem::path                            outputFileName;
   std::string                                      queryType;
   std::string                                      tableName;
   unsigned int                                     tableVersion;
   std::string                                      fromTimeString;
   std::string                                      toTimeString;
   std::map<boost::asio::ip::address, unsigned int> addressToMeasurementID;
   std::filesystem::path                            addressToMeasurementIDFile;
   unsigned long long                               fromTimeStamp = 0;
   unsigned long long                               toTimeStamp   = 0;
   unsigned int                                     fromMeasurementID;
   unsigned int                                     toMeasurementID;
   bool                                             deduplication = false;

   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )
      ( "version",
           "Show program version" )

      ( "loglevel,L",
           boost::program_options::value<unsigned int>(&logLevel)->default_value(boost::log::trivial::severity_level::info),
           "Set logging level" )
      ( "logfile,O",
           boost::program_options::value<std::filesystem::path>(&logFile)->default_value(std::filesystem::path()),
           "Log file" )
      ( "logcolor,Z",
           boost::program_options::value<bool>(&logColor)->default_value(true),
           "Use ANSI color escape sequences for log output" )
      ( "verbose,v",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::trace),
           "Verbose logging level" )
      ( "quiet,q",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::warning),
           "Quiet logging level" )

      ( "output,o",
           boost::program_options::value<std::filesystem::path>(&outputFileName)->default_value(std::filesystem::path()),
           "Output file" )

      ( "table,T",
           boost::program_options::value<std::string>(&tableName)->default_value(std::string()),
           "Table name to query from" )
      ( "table-version,V",
           boost::program_options::value<unsigned int>(&tableVersion)->default_value(0),
           "Table version to query from (0 for current)" )
      ( "address-to-measurementid,M",
           boost::program_options::value<std::filesystem::path>(&addressToMeasurementIDFile)->default_value(std::filesystem::path()),
           "Address to Measurement ID mapping file" )
      ( "deduplication",
           boost::program_options::value<bool>(&deduplication)->default_value(false),
           "Deduplication of output items (only needed for debugging/special cases)" )

      ( "from-time",
           boost::program_options::value<std::string>(&fromTimeString)->default_value(std::string()),
           "Query from time stamp (format: YYYY-MM-DD HH:MM:SS.NNNNNNNNN)" )
      ( "to-time",
           boost::program_options::value<std::string>(&toTimeString)->default_value(std::string()),
           "Query to time stamp (format: YYYY-MM-DD HH:MM:SS.NNNNNNNNN)" )
      ( "from-measurement-id",
           boost::program_options::value<unsigned int>(&fromMeasurementID)->default_value(0),
           "Query from measurement identifier" )
      ( "to-measurement-id",
           boost::program_options::value<unsigned int>(&toMeasurementID)->default_value(0),
           "Query to measurement identifier" )
   ;
   boost::program_options::options_description hiddenOptions;
   hiddenOptions.add_options()
      ( "config",     boost::program_options::value<std::filesystem::path>(&databaseConfigurationFile) )
      ( "query-type", boost::program_options::value<std::string>(&queryType)->default_value("Ping")    )
   ;
   boost::program_options::options_description allOptions;
   allOptions.add(commandLineOptions);
   allOptions.add(hiddenOptions);
   boost::program_options::positional_options_description positionalParameters;
   positionalParameters.add("config",     1);
   positionalParameters.add("query-type", 1);


   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
                                       style(
                                          boost::program_options::command_line_style::style_t::unix_style
                                       ).
                                       options(allOptions).positional(positionalParameters).
                                       run(), vm);
      boost::program_options::notify(vm);
   }
   catch(std::exception& e) {
      std::cerr << "Bad parameter: " << e.what() << "!\n";
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " database_configuration ping|traceroute OPTIONS" << "\n"
                 << commandLineOptions;
       return 1;
   }
   else if(vm.count("version")) {
      std::cerr << "HPCT Query" << " " << HPCT_VERSION << "\n";
      return 0;
   }
   boost::algorithm::to_lower(queryType);

   if(databaseConfigurationFile.empty()) {
      std::cerr << "ERROR: No database configuration file provided!\n";
      return 1;
   }
   if(!fromTimeString.empty()) {
      ResultTimePoint timePoint;
      if(!(stringToTimePoint<ResultTimePoint>(fromTimeString, timePoint))) {
         std::cerr << "ERROR: Bad from time stamp!\n";
         return 1;
      }
      fromTimeStamp = timePointToNanoseconds<ResultTimePoint>(timePoint);
   }
   if(!toTimeString.empty()) {
      ResultTimePoint timePoint;
      if(!(stringToTimePoint<ResultTimePoint>(toTimeString, timePoint))) {
         std::cerr << "ERROR: Bad to time stamp!\n";
         return 1;
      }
      toTimeStamp = timePointToNanoseconds<ResultTimePoint>(timePoint);
   }
   if( (fromTimeStamp > 0) && (toTimeStamp > 0) && (toTimeStamp < fromTimeStamp) ) {
      std::cerr << "ERROR: to time stamp < from time stamp!\n";
      return 1;
   }
   if( (fromMeasurementID > 0) && (toMeasurementID > 0) && (toMeasurementID < fromMeasurementID) ) {
      std::cerr << "ERROR: to measurement identifier < from measurement identifier!\n";
      return 1;
   }
   if(!addressToMeasurementIDFile.empty()) {
      std::ifstream mappingFile(addressToMeasurementIDFile);
      if(!mappingFile.good()) {
         std::cerr << "ERROR: Unable to read mapping file " << addressToMeasurementIDFile << "!\n";
         return 1;
      }
      unsigned long long line = 0;
      try {
         std::string  addressString;
         unsigned int measurementID;
         while(!mappingFile.eof()) {
            line++;
            mappingFile >> addressString >> measurementID;
            addressToMeasurementID.insert(std::pair<boost::asio::ip::address, unsigned int>(
                                             boost::asio::ip::make_address(addressString),
                                             measurementID));
         }
      }
      catch(const std::exception& e) {
         std::cerr << "ERROR: Parsing mapping file " << addressToMeasurementIDFile
                   << " failed in line " << line << ": " << e.what() << "!\n";
         return 1;
      }
   }


   // ====== Initialise importer ============================================
   initialiseLogger(logLevel, logColor,
                    (!logFile.empty()) ? logFile.string().c_str() : nullptr);

   // ====== Read database configuration ====================================
   DatabaseConfiguration databaseConfiguration;
   if(!databaseConfiguration.readConfiguration(databaseConfigurationFile)) {
      exit(1);
   }
   HPCT_LOG(info) << "Startup:\n" << databaseConfiguration;

   // ====== Initialise database client =====================================
   DatabaseClientBase* databaseClient = databaseConfiguration.createClient();
   assert(databaseClient != nullptr);
   if(!databaseClient->open()) {
      exit(1);
   }

   // ====== Open output stream =============================================
   OutputStream outputStream;
   try {
      if(!outputFileName.empty()) {
         outputStream.openStream(outputFileName);
      }
      else {
         outputStream.openStream(std::cout);
      }
   }
   catch(std::exception& e) {
      HPCT_LOG(fatal) << "Failed to create output file " << outputFileName << ": " << e.what();
      exit(1);
   }

   // ====== Prepare query ==================================================
   const DatabaseBackendType                   backend       = databaseClient->getBackend();
   Statement&                                  statement     = databaseClient->getStatement(queryType, false, true);
   const std::chrono::system_clock::time_point t1            = std::chrono::system_clock::now();
   unsigned long long                          lastTimeStamp = 0;
   unsigned long long                          lines         = 0;
   std::string                                 dedupLastItem;
   bool                                        dedupInProgress        = false;
   unsigned long long                          dedupDuplicatesRemoved = 0;
   try {
      // ====== Ping ========================================================
      if(queryType == "ping") {
         if(backend & DatabaseBackendType::SQL_Generic) {
            // ====== Old version 1 table ===================================
            if(tableVersion == 1) {
               std::string ts;
               std::string rtt;
               if( (backend & DatabaseBackendType::SQL_PostgreSQL) == DatabaseBackendType::SQL_PostgreSQL ) {
                  ts  = "CAST((1000000000.0 * EXTRACT(EPOCH FROM TimeStamp)) AS BIGINT)";
                  rtt = "1000 * CAST(RTT AS BIGINT)";
               }
               else {
                  ts  = "UNIX_TIMESTAMP(CONVERT_TZ(TimeStamp, '+00:00', @@global.time_zone)) * 1000000000";
                  rtt = "1000 * RTT";
               }
               statement
                  << "SELECT"
                     " " << ts <<  " AS SendTimestamp,"
                     " 0             AS MeasurementID,"
                     " FromIP        AS SourceIP,"
                     " ToIP          AS DestinationIP,"
                     " 105           AS Protocol,"         /* 'i', since HiPerConTracer 1.x only supports ICMP */
                     " TC            AS TrafficClass,"
                     " 0             AS BurstSeq,"
                     " PktSize       AS PacketSize,"
                     " 0             AS ResponseSize,"
                     " Checksum      AS Checksum,"
                     " 0             AS SourcePort,"
                     " 0             AS DestinationPort,"
                     " Status        AS Status,"
                     " 0             AS TimeSource,"
                     " -1            AS Delay_AppSend,"
                     " -1            AS Delay_Queuing,"
                     " -1            AS Delay_AppReceive,"
                     " " << rtt << " AS RTT_App,"
                     " -1            AS RTT_SW,"
                     " -1            AS RTT_HW "
                     "FROM " << ((tableName.size() == 0) ? "Ping" : tableName);
               addSQLWhere(statement, "TimeStamp", fromTimeStamp, toTimeStamp, fromMeasurementID, toMeasurementID, true);
            }
            // ====== Current version 2 table ============================
            else {
               statement
                  << "SELECT SendTimestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,BurstSeq,PacketSize,ResponseSize,Checksum,SourcePort,DestinationPort,Status,TimeSource,Delay_AppSend,Delay_Queuing, Delay_AppReceive,RTT_App,RTT_SW,RTT_HW "
                     "FROM " << ((tableName.size() == 0) ? "Ping" : tableName);
               addSQLWhere(statement, "SendTimestamp", fromTimeStamp, toTimeStamp, fromMeasurementID, toMeasurementID);
            }
            statement << " ORDER BY SendTimestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass";

            HPCT_LOG(debug) << "Query: " << statement;
            databaseClient->executeQuery(statement);
            try {
               while(databaseClient->fetchNextTuple()) {
                  const unsigned long long       sendTimeStamp   = databaseClient->getBigInt(1);
                  const boost::asio::ip::address sourceIP        = statement.decodeAddress(databaseClient->getString(3));
                  const boost::asio::ip::address destinationIP   = statement.decodeAddress(databaseClient->getString(4));
                  const unsigned long long       measurementID   = mapMeasurementID(databaseClient->getInteger(2),
                                                                                    addressToMeasurementID, sourceIP);
                  const char                     protocol        = databaseClient->getInteger(5);
                  const uint8_t                  trafficClass    = databaseClient->getInteger(6);
                  const unsigned int             burstSeq        = databaseClient->getInteger(7);
                  const unsigned int             packetSize      = databaseClient->getInteger(8);
                  const unsigned int             responseSize    = databaseClient->getInteger(9);
                  const uint16_t                 checksum        = databaseClient->getInteger(10);
                  const uint16_t                 sourcePort      = databaseClient->getInteger(11);
                  const uint16_t                 destinationPort = databaseClient->getInteger(12);
                  const unsigned int             status          = databaseClient->getInteger(13);
                  const unsigned int             timeSource      = databaseClient->getInteger(14);
                  const long long                delayAppSend    = databaseClient->getBigInt(15);
                  const long long                delayQueuing    = databaseClient->getBigInt(16);
                  const long long                delayAppReceive = databaseClient->getBigInt(17);
                  const long long                rttApplication  = databaseClient->getBigInt(18);
                  const long long                rttSoftware     = databaseClient->getBigInt(19);
                  const long long                rttHardware     = databaseClient->getBigInt(20);
                  OUTPUT_PING_V2(sendTimeStamp);
               }
            }
            catch(const std::exception& e) {
               HPCT_LOG(fatal) << "Bad data: " << e.what();
               exit(1);
            }
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            // ====== Old version 1 table ===================================
            if(tableVersion == 1) {
               statement << "[ { \"" <<  ((tableName.size() == 0 ? "ping" : tableName)) << "\": { ";
               addNoSQLFilter(statement, "timestamp", fromTimeStamp / 1000ULL, toTimeStamp / 1000ULL,
                              fromMeasurementID, toMeasurementID);
               statement << " } }, { \"sort\": { \"timestamp\": 1, \"source\": 1, \"destination\": 1, \"tc\": 1 } } ]";
            }
            // ====== Current version 2 table ===============================
            else {
               statement << "[ { \"" <<  ((tableName.size() == 0 ? "ping" : tableName)) << "\": { ";
               addNoSQLFilter(statement, "sendTimestamp", fromTimeStamp, toTimeStamp,
                              fromMeasurementID, toMeasurementID);
               statement << " } }, { \"sort\": { \"sendTimestamp\": 1, \"measurementID\": 1, \"sourceIP\": 1, \"destinationIP\": 1, \"protocol\": 1, \"trafficClass\": 1, \"burstSeq\": 1 } } ]";
            }

            HPCT_LOG(debug) << "Query: " << statement;
            databaseClient->executeQuery(statement);
            try {
               // ====== Old version 1 table ================================
               if(tableVersion == 1) {
                  while(databaseClient->fetchNextTuple()) {
                     const unsigned long long       sendTimeStamp   = 1000 * databaseClient->getBigInt("timestamp");
                     const boost::asio::ip::address sourceIP        = statement.decodeAddress(databaseClient->getString("source"));
                     const boost::asio::ip::address destinationIP   = statement.decodeAddress(databaseClient->getString("destination"));
                     const unsigned long long       measurementID   = mapMeasurementID(0, addressToMeasurementID, sourceIP);
                     const char                     protocol        = 'i';
                     const uint8_t                  trafficClass    = (databaseClient->hasColumn("trafficClass") == true) ?
                                                                         databaseClient->getInteger("trafficClass") : 0x00;
                     const unsigned int             burstSeq        = 0;
                     const unsigned int             packetSize      = (databaseClient->hasColumn("packetSize") == true) ?
                                                                         databaseClient->getInteger("packetSize") : 0;
                     const unsigned int             responseSize    = 0;
                     const uint16_t                 checksum        = databaseClient->getInteger("checksum");
                     const uint16_t                 sourcePort      = 0;
                     const uint16_t                 destinationPort = 0;
                     const unsigned int             status          = databaseClient->getInteger("status");
                     const unsigned int             timeSource      = 0x00000000;
                     const long long                delayAppSend    = -1;
                     const long long                delayQueuing    = -1;
                     const long long                delayAppReceive = -1;
                     const long long                rttApplication  = 1000 * databaseClient->getBigInt("rtt");
                     const long long                rttSoftware     = 0;
                     const long long                rttHardware     = 0;
                     OUTPUT_PING_V2(sendTimeStamp);
                  }
               }
               // ====== Current version 2 table ============================
               else {
                  while(databaseClient->fetchNextTuple()) {
                     const unsigned long long       sendTimeStamp   = databaseClient->getBigInt("sendTimestamp");
                     const boost::asio::ip::address sourceIP        = statement.decodeAddress(databaseClient->getString("sourceIP"));
                     const boost::asio::ip::address destinationIP   = statement.decodeAddress(databaseClient->getString("destinationIP"));
                     const unsigned long long       measurementID   = mapMeasurementID(databaseClient->getInteger("measurementID"),
                                                                                       addressToMeasurementID, sourceIP);
                     const char                     protocol        = databaseClient->getInteger("protocol");
                     const uint8_t                  trafficClass    = databaseClient->getInteger("trafficClass");
                     const unsigned int             burstSeq        = databaseClient->getInteger("burstSeq");
                     const unsigned int             packetSize      = databaseClient->getInteger("packetSize");
                     const unsigned int             responseSize    = databaseClient->getInteger("responseSize");
                     const uint16_t                 checksum        = databaseClient->getInteger("checksum");
                     const uint16_t                 sourcePort      = databaseClient->getInteger("sourcePort");
                     const uint16_t                 destinationPort = databaseClient->getInteger("destinationPort");
                     const unsigned int             status          = databaseClient->getInteger("status");
                     const unsigned int             timeSource      = databaseClient->getInteger("timeSource");
                     const long long                delayAppSend    = databaseClient->getBigInt("delay.appSend");
                     const long long                delayQueuing    = databaseClient->getBigInt("delay.queuing");
                     const long long                delayAppReceive = databaseClient->getBigInt("delay.appRecv");
                     const long long                rttApplication  = databaseClient->getBigInt("rtt.app");
                     const long long                rttSoftware     = databaseClient->getBigInt("rtt.sw");
                     const long long                rttHardware     = databaseClient->getBigInt("rtt.hw");
                     OUTPUT_PING_V2(sendTimeStamp);
                  }
               }
            }
            catch(const std::exception& e) {
               HPCT_LOG(fatal) << "Bad data: " << e.what();
               exit(1);
            }
         }
         else {
            HPCT_LOG(fatal) << "Unknown backend";
            abort();
         }
      }

      // ====== Traceroute ==================================================
      else if(queryType == "traceroute") {
         if(backend & DatabaseBackendType::SQL_Generic) {
            // ====== Old version 1 table ===================================
            if(tableVersion == 1) {
               std::string ts;
               std::string rtt;
               if( (backend & DatabaseBackendType::SQL_PostgreSQL) == DatabaseBackendType::SQL_PostgreSQL ) {
                  ts  = "CAST((1000000000.0 * EXTRACT(EPOCH FROM TimeStamp)) AS BIGINT)";
                  rtt = "1000 * CAST(RTT AS BIGINT)";
               }
               else {
                  ts  = "UNIX_TIMESTAMP(CONVERT_TZ(TimeStamp, '+00:00', @@global.time_zone)) * 1000000000";
                  rtt = "1000 * RTT";
               }
               statement
                  << "SELECT"
                     " " << ts << "  AS Timestamp,"
                     " 0             AS MeasurementID,"
                     " FromIP        AS SourceIP,"
                     " ToIP          AS DestinationIP,"
                     " 105           AS Protocol,"     /* 'i', since HiPerConTracer 1.x only supports ICMP */
                     " TC            AS TrafficClass,"
                     " Round         AS RoundNumber,"
                     " HopNumber     AS HopNumber,"
                     " TotalHops     AS TotalHops,"
                     " PktSize       AS PacketSize,"
                     " 0             AS ResponseSize,"
                     " Checksum      AS Checksum,"
                     " 0             AS SourcePort,"
                     " 0             AS DestinationPort,"
                     " Status        AS Status,"
                     " PathHash      AS PathHash,"
                     " " << ts << "  AS SendTimestamp,"
                     " HopIP         AS HopIP,"
                     " 0             AS TimeSource,"
                     " -1            AS Delay_AppSend,"
                     " -1            AS Delay_Queuing,"
                     " -1            AS Delay_AppReceive,"
                     " " << rtt << " AS RTT_App,"
                     " -1            AS RTT_SW,"
                     " -1            AS RTT_HW "
                     "FROM " << ((tableName.size() == 0) ? "Traceroute" : tableName);
               addSQLWhere(statement, "TimeStamp", fromTimeStamp, toTimeStamp, fromMeasurementID, toMeasurementID, true);
            }
            // ====== Current version 2 table ============================
            else {
               statement
                  << "SELECT Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,HopNumber,TotalHops,PacketSize,ResponseSize,Checksum,SourcePort,DestinationPort,Status,PathHash,SendTimestamp,HopIP,TimeSource,Delay_AppSend,Delay_Queuing,Delay_AppReceive,RTT_App,RTT_SW,RTT_HW"
                     " FROM " << ((tableName.size() == 0) ? "Traceroute" : tableName);
               addSQLWhere(statement, "Timestamp", fromTimeStamp, toTimeStamp, fromMeasurementID, toMeasurementID);
            }
            statement << " ORDER BY Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,HopNumber";

            HPCT_LOG(debug) << "Query: " << statement;
            databaseClient->executeQuery(statement);
            try {
               while(databaseClient->fetchNextTuple()) {
                  const unsigned long long       timeStamp       = databaseClient->getBigInt(1);
                  const boost::asio::ip::address sourceIP        = statement.decodeAddress(databaseClient->getString(3));
                  const boost::asio::ip::address destinationIP   = statement.decodeAddress(databaseClient->getString(4));
                  const unsigned long long       measurementID   = mapMeasurementID(databaseClient->getBigInt(2),
                                                                                    addressToMeasurementID, sourceIP);
                  const char                     protocol        = databaseClient->getInteger(5);
                  const uint8_t                  trafficClass    = databaseClient->getInteger(6);
                  const unsigned int             roundNumber     = databaseClient->getInteger(7);
                  const unsigned int             hopNumber       = databaseClient->getInteger(8);
                  const unsigned int             totalHops       = databaseClient->getInteger(9);
                  const unsigned int             packetSize      = databaseClient->getInteger(10);
                  const unsigned int             responseSize    = databaseClient->getInteger(11);
                  const uint16_t                 checksum        = databaseClient->getInteger(12);
                  const uint16_t                 sourcePort      = databaseClient->getInteger(13);
                  const uint16_t                 destinationPort = databaseClient->getInteger(14);
                  const unsigned int             status          = databaseClient->getInteger(15);
                  const long long                pathHash        = databaseClient->getBigInt(16);
                  const unsigned long long       sendTimeStamp   = databaseClient->getBigInt(17);
                  const boost::asio::ip::address hopIP           = statement.decodeAddress(databaseClient->getString(18));
                  const unsigned int             timeSource      = databaseClient->getInteger(19);
                  const long long                delayAppSend    = databaseClient->getBigInt(20);
                  const long long                delayQueuing    = databaseClient->getBigInt(21);
                  const long long                delayAppReceive = databaseClient->getBigInt(22);
                  const long long                rttApplication  = databaseClient->getBigInt(23);
                  const long long                rttSoftware     = databaseClient->getBigInt(24);
                  const long long                rttHardware     = databaseClient->getBigInt(25);

                  if(hopNumber == 1) {
                     const unsigned int statusFlags = status - (status & 0xff);
                     OUTPUT_TRACEROUTE_HEADER_V2(timeStamp);
                  }
                  OUTPUT_TRACEROUTE_HOP_V2();
               }
            }
            catch(const std::exception& e) {
               HPCT_LOG(fatal) << "Bad data: " << e.what();
               exit(1);
            }
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            // ====== Old version 1 table ===================================
            if(tableVersion == 1) {
               statement << "[ { \"" <<  ((tableName.size() == 0 ? "traceroute" : tableName)) << "\": { ";
               addNoSQLFilter(statement, "timestamp", fromTimeStamp/ 1000ULL, toTimeStamp/ 1000ULL,
                              fromMeasurementID, toMeasurementID);
               statement << " } }, { \"sort\": { \"timestamp\": 1, \"source\": 1, \"destination\": 1, \"tc\": 1, \"round\": 1 } } ]";
            }
            // ====== Current version 2 table ===============================
            else {
               statement << "[ { \"" <<  ((tableName.size() == 0 ? "traceroute" : tableName)) << "\": { ";
               addNoSQLFilter(statement, "timestamp", fromTimeStamp, toTimeStamp,
                              fromMeasurementID, toMeasurementID);
               statement << " } }, { \"sort\": { \"timestamp\": 1, \"measurementID\": 1, \"sourceIP\": 1, \"destinationIP\": 1, \"protocol\": 1, \"trafficClass\": 1, \"roundNumber\": 1 } } ]";
            }

            HPCT_LOG(debug) << "Query: " << statement;
            databaseClient->executeQuery(statement);
            try {
               // ====== Old version 1 table ================================
               if(tableVersion == 1) {
                  while(databaseClient->fetchNextTuple()) {
                     const unsigned long long       timeStamp       = 1000 * databaseClient->getBigInt("timestamp");
                     const boost::asio::ip::address sourceIP        = statement.decodeAddress(databaseClient->getString("source"));
                     const boost::asio::ip::address destinationIP   = statement.decodeAddress(databaseClient->getString("destination"));
                     const unsigned long long       measurementID   = mapMeasurementID(0, addressToMeasurementID, sourceIP);
                     const char                     protocol        = 'i';
                     const uint8_t                  trafficClass    = (databaseClient->hasColumn("trafficClass") == true) ?
                                                                         databaseClient->getInteger("trafficClass") : 0x00;
                     const unsigned int             roundNumber     = databaseClient->getInteger("round");
                     const unsigned int             totalHops       = databaseClient->getInteger("totalHops");
                     const unsigned int             packetSize      = (databaseClient->hasColumn("packetSize") == true) ?
                                                                         databaseClient->getInteger("packetSize") : 0;
                     const uint16_t                 checksum        = databaseClient->getInteger("checksum");
                     const uint16_t                 sourcePort      = 0;
                     const uint16_t                 destinationPort = 0;
                     const unsigned int             statusFlags     = databaseClient->getInteger("statusFlags");
                     const long long                pathHash        = databaseClient->getBigInt("pathHash");
                     OUTPUT_TRACEROUTE_HEADER_V2(timeStamp);

                     databaseClient->getArrayBegin("hops");
                     unsigned int hopNumber = 0;
                     while(databaseClient->fetchNextArrayTuple()) {
                        hopNumber++;

                        const unsigned long long       sendTimeStamp   = timeStamp;
                        const unsigned int             responseSize    = 0;
                        const boost::asio::ip::address hopIP           = statement.decodeAddress(databaseClient->getString("hop"));
                        const unsigned int             status          = databaseClient->getInteger("status");
                        const unsigned int             timeSource      = 0x00000000;
                        const long long                delayAppSend    = -1;
                        const long long                delayQueuing    = -1;
                        const long long                delayAppReceive = -1;
                        const long long                rttApplication  = 1000 * databaseClient->getBigInt("rtt");
                        const long long                rttSoftware     = -1;
                        const long long                rttHardware     = -1;
                        OUTPUT_TRACEROUTE_HOP_V2();
                     }
                     databaseClient->getArrayEnd();
                  }
               }

               else {
                  // ====== Current version 2 table =========================
                  while(databaseClient->fetchNextTuple()) {
                     const unsigned long long       timeStamp       = databaseClient->getBigInt("timestamp");
                     const boost::asio::ip::address sourceIP        = statement.decodeAddress(databaseClient->getString("sourceIP"));
                     const boost::asio::ip::address destinationIP   = statement.decodeAddress(databaseClient->getString("destinationIP"));
                     const unsigned long long       measurementID   = mapMeasurementID(databaseClient->getBigInt("measurementID"),
                                                                                       addressToMeasurementID, sourceIP);
                     const char                     protocol        = databaseClient->getInteger("protocol");
                     const uint8_t                  trafficClass    = databaseClient->getInteger("trafficClass");
                     const unsigned int             roundNumber     = databaseClient->getInteger("roundNumber");
                     const unsigned int             totalHops       = databaseClient->getInteger("totalHops");
                     const unsigned int             packetSize      = databaseClient->getInteger("packetSize");
                     const uint16_t                 checksum        = databaseClient->getInteger("checksum");
                     const uint16_t                 sourcePort      = databaseClient->getInteger("sourcePort");
                     const uint16_t                 destinationPort = databaseClient->getInteger("destinationPort");
                     const unsigned int             statusFlags     = databaseClient->getInteger("statusFlags");
                     const long long                pathHash        = databaseClient->getBigInt("pathHash");
                     OUTPUT_TRACEROUTE_HEADER_V2(timeStamp);

                     databaseClient->getArrayBegin("hops");
                     unsigned int hopNumber = 0;
                     while(databaseClient->fetchNextArrayTuple()) {
                        hopNumber++;

                        const unsigned long long       sendTimeStamp   = databaseClient->getBigInt("sendTimestamp");
                        const unsigned int             responseSize    = databaseClient->getInteger("responseSize");
                        const boost::asio::ip::address hopIP           = statement.decodeAddress(databaseClient->getString("hopIP"));
                        const unsigned int             status          = databaseClient->getInteger("status");
                        const unsigned int             timeSource      = databaseClient->getInteger("timeSource");
                        const long long                delayAppSend    = databaseClient->getBigInt("delay.appSend");
                        const long long                delayQueuing    = databaseClient->getBigInt("delay.queuing");
                        const long long                delayAppReceive = databaseClient->getBigInt("delay.appRecv");
                        const long long                rttApplication  = databaseClient->getBigInt("rtt.app");
                        const long long                rttSoftware     = databaseClient->getBigInt("rtt.sw");
                        const long long                rttHardware     = databaseClient->getBigInt("rtt.hw");
                        OUTPUT_TRACEROUTE_HOP_V2();
                     }
                     databaseClient->getArrayEnd();
                  }
               }
            }
            catch(const std::exception& e) {
               HPCT_LOG(fatal) << "Bad data: " << e.what();
               exit(1);
            }
         }
         else {
            HPCT_LOG(fatal) << "Unknown backend";
            abort();
         }
      }

#if 0
      // ====== Jitter ======================================================
      else if(queryType == "jitter") {
         if(backend & DatabaseBackendType::SQL_Generic) {
            statement
               << "SELECT Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,PacketSize,Checksum,SourcePort,DestinationPort,Status,JitterType,TimeSource,Packets_AppSend,MeanDelay_AppSend,Jitter_AppSend,Packets_Queuing,MeanDelay_Queuing,Jitter_Queuing,Packets_AppReceive,MeanDelay_AppReceive,Jitter_AppReceive,Packets_App,MeanRTT_App,Jitter_App,Packets_SW,MeanRTT_SW,Jitter_SW,Packets_HW,MeanRTT_HW,Jitter_HW"
                  " FROM " << ((tableName.size() == 0) ? "Jitter" : tableName);
            addSQLWhere(statement, "Timestamp", fromTimeStamp, toTimeStamp, fromMeasurementID, toMeasurementID);
            statement << " ORDER BY Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber";

            HPCT_LOG(debug) << "Query: " << statement;
            databaseClient->executeQuery(statement);
            while(databaseClient->fetchNextTuple()) {
               const unsigned long long       timeStamp             = databaseClient->getBigInt(1);
               const boost::asio::ip::address sourceIP              = statement.decodeAddress(databaseClient->getString(3));
               const boost::asio::ip::address destinationIP         = statement.decodeAddress(databaseClient->getString(4));
               const unsigned long long       measurementID         = mapMeasurementID(databaseClient->getInteger(2),
                                                                                       addressToMeasurementID, sourceIP);
               const char                     protocol              = databaseClient->getInteger(5);
               const uint8_t                  trafficClass          = databaseClient->getInteger(6);
               const unsigned int             roundNumber           = databaseClient->getInteger(7);
               const unsigned int             packetSize            = databaseClient->getInteger(8);
               const uint16_t                 checksum              = databaseClient->getInteger(9);
               const uint16_t                 sourcePort            = databaseClient->getInteger(10);
               const uint16_t                 destinationPort       = databaseClient->getInteger(11);
               const unsigned int             status                = databaseClient->getInteger(12);
               const unsigned int             jitterType            = databaseClient->getInteger(13);
               const unsigned int             timeSource            = databaseClient->getInteger(14);

               const unsigned long long       appSendPackets        = databaseClient->getInteger(15);
               const unsigned long long       appSendMeanLatency    = databaseClient->getBigInt(16);
               const unsigned long long       appSendJitter         = databaseClient->getBigInt(17);

               const unsigned long long       queuingPackets        = databaseClient->getInteger(18);
               const unsigned long long       queuingMeanLatency    = databaseClient->getBigInt(19);
               const unsigned long long       queuingJitter         = databaseClient->getBigInt(20);

               const unsigned long long       appReceivePackets     = databaseClient->getInteger(21);
               const unsigned long long       appReceiveMeanLatency = databaseClient->getBigInt(22);
               const unsigned long long       appReceiveJitter      = databaseClient->getBigInt(23);

               const unsigned long long       applicationPackets    = databaseClient->getInteger(24);
               const unsigned long long       applicationMeanRTT    = databaseClient->getBigInt(25);
               const unsigned long long       applicationJitter     = databaseClient->getBigInt(26);

               const unsigned long long       softwarePackets       = databaseClient->getInteger(27);
               const unsigned long long       softwareMeanRTT       = databaseClient->getBigInt(28);
               const unsigned long long       softwareJitter        = databaseClient->getBigInt(29);

               const unsigned long long       hardwarePackets       = databaseClient->getInteger(30);
               const unsigned long long       hardwareMeanRTT       = databaseClient->getBigInt(31);
               const unsigned long long       hardwareJitter        = databaseClient->getBigInt(32);

               OUTPUT_JITTER_V2(timeStamp);
            }
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement << "[ { \"" <<  ((tableName.size() == 0 ? "jitter" : tableName)) << "\": { ";
            addNoSQLFilter(statement, "timestamp", fromTimeStamp, toTimeStamp, fromMeasurementID, toMeasurementID);
            statement << " } }, { \"sort\": { \"timestamp\": 1, \"measurementID\": 1, \"sourceIP\": 1, \"destinationIP\": 1, \"protocol\": 1, \"trafficClass\": 1, \"roundNumber\": 1 } } ]";

            HPCT_LOG(debug) << "Query: " << statement;
            databaseClient->executeQuery(statement);
            try {
               // ====== Current version 2 table ============================
               while(databaseClient->fetchNextTuple()) {
                  const unsigned long long       timeStamp             = databaseClient->getBigInt("timestamp");
                  const boost::asio::ip::address sourceIP              = statement.decodeAddress(databaseClient->getString("sourceIP"));
                  const boost::asio::ip::address destinationIP         = statement.decodeAddress(databaseClient->getString("destinationIP"));
                  const unsigned long long       measurementID         = mapMeasurementID(databaseClient->getInteger("measurementID"),
                                                                                          addressToMeasurementID, sourceIP);
                  const char                     protocol              = databaseClient->getInteger("protocol");
                  const uint8_t                  trafficClass          = databaseClient->getInteger("trafficClass");
                  const unsigned int             roundNumber           = databaseClient->getInteger("roundNumber");
                  const unsigned int             packetSize            = databaseClient->getInteger("packetSize");
                  const uint16_t                 checksum              = databaseClient->getInteger("checksum");
                  const uint16_t                 sourcePort            = databaseClient->getInteger("sourcePort");
                  const uint16_t                 destinationPort       = databaseClient->getInteger("destinationPort");
                  const unsigned int             status                = databaseClient->getInteger("status");
                  const unsigned int             jitterType            = databaseClient->getInteger("jitterType");
                  const unsigned int             timeSource            = databaseClient->getInteger("timeSource");

                  const unsigned long long       appSendPackets        = databaseClient->getInteger("appSendPackets");
                  const unsigned long long       appSendMeanLatency    = databaseClient->getBigInt("appSendMeanLatency");
                  const unsigned long long       appSendJitter         = databaseClient->getBigInt("appSendJitter");

                  const unsigned long long       queuingPackets        = databaseClient->getInteger("queuingPackets");
                  const unsigned long long       queuingMeanLatency    = databaseClient->getBigInt("queuingMeanLatency");
                  const unsigned long long       queuingJitter         = databaseClient->getBigInt("queuingJitter");

                  const unsigned long long       appReceivePackets     = databaseClient->getInteger("appReceivePackets");
                  const unsigned long long       appReceiveMeanLatency = databaseClient->getBigInt("appReceiveMeanLatency");
                  const unsigned long long       appReceiveJitter      = databaseClient->getBigInt("appReceiveJitter");

                  const unsigned long long       applicationPackets    = databaseClient->getInteger("applicationPackets");
                  const unsigned long long       applicationMeanRTT    = databaseClient->getBigInt("applicationMeanRTT");
                  const unsigned long long       applicationJitter     = databaseClient->getBigInt("applicationJitter");

                  const unsigned long long       softwarePackets       = databaseClient->getInteger("softwarePackets");
                  const unsigned long long       softwareMeanRTT       = databaseClient->getBigInt("softwareMeanRTT");
                  const unsigned long long       softwareJitter        = databaseClient->getBigInt("softwareJitter");

                  const unsigned long long       hardwarePackets       = databaseClient->getInteger("hardwarePackets");
                  const unsigned long long       hardwareMeanRTT       = databaseClient->getBigInt("hardwareMeanRTT");
                  const unsigned long long       hardwareJitter        = databaseClient->getBigInt("hardwareJitter");

                  OUTPUT_JITTER_V2(timeStamp);
               }
            }
            catch(const std::exception& e) {
               HPCT_LOG(fatal) << "Bad data: " << e.what();
               exit(1);
            }
         }
         else {
            HPCT_LOG(fatal) << "Unknown backend";
            abort();
         }
      }
#endif

      // ====== Invalid query ===============================================
      else {
         HPCT_LOG(fatal) << "Invalid query type " << queryType;
         exit(1);
      }
   }
   catch(const std::exception& e) {
      HPCT_LOG(fatal) << "Query failed: " << e.what();
      exit(1);
   }

   // ====== Close output file ==============================================
   try {
      outputStream.closeStream();
      if(!outputFileName.empty()) {
         // Set timestamp to the latest timestamp in the data. Note: the timestamp is UTC!
         const std::time_t t = (std::time_t)(lastTimeStamp / 1000000000);
         boost::filesystem::last_write_time(boost::filesystem::path(outputFileName), t);
      }
   }
   catch(const std::exception& e) {
      HPCT_LOG(fatal) << "Failed to close output file "
                      << outputFileName << ": " << e.what();
      exit(1);
   }

   // ====== Print statistics ===============================================
   const std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();
   HPCT_LOG(info) << "Wrote " << lines << " results lines in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms";
   if( (deduplication) && (dedupDuplicatesRemoved > 0) ) {
      HPCT_LOG(warning) << "Found and removed " << dedupDuplicatesRemoved << " duplicates from output!";
   }

   // ====== Clean up =======================================================
   delete databaseClient;
   databaseClient = nullptr;
   return 0;
}
