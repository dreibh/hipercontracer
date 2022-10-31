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

#ifndef READER_BASE_H
#define READER_BASE_H

#include "databaseclient-base.h"

#include <filesystem>
#include <iostream>
#include <list>
#include <mutex>
#include <regex>
#include <string>

#include <boost/iostreams/filtering_stream.hpp>


class ReaderBase
{
   public:
   ReaderBase(const unsigned int workers,
              const unsigned int maxTransactionSize);
   virtual ~ReaderBase();

   virtual const std::string& getIdentification() const = 0;
   virtual const std::regex&  getFileNameRegExp() const = 0;

   virtual int addFile(const std::filesystem::path& dataFile,
                       const std::smatch            match) = 0;
   virtual bool removeFile(const std::filesystem::path& dataFile,
                           const std::smatch            match) = 0;
   virtual unsigned int fetchFiles(std::list<std::filesystem::path>& dataFileList,
                                   const unsigned int                worker,
                                   const unsigned int                limit = 1) = 0;
   virtual void printStatus(std::ostream& os = std::cout) = 0;

   virtual void beginParsing(DatabaseClientBase& databaseClient,
                             unsigned long long& rows) = 0;
   virtual bool finishParsing(DatabaseClientBase& databaseClient,
                              unsigned long long& rows) = 0;
   virtual void parseContents(DatabaseClientBase&                  databaseClient,
                              unsigned long long&                  rows,
                              const std::filesystem::path&         dataFile,
                              boost::iostreams::filtering_istream& dataStream) = 0;

   inline const unsigned int getWorkers() const { return Workers; }
   inline const unsigned int getMaxTransactionSize() const { return MaxTransactionSize; }

   protected:
   const unsigned int Workers;
   const unsigned int MaxTransactionSize;
   unsigned long long TotalFiles;
   std::mutex         Mutex;
};

#endif
