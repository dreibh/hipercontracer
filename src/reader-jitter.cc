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

#include "reader-jitter.h"
#include "logger.h"
#include "tools.h"

#include <boost/asio.hpp>


const std::string JitterReader::Identification("Jitter");
const std::regex  JitterReader::FileNameRegExp(
   // Format: Jitter-(Protocol-|)[P#]<ID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.(hpct|results)(<.xz|.bz2|.gz|>)
   "^Jitter-([A-Z]+-|)([#P])([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.(hpct|results)(\\.xz|\\.bz2|\\.gz|)$"
);


// ###### Constructor #######################################################
JitterReader::JitterReader(const ImporterConfiguration& importerConfiguration,
                           const unsigned int           workers,
                           const unsigned int           maxTransactionSize,
                           const std::string&           table)
   : PingReader(importerConfiguration, workers, maxTransactionSize, table)
{
}


// ###### Destructor ########################################################
JitterReader::~JitterReader()
{
}


// ###### Begin parsing #####################################################
void JitterReader::beginParsing(DatabaseClientBase& databaseClient,
                                unsigned long long& rows)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement              = databaseClient.getStatement("Jitter", false, true);

   rows = 0;

   // ====== Generate import statement ======================================
   if(backend & DatabaseBackendType::SQL_Generic) {
      statement
         << "INSERT INTO " << Table
         << " (Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,PacketSize,Checksum,SourcePort,DestinationPort,Status,JitterType,TimeSource,Packets_AppSend,MeanDelay_AppSend,Jitter_AppSend,Packets_Queuing,MeanDelay_Queuing,Jitter_Queuing,Packets_AppReceive,MeanDelay_AppReceive,Jitter_AppReceive,Packets_App,MeanRTT_App,Jitter_App,Packets_SW,MeanRTT_SW,Jitter_SW,Packets_HW,MeanRTT_HW,Jitter_HW) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      statement << "{ \"" << Table <<  "\": [";
   }
   else {
      throw ResultsLogicException("Unknown output format");
   }
}


