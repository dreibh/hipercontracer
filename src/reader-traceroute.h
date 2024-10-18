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

#ifndef READER_TRACEROUTE
#define READER_TRACEROUTE

#include "conversions.h"
#include "reader-base.h"


// ###### Input file list structure #########################################
struct TracerouteFileEntry {
   std::string           Source;
   ReaderTimePoint       TimeStamp;
   unsigned int          SeqNumber;
   std::filesystem::path DataFile;
};
bool operator<(const TracerouteFileEntry& a, const TracerouteFileEntry& b);
std::ostream& operator<<(std::ostream& os, const TracerouteFileEntry& entry);

int makeInputFileEntry(const std::filesystem::path& dataFile,
                       const std::smatch            match,
                       TracerouteFileEntry&         inputFileEntry,
                       const unsigned int           workers);
ReaderPriority getPriorityOfFileEntry(const TracerouteFileEntry& inputFileEntry);


// ###### Reader class ######################################################
class TracerouteReader : public ReaderImplementation<TracerouteFileEntry>
{
   public:
   TracerouteReader(const ImporterConfiguration& importerConfiguration,
                    const unsigned int           workers            = 1,
                    const unsigned int           maxTransactionSize = 4,
                    const std::string&           table              = "Traceroute");
   virtual ~TracerouteReader();

   virtual const std::string& getIdentification() const { return Identification; }
   virtual const std::regex&  getFileNameRegExp() const { return FileNameRegExp; }

   virtual void beginParsing(DatabaseClientBase& databaseClient,
                             unsigned long long& rows);
   virtual bool finishParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows);
   virtual void parseContents(DatabaseClientBase&                  databaseClient,
                              unsigned long long&                  rows,
                              const std::filesystem::path&         dataFile,
                              boost::iostreams::filtering_istream& dataStream);

   protected:
   unsigned long long parseMeasurementID(const std::string&           value,
                                         const std::filesystem::path& dataFile);
   boost::asio::ip::address parseAddress(const std::string&           value,
                                         const std::filesystem::path& dataFile);
   ReaderTimePoint parseTimeStamp(const std::string&           value,
                                  const ReaderTimePoint&       now,
                                  const bool                   inNanoseconds,
                                  const std::filesystem::path& dataFile);
   unsigned int parseRoundNumber(const std::string&           value,
                                 const std::filesystem::path& dataFile);
   uint8_t parseTrafficClass(const std::string&           value,
                             const std::filesystem::path& dataFile);
   unsigned int parsePacketSize(const std::string&           value,
                                const std::filesystem::path& dataFile);
   unsigned int parseResponseSize(const std::string&           value,
                                 const std::filesystem::path& dataFile);
   uint16_t parseChecksum(const std::string&           value,
                          const std::filesystem::path& dataFile);
   uint16_t parsePort(const std::string&           value,
                      const std::filesystem::path& dataFile);
   unsigned int parseStatus(const std::string&           value,
                            const std::filesystem::path& dataFile,
                            const unsigned int           base = 16);
   unsigned int parseTimeSource(const std::string&           value,
                                const std::filesystem::path& dataFile);
   unsigned int parseTotalHops(const std::string&           value,
                               const std::filesystem::path& dataFile);
   unsigned int parseHopNumber(const std::string&           value,
                               const std::filesystem::path& dataFile);
   unsigned int parseHop(const std::string&           value,
                         const std::filesystem::path& dataFile);
   long long parsePathHash(const std::string&           value,
                           const std::filesystem::path& dataFile);
   long long parseNanoseconds(const std::string&           value,
                              const std::filesystem::path& dataFile);

   protected:
   const std::string        Table;

   public:
   static const std::string  Identification;
   static const std::regex   FileNameRegExp;
   static const unsigned int FileNameRegExpMatchSize;
};

#endif
