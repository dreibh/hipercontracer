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

#include "conversions.h"
#include "reader-ping.h"
#include "importer-exception.h"
#include "logger.h"
#include "tools.h"

#include <boost/asio.hpp>


const std::string PingReader::Identification("Ping");
const std::regex  PingReader::FileNameRegExp(
   // Format: Ping-(Protocol-|)[P#]<ID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results<EXT>
   "^Ping-([A-Z]+-|)([#P])([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


// ###### Constructor #######################################################
PingReader::PingReader(const DatabaseConfiguration& databaseConfiguration,
                       const unsigned int           workers,
                       const unsigned int           maxTransactionSize,
                       const std::string&           table)
   : TracerouteReader(databaseConfiguration, workers, maxTransactionSize, table)
{
}


// ###### Destructor ########################################################
PingReader::~PingReader()
{
}


// ###### Begin parsing #####################################################
void PingReader::beginParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement              = databaseClient.getStatement("Ping", false, true);

   rows = 0;

   // ====== Generate import statement ======================================
   if(backend & DatabaseBackendType::SQL_Generic) {
      statement
         << "INSERT INTO " << Table
         << " (SendTimestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,BurstSeq,PacketSize,ResponseSize,Checksum,Status,TimeSource,Delay_AppSend,Delay_Queuing, Delay_AppReceive,RTT_App,RTT_SW,RTT_HW) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      statement << "{ \"" << Table <<  "\": [";
   }
   else {
      throw ImporterLogicException("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool PingReader::finishParsing(DatabaseClientBase& databaseClient,
                               unsigned long long& rows)
{
   const DatabaseBackendType backend   = databaseClient.getBackend();
   Statement&                statement = databaseClient.getStatement("Ping");
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
void PingReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        const std::filesystem::path&         dataFile,
        boost::iostreams::filtering_istream& dataStream)
{
   Statement&                statement = databaseClient.getStatement("Ping");
   const DatabaseBackendType backend   = databaseClient.getBackend();
   static const unsigned int PingMinColumns = 18;
   static const unsigned int PingMaxColumns = 18;
   static const char         PingDelimiter  = ' ';

   std::string inputLine;
   std::string tuple[PingMaxColumns];
   const ReaderTimePoint now = ReaderClock::now();
   while(std::getline(dataStream, inputLine)) {
      // ====== Conversion from old versions ================================
      if(inputLine.substr(0, 3) == "#P ") {
         inputLine = convertOldPingLine(inputLine);
      }

      // ====== Parse line ==================================================
      size_t columns = 0;
      size_t start;
      size_t end = 0;
      while((start = inputLine.find_first_not_of(PingDelimiter, end)) != std::string::npos) {
         end = inputLine.find(PingDelimiter, start);
         if(columns == PingMaxColumns) {
            // Skip additional columns
            break;
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns < PingMinColumns) {
         throw ImporterReaderDataErrorException("Too few columns in input file " + dataFile.string());
      }

      // ====== Generate import statement ===================================
      if( (tuple[0].size() >= 2) && (tuple[0][0] == '#') && (tuple[0][1] == 'P') ) {
         const char                     protocol        = tuple[0][2];
         const unsigned int             measurementID   = parseMeasurementID(tuple[1], dataFile);
         const boost::asio::ip::address sourceIP        = parseAddress(tuple[2], dataFile);
         const boost::asio::ip::address destinationIP   = parseAddress(tuple[3], dataFile);
         const ReaderTimePoint          sendTimeStamp   = parseTimeStamp(tuple[4], now, true, dataFile);
         uint8_t                        trafficClass    = parseTrafficClass(tuple[6], dataFile);
         const unsigned int             burstSeq        = parseRoundNumber(tuple[5], dataFile);
         unsigned int                   packetSize      = parsePacketSize(tuple[7], dataFile);
         const unsigned int             responseSize    = parseResponseSize(tuple[8], dataFile);
         const uint16_t                 checksum        = parseChecksum(tuple[9], dataFile);
         const unsigned int             status          = parseStatus(tuple[10], dataFile, 10);
         unsigned int                   timeSource      = parseTimeSource(tuple[11], dataFile);

         const long long                delayAppSend    = parseNanoseconds(tuple[12], dataFile);
         const long long                delayQueuing    = parseNanoseconds(tuple[13], dataFile);
         const long long                delayAppReceive = parseNanoseconds(tuple[14], dataFile);
         const long long                rttApp          = parseNanoseconds(tuple[15], dataFile);
         const long long                rttSoftware     = parseNanoseconds(tuple[16], dataFile);
         const long long                rttHardware     = parseNanoseconds(tuple[17], dataFile);

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << timePointToNanoseconds<ReaderTimePoint>(sendTimeStamp) << statement.sep()
               << measurementID                                          << statement.sep()
               << statement.encodeAddress(sourceIP)                      << statement.sep()
               << statement.encodeAddress(destinationIP)                 << statement.sep()
               << (unsigned int)protocol                                 << statement.sep()
               << (unsigned int)trafficClass                             << statement.sep()
               << burstSeq                                               << statement.sep()
               << packetSize                                             << statement.sep()
               << responseSize                                           << statement.sep()
               << checksum                                               << statement.sep()
               << status                                                 << statement.sep()

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
            statement.beginRow();
            statement
               << "\"sendTimestamp\": " << timePointToNanoseconds<ReaderTimePoint>(sendTimeStamp) << statement.sep()
               << "\"measurementID\": " << measurementID                                          << statement.sep()
               << "\"sourceIP\": "      << statement.encodeAddress(sourceIP)                      << statement.sep()
               << "\"destinationIP\": " << statement.encodeAddress(destinationIP)                 << statement.sep()
               << "\"protocol\": \""    << protocol << "\""                                       << statement.sep()
               << "\"trafficClass\": "  << (unsigned int)trafficClass                             << statement.sep()
               << "\"burstSeq\": "      << burstSeq                                               << statement.sep()
               << "\"packetSize\": "    << packetSize                                             << statement.sep()
               << "\"responseSize\": "  << responseSize                                           << statement.sep()
               << "\"checksum\": "      << checksum                                               << statement.sep()
               << "\"status\": "        << status                                                 << statement.sep()

               << "\"timeSource\": "    << (long long)timeSource                                  << statement.sep()
               << "\"delay.appSend\": " << delayAppSend                                           << statement.sep()
               << "\"delay.queuing\": " << delayQueuing                                           << statement.sep()
               << "\"delay.appRecv\": " << delayAppReceive                                        << statement.sep()
               << "\"rtt.app\": "       << rttApp                                                 << statement.sep()
               << "\"rtt.sw\": "        << rttSoftware                                            << statement.sep()
               << "\"rtt.hw\": "        << rttHardware                                            << statement.sep();

            statement.endRow();
            rows++;
         }
         else {
            throw ImporterLogicException("Unknown output format");
         }
      }
      else {
         throw ImporterReaderDataErrorException("Unexpected input in input file " + dataFile.string());
      }
   }
}
