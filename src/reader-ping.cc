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


const std::string PingReader::Identification = "Ping";
const std::regex  PingReader::FileNameRegExp = std::regex(
   // Format: Ping-<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Ping-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


// ###### Constructor #######################################################
PingReader::PingReader(const unsigned int workers,
                       const unsigned int maxTransactionSize,
                       const std::string& table)
   : TracerouteReader(workers, maxTransactionSize, table)
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


// ###### Begin parsing #####################################################
void PingReader::beginParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows)
{
   rows = 0;

   // ====== Generate import statement ======================================
   const DatabaseBackendType backend = databaseClient.getBackend();
   if(backend & DatabaseBackendType::SQL_Generic) {
//       assert(databaseClient.statementIsEmpty());
//       databaseClient.getStatement()
//          << "INSERT INTO " << Table_measurement_generic_data
//          << "(ts, mi_id, seq, xml_data, crc, stats) VALUES \n";
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
      const DatabaseBackendType backend = databaseClient.getBackend();
      if(backend & DatabaseBackendType::SQL_Generic) {
         if(rows > 0) {
//             databaseClient.getStatement()
//                << "\nON DUPLICATE KEY UPDATE stats=stats;\n";
//             databaseClient.executeStatement();
         }
         else {
            databaseClient.clearStatement();
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
        boost::iostreams::filtering_istream& inputStream)
{
   assert(false);
//    const DatabaseBackendType backend = databaseClient.getBackend();
//    static const unsigned int PingColumns   = 4;
//    static const char         PingDelimiter = '\t';
//
//    std::string inputLine;
//    std::string tuple[PingColumns];
//    while(std::getline(inputStream, inputLine)) {
//       // ====== Parse line ==================================================
//       size_t columns = 0;
//       size_t start;
//       size_t end = 0;
//       while((start = inputLine.find_first_not_of(PingDelimiter, end)) != std::string::npos) {
//          end = inputLine.find(PingDelimiter, start);
//
//          if(columns == PingColumns) {
//             throw ImporterReaderException("Too many columns in input file");
//          }
//          tuple[columns++] = inputLine.substr(start, end - start);
//       }
//       if(columns != PingColumns) {
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
