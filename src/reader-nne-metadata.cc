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

#include "reader-nne-metadata.h"
#include "importer-exception.h"
#include "logger.h"
#include "tools.h"

#include <boost/property_tree/json_parser.hpp>


#if 0
// ###### Create indentation string #########################################
static std::string indent(const unsigned int level, const char* indentation = "\t")
{
   std::string string;
   for(unsigned int i = 0; i < level; i++) {
      string += "\t";
   }
   return string;
}


// ###### Dump property tree ################################################
static void dumpPropertyTree(std::ostream&                      os,
                             const boost::property_tree::ptree& propertyTree,
                             const unsigned int                 level = 0)
{
   if(propertyTree.empty()) {
      os << "\"" << propertyTree.data() << "\"";
   }
   else {
      os << "{\n"
      for(boost::property_tree::ptree::const_iterator propertySubTreeIterator = propertyTree.begin();
          propertySubTreeIterator != propertyTree.end(); )  {
         os << indent(level + 1) << "\"" << propertySubTreeIterator->first << "\": ";
         dumpPropertyTree(os, propertySubTreeIterator->second, level + 1);
         propertySubTreeIterator++;
         if(propertySubTreeIterator != propertyTree.end()) {
             os << ",";
         }
         os << "\n";
      }
      os << indent(level) << "}";
   }
}
#endif


const std::string NorNetEdgeMetadataReader::Identification = "NorNetEdgeMetadata";
const std::regex  NorNetEdgeMetadataReader::FileNameRegExp = std::regex(
   // Format: nne<NodeID>-metadatacollector-<YYYYMMDD>T<HHMMSS>.json
   "^nne([0-9]+)-metadatacollector-([0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]T[0-9][0-9][0-9][0-9][0-9][0-9])\\.json$"
);


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const NorNetEdgeMetadataFileEntry& a, const NorNetEdgeMetadataFileEntry& b)
{
   // ====== Level 1: TimeStamp =============================================
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   else if(a.TimeStamp == b.TimeStamp) {
      // ====== Level 2: NodeID =============================================
      if(a.NodeID < b.NodeID) {
         return true;
      }
      else if(a.NodeID == b.NodeID) {
         // ====== Level 3: DataFile ========================================
         if(a.DataFile < b.DataFile) {
            return true;
         }
      }
   }
   return false;
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const NorNetEdgeMetadataFileEntry& entry)
{
   os << "("
      << timePointToString<ReaderTimePoint>(entry.TimeStamp) << ", "
      << entry.NodeID << ", "
      << entry.DataFile
      << ")";
   return os;
}


#ifdef WITH_NODEID_FIX
// ###### Extract NodeID from path ##########################################
// Assumption: node ID from 100 to 9999.
// The function handles "all in one directory" as well as "hierarchical" setup.
static unsigned int getNodeIDFromPath(const std::filesystem::path& dataFile)
{
   unsigned int nodeID = 0;

   std::filesystem::path parent = dataFile.parent_path();
   while(parent.has_filename()) {
      if(parent.filename().string().size() >= 3) {
         const unsigned int n = atol(parent.filename().string().c_str());
         if( (n >=100) && (n <= 9999) ) {
            nodeID = n;
         }
      }
      parent = parent.parent_path();
   }

   return nodeID;
}
#endif


// ###### Make NorNetEdgeMetadataFileEntry from file name  ######################
int makeInputFileEntry(const std::filesystem::path& dataFile,
                       const std::smatch            match,
                       NorNetEdgeMetadataFileEntry& inputFileEntry,
                       const unsigned int           workers)
{
   if(match.size() == 3) {
      ReaderTimePoint timeStamp;
      if(stringToTimePoint<ReaderTimePoint>(match[2].str(), timeStamp, "%Y%m%dT%H%M%S")) {
         inputFileEntry.TimeStamp = timeStamp;
         inputFileEntry.NodeID    = atol(match[1].str().c_str());
#ifdef WITH_NODEID_FIX
         const unsigned int nodeIDFromPath = getNodeIDFromPath(dataFile);
         if( (inputFileEntry.NodeID == 4125) && (inputFileEntry.NodeID != nodeIDFromPath) ) {
            inputFileEntry.NodeID = nodeIDFromPath;
         }
#endif
         inputFileEntry.DataFile  = dataFile;
         const int workerID       = inputFileEntry.NodeID % workers;
         return workerID;
      }
   }
   return -1;
}


