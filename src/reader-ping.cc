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
         << " (TimeStamp,FromIP,ToIP,Checksum,PktSize,TC,Status,RTT) VALUES";
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
   static const unsigned int PingMinColumns = 7;
   static const unsigned int PingMaxColumns = 9;
   static const char         PingDelimiter  = ' ';

   std::string inputLine;
   std::string tuple[PingMaxColumns];
   const ReaderTimePoint now = ReaderClock::now();
   while(std::getline(dataStream, inputLine)) {
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
         // ------ Obtain version -------------------------------------------
         unsigned int version;
         const char   protocol;
         if( ((tuple[0].size() > 2) && (columns >= 18) ) {
            version  = 2;
            protocol = tuple[0][2];
         }
         else if(columns >= 7) {
            version  = 1;
            protocol = 'i';
         }
         else {
            throw ImporterReaderDataErrorException("Unexpected syntax in input file " + dataFile.string());
         }

         const unsigned int             measurementID = (version >= 2) ? paraseMeasruementID(tuple[1]) : 0;
         const boost::asio::ip::address sourceIP      = parseAddress(tuple[(version >= 2) 2 : 1], backend, dataFile);
         const boost::asio::ip::address destinationIP = parseAddress(tuple[(version >= 2) 3 : 2], backend, dataFile);
         const ReaderTimePoint          timeStamp     = parseTimeStamp(tuple[(version >= 2) 4 : 3], now, dataFile);
         uint8_t                        trafficClass  = (version >= 2) ? parseTrafficClass(tuple[6], dataFile) : 0x00;
         const unsigned int             burstSeq      = (version >= 2) ? parseBurstSeq(tuple[5], dataFile) : 0;
         unsigned int                   packetSize    = (version >= 2) ? parseTrafficClass(tuple[7], dataFile) : 0;
         const unsigned int             responseSize  = (version >= 2) ? parseTrafficClass(tuple[8], dataFile) : 0;
         const uint16_t                 checksum      = parseChecksum(tuple[(version >= 2) 9 : 4], dataFile);
         const unsigned int             status        = parseStatus(tuple[(version >= 2) 10 : 5], dataFile, 10);   // Decimal!
         unsigned int                   timeSource    = (version >= 2) ? parseTimeSource(tuple[11]) : 0x00000000;

         const long long                delayAppSend  = (version >= 2) ? parseNanoseconds(tuple[12]) : -1;
         const long long                delayAppSend  = (version >= 2) ? parseNanoseconds(tuple[12]) : -1;
         const long long                delayAppSend  = (version >= 2) ? parseNanoseconds(tuple[12]) : -1;
         const long long                delayAppSend  = (version >= 2) ? parseNanoseconds(tuple[12]) : -1;

         if(version == 1) {
            if(columns >= 8) {   // TrafficClass was added in HiPerConTracer 1.4.0!
               trafficClass = parseTrafficClass(tuple[7], dataFile);
               if(columns >= 9) {   // PacketSize was added in HiPerConTracer 1.6.0!
                  packetSize = parsePacketSize(tuple[8], dataFile);
                  if(columns >= 10) {   // TimeSource was added in HiPerConTracer 2.0.0!
                     timeSource = parseTimeSource(tuple[9], dataFile);
                  }
               }
            }
         }

#if 0
         const unsigned int             rtt           = parseRTT(tuple[(version >= 2) 15 : 6], dataFile);



                  "Ping "                  // "#P<p>"
                  "MeasurementID "         // Measurement ID
                  "SourceIP "              // Source address
                  "DestinationIP "         // Destination address
                  "Timestamp "             // Timestamp (nanoseconds since the UTC epoch, hexadecimal).
                  "BurstSeq "              // Sequence number within a burst (decimal), numbered from 0.
                  "TrafficClass "          // Traffic Class setting (hexadeciaml)
                  "PacketSize "            // Packet size, in bytes (decimal)
                  "ResponseSize "          // Response size, in bytes (decimal)
                  "Checksum "              // Checksum (hexadeciaml)
                  "Status "                // Status (decimal)
                  "TimeSource "            // Source of the timing information (hexadecimal) as: AAQQSSHH
                  "Delay.AppSend "         // The measured application send delay (nanoseconds, decimal; -1 if not available).
                  "Delay.Queuing "         // The measured kernel software queuing delay (nanoseconds, decimal; -1 if not available).
                  "Delay.AppReceive "      // The measured application receive delay (nanoseconds, decimal; -1 if not available).
                  "RTT.App "               // The measured application RTT (nanoseconds, decimal).
                  "RTT.SW "                // The measured kernel software RTT (nanoseconds, decimal; -1 if not available).
                  "RTT.HW";                // The measured kernel hardware RTT (nanoseconds, decimal; -1 if not available).

         }
         // ------ Ping, Version 1 ------------------------------------------
         else {

         }

         const ReaderTimePoint          timeStamp     = parseTimeStamp(tuple[3], now, dataFile);
         const boost::asio::ip::address sourceIP      = parseAddress(tuple[1], backend, dataFile);
         const boost::asio::ip::address destinationIP = parseAddress(tuple[2], backend, dataFile);
         const uint16_t                 checksum      = parseChecksum(tuple[4], dataFile);
         const unsigned int             status        = parseStatus(tuple[5], dataFile, 10);   // Decimal!
         const unsigned int             rtt           = parseRTT(tuple[6], dataFile);
         uint8_t                        trafficClass  = 0x0;
         unsigned int                   packetSize    = 0;
         if(columns >= 8) {   // TrafficClass was added in HiPerConTracer 1.4.0!
            trafficClass = parseTrafficClass(tuple[7], dataFile);
            if(columns >= 9) {   // PacketSize was added in HiPerConTracer 1.6.0!
               packetSize = parsePacketSize(tuple[8], dataFile);
            }
         }

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << statement.quote(timePointToString<ReaderTimePoint>(timeStamp, 6)) << statement.sep()
               << statement.encodeAddress(sourceIP)                                 << statement.sep()
               << statement.encodeAddress(destinationIP)                             << statement.sep()
               << checksum                   << statement.sep()
               << packetSize                 << statement.sep()
               << (unsigned int)trafficClass << statement.sep()
               << status                     << statement.sep()
               << rtt;
            statement.endRow();
            rows++;
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            statement.beginRow();
            statement
               << "\"timestamp\": "   << timePointToMicroseconds<ReaderTimePoint>(timeStamp) << statement.sep()
               << "\"source\": "      << statement.encodeAddress(sourceIP)                   << statement.sep()
               << "\"destination\": " << statement.encodeAddress(destinationIP)              << statement.sep()
               << "\"checksum\": "    << checksum                                            << statement.sep()
               << "\"pktsize\": "     << packetSize                                          << statement.sep()
               << "\"tc\": "          << (unsigned int)trafficClass                          << statement.sep()
               << "\"status\": "      << status                                              << statement.sep()
               << "\"rtt\": "         << rtt;
            statement.endRow();
            rows++;
         }
         else {
            throw ImporterLogicException("Unknown output format");
         }
#endif
      }
      else {
         throw ImporterReaderDataErrorException("Unexpected input in input file " + dataFile.string());
      }
   }
}
