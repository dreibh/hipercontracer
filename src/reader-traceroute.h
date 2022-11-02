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

#include <chrono>
#include <mutex>
#include <set>


class TracerouteReader : public ReaderBase
{
   public:
   TracerouteReader(const std::filesystem::path& importFilePath,
                    const unsigned int           workers            = 1,
                    const unsigned int           maxTransactionSize = 4,
                    const std::string&           table              = "Traceroute");
   virtual ~TracerouteReader();

   virtual const std::string& getIdentification() const;
   virtual const std::regex&  getFileNameRegExp() const;

   virtual int addFile(const std::filesystem::path& dataFile,
                       const std::smatch            match);
   virtual bool removeFile(const std::filesystem::path& dataFile,
                           const std::smatch            match);
   virtual unsigned int fetchFiles(std::list<std::filesystem::path>& dataFileList,
                                   const unsigned int                worker,
                                   const unsigned int                limit = 1);
   virtual void printStatus(std::ostream& os = std::cout);

   virtual void beginParsing(DatabaseClientBase& databaseClient,
                             unsigned long long& rows);
   virtual bool finishParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows);
   virtual void parseContents(DatabaseClientBase&                  databaseClient,
                              unsigned long long&                  rows,
                              const std::filesystem::path&         dataFile,
                              boost::iostreams::filtering_istream& dataStream);

   protected:
   typedef std::chrono::system_clock               FileEntryClock;
   typedef std::chrono::time_point<FileEntryClock> FileEntryTimePoint;
   struct InputFileEntry {
      std::string           Source;
      FileEntryTimePoint    TimeStamp;
      unsigned int          SeqNumber;
      std::filesystem::path DataFile;
   };
   friend bool operator<(const TracerouteReader::InputFileEntry& a,
                         const TracerouteReader::InputFileEntry& b);
   friend std::ostream& operator<<(std::ostream& os, const InputFileEntry& entry);

   private:
   static const std::string  Identification;
   static const std::regex   FileNameRegExp;
   const std::string         Table;
   std::set<InputFileEntry>* DataFileSet;
};

#endif