// ###### Get priority of NorNetEdgeMetadataFileEntry #######################
ReaderPriority getPriorityOfFileEntry(const NorNetEdgeMetadataFileEntry& inputFileEntry)
{
   const ReaderTimePoint    now = nowInUTC<ReaderTimePoint>();
   const ReaderTimeDuration age = now - inputFileEntry.TimeStamp;
   const long long seconds = std::chrono::duration_cast<std::chrono::seconds>(age).count();
   if(seconds < 6 * 3600) {
      return ReaderPriority::High;
   }
   return ReaderPriority::Low;
}


// ###### Constructor #######################################################
NorNetEdgeMetadataReader::NorNetEdgeMetadataReader(const DatabaseConfiguration& databaseConfiguration,
                                                   const unsigned int           workers,
                                                   const unsigned int           maxTransactionSize,
                                                   const std::string&           table_bins1min,
                                                   const std::string&           table_event)
   : ReaderImplementation<NorNetEdgeMetadataFileEntry>(databaseConfiguration, workers, maxTransactionSize),
     Table_bins1min(table_bins1min),
     Table_event(table_event)
{
   // ====== Sanity checks ==================================================
   const unsigned long long t1 = 1666261441000000;
   const unsigned long long t2 = 1000000000000000;
   const unsigned long long t3 = 2000000000000000;
   const unsigned long long t4 = 1000000000123456;
   const ReaderTimePoint tp1 = microsecondsToTimePoint<ReaderTimePoint>(t1);
   const ReaderTimePoint tp2 = microsecondsToTimePoint<ReaderTimePoint>(t2);
   const ReaderTimePoint tp3 = microsecondsToTimePoint<ReaderTimePoint>(t3);
   const ReaderTimePoint tp4 = microsecondsToTimePoint<ReaderTimePoint>(t4);
   const std::string ts1 = timePointToString<ReaderTimePoint>(tp1, 0);
   const std::string ts2 = timePointToString<ReaderTimePoint>(tp2, 6);
   const std::string ts3 = timePointToString<ReaderTimePoint>(tp3, 0);
   const std::string ts4 = timePointToString<ReaderTimePoint>(tp4, 6);
   const ReaderTimePoint dp1 = makeMin<ReaderTimePoint>(tp1);
   const ReaderTimePoint dp2 = makeMin<ReaderTimePoint>(tp2);
   const ReaderTimePoint dp3 = makeMin<ReaderTimePoint>(tp3);
   const ReaderTimePoint dp4 = makeMin<ReaderTimePoint>(tp4);
   const std::string ds1 = timePointToString<ReaderTimePoint>(dp1, 0);
   const std::string ds2 = timePointToString<ReaderTimePoint>(dp2, 6);
   const std::string ds3 = timePointToString<ReaderTimePoint>(dp3, 0);
   const std::string ds4 = timePointToString<ReaderTimePoint>(dp4, 6);
/*
   std::cout << t1 << "\t" << ts1 << "\t\t" << ds1 << "\n";
   std::cout << t2 << "\t" << ts2 << "\t" << ds2 << "\n";
   std::cout << t3 << "\t" << ts3 << "\t\t" << ds3 << "\n";
   std::cout << t4 << "\t" << ts4 << "\t" << ds4 << "\n";
*/

   assert(ts1 == "2022-10-20 10:24:01");
   assert(ts2 == "2001-09-09 01:46:40.000000");
   assert(ts3 == "2033-05-18 03:33:20");
   assert(ts4 == "2001-09-09 01:46:40.123456");

   assert(ds1 == "2022-10-20 10:24:00");
   assert(ds2 == "2001-09-09 01:46:00.000000");
   assert(ds3 == "2033-05-18 03:33:00");
   assert(ds4 == "2001-09-09 01:46:00.000000");
}


// ###### Destructor ########################################################
NorNetEdgeMetadataReader::~NorNetEdgeMetadataReader()
{
#ifdef WITH_TIMESTAMP_FIX
   std::map<unsigned int, TimeStampFix*>::iterator iterator = TSFixMap.begin();
   while(iterator != TSFixMap.end()) {
      delete iterator->second;
      TSFixMap.erase(iterator);
      iterator = TSFixMap.begin();
   }
#endif
}


// ###### Helper function to calculate "min" value ##########################
template<typename TimePoint> TimePoint NorNetEdgeMetadataReader::makeMin(const TimePoint& timePoint)
{
   unsigned long long us = timePointToMicroseconds<ReaderTimePoint>(timePoint);
   us = us - (us % (60000000ULL));   // floor to minute
   return  microsecondsToTimePoint<ReaderTimePoint>(us);
}


