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

#ifndef READER_PING
#define READER_PING

#include "reader-traceroute.h"

#include <chrono>


class PingReader : public TracerouteReader
{
   public:
   PingReader(const unsigned int workers            = 1,
              const unsigned int maxTransactionSize = 4,
              const std::string& table              = "Ping");
   virtual ~PingReader();

   virtual const std::string& getIdentification() const;
   virtual const std::regex&  getFileNameRegExp() const;

   virtual void beginParsing(DatabaseClientBase& databaseClient,
                             unsigned long long& rows);
   virtual bool finishParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows);
   virtual void parseContents(DatabaseClientBase&                  databaseClient,
                              unsigned long long&                  rows,
                              const std::filesystem::path&         dataFile,
                              boost::iostreams::filtering_istream& dataStream);

   private:
   template<typename TimePoint> static TimePoint parseTimeStamp(const std::string&           value,
                                                                const TimePoint&             now,
                                                                const std::filesystem::path& dataFile);
   static uint16_t parseChecksum(const std::string&           value,
                                 const std::filesystem::path& dataFile);
   static unsigned int parseStatus(const std::string&           value,
                                   const std::filesystem::path& dataFile);
   static unsigned int parseRTT(const std::string&           value,
                                const std::filesystem::path& dataFile);
   static unsigned int parsePacketSize(const std::string&           value,
                                       const std::filesystem::path& dataFile);
   static uint8_t parseTrafficClass(const std::string&           value,
                                    const std::filesystem::path& dataFile);

   static const std::string Identification;
   static const std::regex  FileNameRegExp;
   const std::string        Table;
};

#endif
