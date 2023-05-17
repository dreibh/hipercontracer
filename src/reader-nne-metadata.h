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

#ifndef READER_NNE_METADATA_H
#define READER_NNE_METADATA_H

#include "reader-base.h"

#include <chrono>
#include <set>

#include <boost/property_tree/ptree.hpp>


// Temporary fixes, should be turned OFF!
#define WITH_NODEID_FIX      // Wrong Node ID 4125
#define WITH_TIMESTAMP_FIX   // Timestamp granularity of 1s


// ###### Input file list structure #########################################
struct NorNetEdgeMetadataFileEntry {
   ReaderTimePoint       TimeStamp;
   unsigned int          NodeID;
   std::filesystem::path DataFile;
};

bool operator<(const NorNetEdgeMetadataFileEntry& a, const NorNetEdgeMetadataFileEntry& b);
std::ostream& operator<<(std::ostream& os, const NorNetEdgeMetadataFileEntry& entry);

int makeInputFileEntry(const std::filesystem::path& dataFile,
                       const std::smatch            match,
                       NorNetEdgeMetadataFileEntry& inputFileEntry,
                       const unsigned int           workers);
ReaderPriority getPriorityOfFileEntry(const NorNetEdgeMetadataFileEntry& inputFileEntry);


// ###### Reader class ######################################################
class NorNetEdgeMetadataReader : public ReaderImplementation<NorNetEdgeMetadataFileEntry>
{
   public:
   NorNetEdgeMetadataReader(const DatabaseConfiguration& databaseConfiguration,
                            const unsigned int           workers            = 1,
                            const unsigned int           maxTransactionSize = 4,
                            const std::string&           table_bins1min     = "node_metadata_bins1min",
                            const std::string&           table_event        = "node_metadata_event");
   virtual ~NorNetEdgeMetadataReader();

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
   template<typename TimePoint> static TimePoint makeMin(const TimePoint& timePoint);
   template<typename TimePoint> TimePoint parseTimeStamp(const boost::property_tree::ptree& item,
                                                         const TimePoint&                   now,
                                                         const std::filesystem::path&       dataFile) const;
   long long parseDelta(const boost::property_tree::ptree& item,
                        const std::filesystem::path&       dataFile) const;
   unsigned int parseNodeID(const boost::property_tree::ptree& item,
                            const std::filesystem::path&       dataFile) const;
   unsigned int parseNetworkID(const boost::property_tree::ptree& item,
                               const std::filesystem::path&       dataFile) const;
   std::string parseMetadataKey(const boost::property_tree::ptree& item,
                                const std::filesystem::path&       dataFile) const;
   std::string parseMetadataValue(const boost::property_tree::ptree& item,
                                  const std::filesystem::path&       dataFile) const;
   std::string parseExtra(const boost::property_tree::ptree& item,
                          const std::filesystem::path&       dataFile) const;

   private:
   static const std::string  Identification;
   static const std::regex   FileNameRegExp;
   const std::string         Table_bins1min;
   const std::string         Table_event;
#ifdef WITH_TIMESTAMP_FIX
   struct TimeStampFix {
      ReaderTimePoint        TSFixLastTimePoint;
      ReaderClock::duration  TSFixTimeOffset;
   };
   std::map<unsigned int, TimeStampFix*> TSFixMap;
#endif
};

#endif
