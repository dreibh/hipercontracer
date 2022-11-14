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

#ifndef READER_TRACEROUTE
#define READER_TRACEROUTE

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
   TracerouteReader(const DatabaseConfiguration& databaseConfiguration,
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

   private:
   static const std::string Identification;
   static const std::regex  FileNameRegExp;
   const std::string        Table;
};

#endif
