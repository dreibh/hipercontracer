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


// ###### Get identification of reader ######################################
const std::string& PingReader::getIdentification() const
{
   return Identification;
}


// ###### Get input file name regular expression ############################
const std::regex& PingReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


// ###### Parse time stamp ##################################################
template<typename TimePoint> TimePoint PingReader::parseTimeStamp(
                                          const std::string&           value,
                                          const TimePoint&             now,
                                          const std::filesystem::path& dataFile)
{
   size_t                   index;
   const unsigned long long ts = std::stoull(value, &index, 16);
   if(index != value.size()) {
      throw ImporterReaderDataErrorException("Bad time stamp format " + value);
   }
   const TimePoint timeStamp =    microsecondsToTimePoint<std::chrono::time_point<std::chrono::high_resolution_clock>>(ts);
   if( (timeStamp < now - std::chrono::hours(365 * 24)) ||   /* 1 year in the past  */
       (timeStamp > now + std::chrono::hours(24)) ) {        /* 1 day in the future */
      throw ImporterReaderDataErrorException("Bad time stamp value " + value);
   }
   return timeStamp;
}

// ###### Parse time stamp ##################################################
uint16_t PingReader::parseChecksum(const std::string&           value,
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
unsigned int PingReader::parseStatus(const std::string&           value,
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
unsigned int PingReader::parseRTT(const std::string&           value,
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
unsigned int PingReader::parsePacketSize(const std::string&           value,
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
uint8_t PingReader::parseTrafficClass(const std::string&           value,
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


// ###### Begin parsing #####################################################
void PingReader::beginParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows)
{
   rows = 0;

   // ====== Generate import statement ======================================
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& statement              = databaseClient.getStatement("Ping", false, true);
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
   if(rows > 0) {
      // ====== Generate import statement ===================================
      const DatabaseBackendType backend   = databaseClient.getBackend();
      Statement&                statement = databaseClient.getStatement("Ping");
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
   std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
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
         const std::chrono::time_point<std::chrono::high_resolution_clock> timeStamp =
            parseTimeStamp<std::chrono::time_point<std::chrono::high_resolution_clock>>(tuple[3], now, dataFile);
         const boost::asio::ip::address sourceIP      = boost::asio::ip::address::from_string(tuple[1]);
         const boost::asio::ip::address destinationIP = boost::asio::ip::address::from_string(tuple[2]);
         const uint16_t                 checksum      = parseChecksum(tuple[4], dataFile);
         const unsigned int             status        = parseStatus(tuple[5], dataFile);
         const unsigned int             rtt           = parseRTT(tuple[6], dataFile);
         uint8_t                        trafficClass  = 0x0;
         unsigned int                   packetSize    = 0;
         if(columns >= 8) {
            trafficClass = parseTrafficClass(tuple[7], dataFile);
            if(packetSize >= 8) {
               packetSize = parsePacketSize(tuple[8], dataFile);
            }
         }

         if(backend & DatabaseBackendType::SQL_Generic) {
            statement.beginRow();
            statement
               << statement.quote(timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(timeStamp, 6)) << statement.sep()
               << statement.quote(sourceIP.to_string())      << statement.sep()
               << statement.quote(destinationIP.to_string()) << statement.sep()
               << checksum                   << statement.sep()
               << packetSize                 << statement.sep()
               << (unsigned int)trafficClass << statement.sep()
               << status                     << statement.sep()
               << rtt;
            statement.endRow();
            rows++;
         }
         else if(backend & DatabaseBackendType::NoSQL_Generic) {
            assert(false);   // FIXME packet addresses!
            statement.beginRow();
            statement
               << "'timestamp': "   << timePointToMicroseconds<std::chrono::time_point<std::chrono::high_resolution_clock>>(timeStamp) << statement.sep()
               << "'source': "      << 0            << statement.sep()
               << "'destination': " << 0            << statement.sep()
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
