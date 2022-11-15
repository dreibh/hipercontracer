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

#include "reader-ping.h"
#include "importer-exception.h"
#include "logger.h"
#include "tools.h"

#include <boost/asio.hpp>


const std::string PingReader::Identification = "Ping";
const std::regex  PingReader::FileNameRegExp = std::regex(
   // Format: Ping-P<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Ping-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
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
         << " (TimeStamp, FromIP, ToIP, Checksum, PktSize, TC, Status, RTT) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      statement << "db['" << Table << "'].insert(";
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
      if(tuple[0] == "#P")  {
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
            if(packetSize >= 8) {   // PacketSize was added in HiPerConTracer 1.6.0!
               packetSize = parsePacketSize(tuple[8], dataFile);
            }
         }

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << statement.quote(timePointToString<ReaderTimePoint>(timeStamp, 6)) << statement.sep()
               << statement.quote(sourceIP.to_string())                             << statement.sep()
               << statement.quote(destinationIP.to_string())                        << statement.sep()
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
               << "'timestamp': "   << timePointToMicroseconds<ReaderTimePoint>(timeStamp)  << statement.sep()
               << "'source': "      << statement.quote(addressToBytesString(sourceIP))      << statement.sep()
               << "'destination': " << statement.quote(addressToBytesString(destinationIP)) << statement.sep()
               << "'checksum': "    << checksum     << statement.sep()
               << "'pktsize': "     << packetSize   << statement.sep()
               << "'tc': "          << trafficClass << statement.sep()
               << "'status': "      << status       << statement.sep()
               << "'rtt': "         << rtt;
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