// ###### Parse jitter type #################################################
unsigned int JitterReader::parseJitterType(const std::string&           value,
                                           const std::filesystem::path& dataFile)
{
   size_t       index      = 0;
   unsigned int jitterType = 0;
   try {
      jitterType = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ResultsReaderDataErrorException("Bad jitter type format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return jitterType;
}


// ###### Parse packets  ####################################################
unsigned int JitterReader::parsePackets(const std::string&           value,
                                        const std::filesystem::path& dataFile)
{
   size_t       index   = 0;
   unsigned int packets = 0;
   try {
      packets = std::stoul(value, &index, 10);
   } catch(...) { }
   if(index != value.size()) {
      throw ResultsReaderDataErrorException("Bad packets format " + value +
                                            " in input file " +
                                            relativeTo(dataFile, ImporterConfig.getImportFilePath()).string());
   }
   return packets;
}


// ###### Finish parsing ####################################################
bool JitterReader::finishParsing(DatabaseClientBase& databaseClient,
                                 unsigned long long& rows)
{
   const DatabaseBackendType backend   = databaseClient.getBackend();
   Statement&                statement = databaseClient.getStatement("Jitter");
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
void JitterReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        const std::filesystem::path&         dataFile,
        boost::iostreams::filtering_istream& dataStream)
{
   Statement&                statement = databaseClient.getStatement("Jitter");
   const DatabaseBackendType backend   = databaseClient.getBackend();
   static const unsigned int JitterMinColumns = 32;
   static const unsigned int JitterMaxColumns = 32;
   static const char         JitterDelimiter  = ' ';

   std::string inputLine;
   std::string tuple[JitterMaxColumns];
   const ReaderTimePoint now =
      ReaderClock::now() + ReaderClockOffsetFromSystemTime;
   while(std::getline(dataStream, inputLine)) {

      // ====== Format identifier ===========================================
      if(inputLine.substr(0, 2) == "#?") {
         // Nothing to do here!
         continue;
      }

      // ====== Parse Jitter line ===========================================
      size_t columns = 0;
      size_t start;
      size_t end = 0;
      while((start = inputLine.find_first_not_of(JitterDelimiter, end)) != std::string::npos) {
         end = inputLine.find(JitterDelimiter, start);
         if(columns == JitterMaxColumns) {
            // Skip additional columns
            break;
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns < JitterMinColumns) {
         throw ResultsReaderDataErrorException("Too few columns in input file " + dataFile.string());
      }

      // ====== Generate import statement ===================================
      if( (tuple[0].size() >= 3) && (tuple[0][0] == '#') && (tuple[0][1] == 'J') ) {
         // ====== Generate import statement ================================
         const char                     protocol              = tuple[0][2];
         const unsigned int             measurementID         = parseMeasurementID(tuple[1], dataFile);
         const boost::asio::ip::address sourceIP              = parseAddress(tuple[2], dataFile);
         const boost::asio::ip::address destinationIP         = parseAddress(tuple[3], dataFile);
         const ReaderTimePoint          timeStamp             = parseTimeStamp(tuple[4], now, true, dataFile);
         const unsigned int             roundNumber           = parseRoundNumber(tuple[5], dataFile);
         const uint8_t                  trafficClass          = parseTrafficClass(tuple[6], dataFile);
         const unsigned int             packetSize            = parsePacketSize(tuple[7], dataFile);

         const uint16_t                 checksum              = parseChecksum(tuple[8], dataFile);
         const uint16_t                 sourcePort            = parsePort(tuple[9], dataFile);
         const uint16_t                 destinationPort       = parsePort(tuple[10], dataFile);
         const unsigned int             status                = parseStatus(tuple[11], dataFile, 10);
         const unsigned int             timeSource            = parseTimeSource(tuple[12], dataFile);
         const unsigned int             jitterType            = parseJitterType(tuple[13], dataFile);

         const unsigned int             appSendPackets        = parsePackets(tuple[14], dataFile);
         const unsigned long long       appSendMeanLatency    = parseNanoseconds(tuple[15], dataFile);
         const unsigned long long       appSendJitter         = parseNanoseconds(tuple[16], dataFile);

         const unsigned int             queuingPackets        = parsePackets(tuple[17], dataFile);
         const unsigned long long       queuingMeanLatency    = parseNanoseconds(tuple[18], dataFile);
         const unsigned long long       queuingJitter         = parseNanoseconds(tuple[19], dataFile);

         const unsigned int             appReceivePackets     = parsePackets(tuple[20], dataFile);
         const unsigned long long       appReceiveMeanLatency = parseNanoseconds(tuple[21], dataFile);
         const unsigned long long       appReceiveJitter      = parseNanoseconds(tuple[22], dataFile);

         const unsigned int             applicationPackets    = parsePackets(tuple[23], dataFile);
         const unsigned long long       applicationMeanRTT    = parseNanoseconds(tuple[24], dataFile);
         const unsigned long long       applicationJitter     = parseNanoseconds(tuple[25], dataFile);

         const unsigned int             softwarePackets       = parsePackets(tuple[26], dataFile);
         const unsigned long long       softwareMeanRTT       = parseNanoseconds(tuple[27], dataFile);
         const unsigned long long       softwareJitter        = parseNanoseconds(tuple[28], dataFile);

         const unsigned int             hardwarePackets       = parsePackets(tuple[29], dataFile);
         const unsigned long long       hardwareMeanRTT       = parseNanoseconds(tuple[30], dataFile);
         const unsigned long long       hardwareJitter        = parseNanoseconds(tuple[31], dataFile);

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << timePointToNanoseconds<ReaderTimePoint>(timeStamp) << statement.sep()
               << measurementID                                      << statement.sep()
               << statement.encodeAddress(sourceIP)                  << statement.sep()
               << statement.encodeAddress(destinationIP)             << statement.sep()
               << (unsigned int)protocol                             << statement.sep()
               << (unsigned int)trafficClass                         << statement.sep()
               << roundNumber                                        << statement.sep()
               << packetSize                                         << statement.sep()
               << checksum                                           << statement.sep()
               << sourcePort                                         << statement.sep()
               << destinationPort                                    << statement.sep()
               << status                                             << statement.sep()
               << jitterType                                         << statement.sep()
               << (long long)timeSource                              << statement.sep()

               << appSendPackets                                     << statement.sep()
               << appSendMeanLatency                                 << statement.sep()
               << appSendJitter                                      << statement.sep()

               << queuingPackets                                     << statement.sep()
               << queuingMeanLatency                                 << statement.sep()
               << queuingJitter                                      << statement.sep()

               << appReceivePackets                                  << statement.sep()
               << appReceiveMeanLatency                              << statement.sep()
               << appReceiveJitter                                   << statement.sep()

               << applicationPackets                                 << statement.sep()
               << applicationMeanRTT                                 << statement.sep()
               << applicationJitter                                  << statement.sep()

               << softwarePackets                                    << statement.sep()
               << softwareMeanRTT                                    << statement.sep()
               << softwareJitter                                     << statement.sep()

               << hardwarePackets                                    << statement.sep()
               << hardwareMeanRTT                                    << statement.sep()
               << hardwareJitter;

            statement.endRow();
            rows++;
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement.beginRow();
            statement
               << "\"timestamp\":"             << timePointToNanoseconds<ReaderTimePoint>(timeStamp) << statement.sep()
               << "\"measurementID\":"         << measurementID                                      << statement.sep()
               << "\"sourceIP\":"              << statement.encodeAddress(sourceIP)                  << statement.sep()
               << "\"destinationIP\":"         << statement.encodeAddress(destinationIP)             << statement.sep()
               << "\"protocol\":"              << (unsigned int)protocol                             << statement.sep()
               << "\"trafficClass\":"          << (unsigned int)trafficClass                         << statement.sep()
               << "\"roundNumber\":"           << roundNumber                                        << statement.sep()
               << "\"packetSize\":"            << packetSize                                         << statement.sep()
               << "\"checksum\":"              << checksum                                           << statement.sep()
               << "\"sourcePort\":"            << sourcePort                                         << statement.sep()
               << "\"destinationPort\":"       << destinationPort                                    << statement.sep()
               << "\"status\":"                << status                                             << statement.sep()
               << "\"jitterType\":"            << jitterType                                         << statement.sep()
               << "\"timeSource\":"            << (long long)timeSource                              << statement.sep()

               << "\"appSendPackets\":"        << appSendPackets                                     << statement.sep()
               << "\"appSendMeanLatency\":"    << appSendMeanLatency                                 << statement.sep()
               << "\"appSendJitter\":"         << appSendJitter                                      << statement.sep()

               << "\"queuingPackets\":"        << queuingPackets                                     << statement.sep()
               << "\"queuingMeanLatency\":"    << queuingMeanLatency                                 << statement.sep()
               << "\"queuingJitter\":"         << queuingJitter                                      << statement.sep()

               << "\"appReceivePackets\":"     << appReceivePackets                                  << statement.sep()
               << "\"appReceiveMeanLatency\":" << appReceiveMeanLatency                              << statement.sep()
               << "\"appReceiveJitter\":"      << appReceiveJitter                                   << statement.sep()

               << "\"applicationPackets\":"    << applicationPackets                                 << statement.sep()
               << "\"applicationMeanRTT\":"    << applicationMeanRTT                                 << statement.sep()
               << "\"applicationJitter\":"     << applicationJitter                                  << statement.sep()

               << "\"softwarePackets\":"       << softwarePackets                                    << statement.sep()
               << "\"softwareMeanRTT\":"       << softwareMeanRTT                                    << statement.sep()
               << "\"softwareJitter\":"        << softwareJitter                                     << statement.sep()

               << "\"hardwarePackets\":"       << hardwarePackets                                    << statement.sep()
               << "\"hardwareMeanRTT\":"       << hardwareMeanRTT                                    << statement.sep()
               << "\"hardwareJitter\":"        << hardwareJitter;

            statement.endRow();
            rows++;
         }
         else {
            throw ResultsLogicException("Unknown output format");
         }
      }
      else {
         throw ResultsReaderDataErrorException("Unexpected input in input file " + dataFile.string());
      }
   }
}