// ###### Parse time stamp ##################################################
template<typename TimePoint> TimePoint NorNetEdgeMetadataReader::parseTimeStamp(
                                          const boost::property_tree::ptree& item,
                                          const TimePoint&                   now,
                                          const std::filesystem::path&       dataFile) const
{
   const unsigned long long ts        = (unsigned long long)rint(1000000.0 * item.get<double>("ts"));
   const TimePoint          timeStamp = microsecondsToTimePoint<ReaderTimePoint>(ts);
   if( (timeStamp < now - std::chrono::hours(365 * 24)) ||   /* 1 year in the past  */
       (timeStamp > now + std::chrono::hours(24)) ) {        /* 1 day in the future */
      throw ImporterReaderDataErrorException("Bad time stamp " + std::to_string(ts) +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   return timeStamp;
}


// ###### Parse delta #######################################################
long long NorNetEdgeMetadataReader::parseDelta(const boost::property_tree::ptree& item,
                                               const std::filesystem::path&       dataFile) const
{
   const unsigned int delta = round(item.get<double>("delta"));
   if( (delta < 0) || (delta > 4294967295.0) ) {
      throw ImporterReaderDataErrorException("Bad delta " + std::to_string(delta) +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   return delta;
}


// ###### Parse node ID #####################################################
unsigned int NorNetEdgeMetadataReader::parseNodeID(const boost::property_tree::ptree& item,
                                                   const std::filesystem::path&       dataFile) const
{
   const std::string& nodeName = item.get<std::string>("node");
   if(nodeName.substr(0, 3) != "nne") {
      throw ImporterReaderDataErrorException("Bad node name " + nodeName +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   const unsigned int nodeID = atol(nodeName.substr(3, nodeName.size() -3).c_str());
   if( (nodeID < 1) || (nodeID > 9999) ) {
      throw ImporterReaderDataErrorException("Bad node ID " + std::to_string(nodeID) +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   return nodeID;
}


// ###### Parse network ID ##################################################
unsigned int NorNetEdgeMetadataReader::parseNetworkID(const boost::property_tree::ptree& item,
                                                      const std::filesystem::path&       dataFile) const
{
   const unsigned int networkID = item.get<unsigned int>("network_id");
   if(networkID > 99) {   // MNC is a two-digit number
      throw ImporterReaderDataErrorException("Bad network ID " + std::to_string(networkID) +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   return networkID;
}


// ###### Parse metadata key ################################################
std::string NorNetEdgeMetadataReader::parseMetadataKey(const boost::property_tree::ptree& item,
                                                       const std::filesystem::path&       dataFile) const
{
   const std::string& metadataKey = item.get<std::string>("key");
   if(metadataKey.size() > 45) {
      throw ImporterReaderDataErrorException("Too long metadata key " + metadataKey +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   return metadataKey;
}


// ###### Parse metadata value ##############################################
std::string NorNetEdgeMetadataReader::parseMetadataValue(const boost::property_tree::ptree& item,
                                                         const std::filesystem::path&       dataFile) const
{
   std::string metadataValue = item.get<std::string>("value");
   if(metadataValue.size() > 500) {
      throw ImporterReaderDataErrorException("Too long metadata value " + metadataValue +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   if(metadataValue == "null") {
      metadataValue = std::string();
   }
   return metadataValue;
}


// ###### Parse extra data ##################################################
std::string NorNetEdgeMetadataReader::parseExtra(const boost::property_tree::ptree& item,
                                                 const std::filesystem::path&       dataFile) const
{
   std::string extra = item.get<std::string>("extra", std::string());
   if(extra.size() > 500) {
      throw ImporterReaderDataErrorException("Too long extra " + extra +
                                             " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
   }
   if(extra == "null") {
      extra = std::string();
   }
   return extra;
}


// ###### Begin parsing #####################################################
void NorNetEdgeMetadataReader::beginParsing(DatabaseClientBase& databaseClient,
                                            unsigned long long& rows)
{
   rows = 0;

   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& eventStatement    = databaseClient.getStatement("event",    false, true);
   Statement& bins1minStatement = databaseClient.getStatement("bins1min", false, true);
   if(backend & DatabaseBackendType::SQL_Generic) {
      eventStatement
         << "INSERT INTO " << Table_event
         << "(ts, node_id, network_id, metadata_key, metadata_value, extra, min) VALUES";
      bins1minStatement
          << "INSERT INTO " << Table_bins1min
          << "(ts, delta, node_id, network_id, metadata_key, metadata_value) VALUES";
   }
   else if(backend & DatabaseBackendType::NoSQL_Generic) {
      eventStatement    << "{ \"" << Table_event       <<  "\": [";
      bins1minStatement << "{ \"" << bins1minStatement <<  "\": [";
   }
   else {
      throw ImporterLogicException("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool NorNetEdgeMetadataReader::finishParsing(DatabaseClientBase& databaseClient,
                                             unsigned long long& rows)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
   Statement& eventStatement    = databaseClient.getStatement("event");
   Statement& bins1minStatement = databaseClient.getStatement("bins1min");
   assert(eventStatement.getRows() + bins1minStatement.getRows() == rows);

   if(rows > 0) {
      if(eventStatement.isValid()) {
         if(backend & DatabaseBackendType::NoSQL_Generic) {
            eventStatement << " \n] }";
         }
         databaseClient.executeUpdate(eventStatement);
      }
      if(bins1minStatement.isValid()) {
         if(backend & DatabaseBackendType::NoSQL_Generic) {
            bins1minStatement << " \n] }";
         }
         databaseClient.executeUpdate(bins1minStatement);
      }
      return true;
   }
   return false;
}


// ###### Parse input file ##################################################
void NorNetEdgeMetadataReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        const std::filesystem::path&         dataFile,
        boost::iostreams::filtering_istream& dataStream)
{
   const DatabaseBackendType backend           = databaseClient.getBackend();
   Statement&                eventStatement    = databaseClient.getStatement("event");
   Statement&                bins1minStatement = databaseClient.getStatement("bins1min");

   try {
      boost::property_tree::ptree propertyTreeRoot;
      boost::property_tree::read_json(dataStream, propertyTreeRoot);
      // dumpPropertyTree(std::cerr, propertyTreeRoot);

#ifdef WITH_TIMESTAMP_FIX
      bool showTimeStampFixWarning = true;
      // std::cout << "-----\n";
#endif
#ifdef WITH_NODEID_FIX
      const unsigned int nodeIDFromPath       = getNodeIDFromPath(dataFile);
      bool               showNodeIDFixWarning = true;
#endif

      // ====== Process all metadata items ==================================
      std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
      for(boost::property_tree::ptree::const_iterator itemIterator = propertyTreeRoot.begin();
         itemIterator != propertyTreeRoot.end(); itemIterator++)  {
         const boost::property_tree::ptree& item = itemIterator->second;
         const std::string& itemType             = item.get<std::string>("type");

#ifndef WITH_NODEID_FIX
         const unsigned int nodeID               = parseNodeID(item, dataFile);
#else
#warning With NodeID fix!
         unsigned int nodeID = parseNodeID(item, dataFile);
         if( (nodeID == 4125) && (nodeID != nodeIDFromPath) ) {
            if(showNodeIDFixWarning) {
               HPCT_LOG(debug) << Identification << ": Bad NodeID fix: " << nodeID << " -> "
                               << nodeIDFromPath << " for "
                               << relative_to(dataFile, Configuration.getImportFilePath());
               showNodeIDFixWarning = false;
            }
            nodeID = nodeIDFromPath;
         }
#endif
#ifndef WITH_TIMESTAMP_FIX
         const ReaderTimePoint ts =
            parseTimeStamp<ReaderTimePoint>(item, now, dataFile);
#else
#warning With timestamp fix!
         ReaderTimePoint ts =
            parseTimeStamp<ReaderTimePoint>(item, now, dataFile);
            const unsigned long long us = timePointToMicroseconds<ReaderTimePoint>(ts);
         if(itemType == "event") {
            if((us % 1000000) == 0) {
               if(showTimeStampFixWarning) {
                  HPCT_LOG(debug) << Identification << ": Applying time stamp fix for "
                                    << relative_to(dataFile, Configuration.getImportFilePath());
                  showTimeStampFixWarning = false;
               }

               TimeStampFix* timeStampFix;
               std::map<unsigned int, TimeStampFix*>::iterator found = TSFixMap.find(nodeID);
               if(found == TSFixMap.end()) {
                  timeStampFix = new TimeStampFix;
                  assert(timeStampFix != nullptr);
                  timeStampFix->TSFixTimeOffset = std::chrono::microseconds(1);
                  const bool success = TSFixMap.insert(std::pair<unsigned int, TimeStampFix*>(nodeID, timeStampFix)).second;
                  assert(success);
               }
               else {
                  timeStampFix = found->second;
               }

               if(ts == timeStampFix->TSFixLastTimePoint) {
                  ts += timeStampFix->TSFixTimeOffset;   // Prevent possible duplicate
                  timeStampFix->TSFixTimeOffset += std::chrono::microseconds(1);
               }
               else {
                  // std::cout << "reset\n";
                  timeStampFix->TSFixLastTimePoint = ts;   // First occurrence of this time stamp
                  timeStampFix->TSFixTimeOffset = std::chrono::microseconds(1);
               }
               // std::cout << "* " << timePointToString<ReaderTimePoint>(ts, 6) << "\n";
            }
         }
#endif
         const unsigned int networkID     = parseNetworkID(item, dataFile);
         const std::string  metadataKey   = parseMetadataKey(item, dataFile);
         const std::string  metadataValue = parseMetadataValue(item, dataFile);

         if(itemType == "event") {
            const ReaderTimePoint min   = makeMin<ReaderTimePoint>(ts);
            const std::string     extra = parseExtra(item, dataFile);
            if(backend & DatabaseBackendType::SQL_Generic) {
               eventStatement.beginRow();
               eventStatement
                  << eventStatement.quote(timePointToString<ReaderTimePoint>(ts, 6)) << eventStatement.sep()
                  << nodeID                                    << eventStatement.sep()
                  << networkID                                 << eventStatement.sep()
                  << eventStatement.quote(metadataKey)         << eventStatement.sep()
                  << eventStatement.quoteOrNull(metadataValue) << eventStatement.sep()
                  << eventStatement.quoteOrNull(extra)         << eventStatement.sep()
                  << eventStatement.quote(timePointToString<ReaderTimePoint>(min));   // FROM_UNIXTIME(UNIX_TIMESTAMP(ts) DIV 60*60)
               eventStatement.endRow();
               rows++;
            }
            else if(backend & DatabaseBackendType::NoSQL_Generic) {
               eventStatement.beginRow();
               eventStatement
                  << "\"ts\":"             << eventStatement.quote(timePointToString<ReaderTimePoint>(ts, 6)) << eventStatement.sep()
                  << "\"node_id\":"        << nodeID                                    << eventStatement.sep()
                  << "\"network_id\":"     << networkID                                 << eventStatement.sep()
                  << "\"metadata_key\":"   << eventStatement.quote(metadataKey)         << eventStatement.sep()
                  << "\"metadata_value\":" << eventStatement.quoteOrNull(metadataValue) << eventStatement.sep()
                  << "\"extra\":"          << eventStatement.quoteOrNull(extra)         << eventStatement.sep()
                  << "\"min\":"            << eventStatement.quote(timePointToString<ReaderTimePoint>(min));   // FROM_UNIXTIME(UNIX_TIMESTAMP(ts) DIV 60*60)
               eventStatement.endRow();
               rows++;
            }
            else {
               throw ImporterLogicException("Unknown output format");
            }
         }
         else if(itemType == "bins-1min") {
            const long long delta = parseDelta(item, dataFile);
            if(backend & DatabaseBackendType::SQL_Generic) {
               bins1minStatement.beginRow();
               bins1minStatement
                  << bins1minStatement.quote(timePointToString<ReaderTimePoint>(ts)) << bins1minStatement.sep()
                  << delta                                << bins1minStatement.sep()
                  << nodeID                               << bins1minStatement.sep()
                  << networkID                            << bins1minStatement.sep()
                  << bins1minStatement.quote(metadataKey) << bins1minStatement.sep()
                  << bins1minStatement.quoteOrNull(metadataValue);
               bins1minStatement.endRow();
               rows++;
            }
            else if(backend & DatabaseBackendType::NoSQL_Generic) {
               eventStatement.beginRow();
               eventStatement
                  << "\"ts\":"             << eventStatement.quote(timePointToString<ReaderTimePoint>(ts)) << eventStatement.sep()
                  << "\"delta\":"          << delta                             << eventStatement.sep()
                  << "\"node_id\":"        << nodeID                            << eventStatement.sep()
                  << "\"network_id\":"     << networkID                         << eventStatement.sep()
                  << "\"metadata_key\":"   << eventStatement.quote(metadataKey) << eventStatement.sep()
                  << "\"metadata_value\":" << eventStatement.quoteOrNull(metadataValue);
               eventStatement.endRow();
               rows++;
            }
            else {
               throw ImporterLogicException("Unknown output format");
            }
         }
         else {
            throw ImporterReaderDataErrorException("Got unknown metadata type " + itemType +
                                                   " in input file " + relative_to(dataFile, Configuration.getImportFilePath()).string());
         }
      }
   }
   catch(const boost::property_tree::ptree_error& exception) {
      throw ImporterReaderDataErrorException("JSON processing failed in input file " +
                                             relative_to(dataFile, Configuration.getImportFilePath()).string() + ": " +
                                             exception.what());
   }
}
