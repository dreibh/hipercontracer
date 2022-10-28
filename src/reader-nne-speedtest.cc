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

#include "reader-nne-speedtest.h"
#include "importer-exception.h"
#include "logger.h"
#include "tools.h"


const std::string NorNetEdgeSpeedTestReader::Identification = "SpeedTest";
const std::regex  NorNetEdgeSpeedTestReader::FileNameRegExp = std::regex(
   // Format: speedtest_<MeasurementID>.sdat.<YYYY-MM-DD_HH-MM-SS>.xz
   "^speedtest_([0-9]+)\\.sdat\\.([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9][0-9]-[0-9][0-9]-[0-9][0-9])\\.xz$"
);


// ###### Constructor #######################################################
NorNetEdgeSpeedTestReader::NorNetEdgeSpeedTestReader(const unsigned int workers,
                                                     const unsigned int maxTransactionSize)
   : NorNetEdgePingReader(workers, maxTransactionSize, std::string())
{
}


// ###### Destructor ########################################################
NorNetEdgeSpeedTestReader::~NorNetEdgeSpeedTestReader()
{
}


// ###### Get identification of reader ######################################
const std::string& NorNetEdgeSpeedTestReader::getIdentification() const
{
   return Identification;
}


// ###### Get input file name regular expression ############################
const std::regex& NorNetEdgeSpeedTestReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


// ###### Begin parsing #####################################################
void NorNetEdgeSpeedTestReader::beginParsing(DatabaseClientBase& databaseClient,
                                             unsigned long long& rows)
{
   rows = 0;
}


// ###### Finish parsing ####################################################
bool NorNetEdgeSpeedTestReader::finishParsing(DatabaseClientBase& databaseClient,
                                         unsigned long long& rows)
{
   if(rows > 0) {
      return true;
   }
   return false;
}


// ###### Parse input file ##################################################
void NorNetEdgeSpeedTestReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        boost::iostreams::filtering_istream& inputStream)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   static const unsigned int NorNetEdgeSpeedTestColumns   = 4;
   static const char         NorNetEdgeSpeedTestDelimiter = '\t';

   std::string inputLine;
   std::string tuple[NorNetEdgeSpeedTestColumns];
   while(std::getline(inputStream, inputLine)) {
      // ====== Parse line ==================================================
      size_t columns = 0;
      size_t start;
      size_t end = 0;
      while((start = inputLine.find_first_not_of(NorNetEdgeSpeedTestDelimiter, end)) != std::string::npos) {
         end = inputLine.find(NorNetEdgeSpeedTestDelimiter, start);

         if(columns == NorNetEdgeSpeedTestColumns) {
            throw ImporterReaderException("Too many columns in input file");
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns != NorNetEdgeSpeedTestColumns) {
         throw ImporterReaderException("Too few columns in input file");
      }

      // ====== Generate import statement ===================================
      if(backend & DatabaseBackendType::SQL_Generic) {
         databaseClient.clearStatement();
         databaseClient.getStatement()
            << "'CALL insert_measurement_data("
            << "'" << tuple[0] << "', "
            << std::stoul(tuple[1]) << ", "
            << std::stoul(tuple[2]) << ", "
            << "'" << tuple[3] << "')";
         databaseClient.executeStatement();
         databaseClient.clearStatement();
         rows++;
      }
      else {
         throw ImporterLogicException("Unknown output format");
      }
   }
}
