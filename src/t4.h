#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


// ###### Create indentation string #########################################
std::string indent(const unsigned int level, const char* indentation = "\t") {
   std::string string;
   for(unsigned int i = 0; i < level; i++) {
      string += "\t";
   }
   return string;
}


// ###### Dump property tree ################################################
void dumpPropertyTree(std::ostream&                      os,
                      const boost::property_tree::ptree& propertyTree,
                      const unsigned int                 level = 0)
{
   if(propertyTree.empty()) {
      os << "\"" << propertyTree.data() << "\"";
   }
   else {
      os << "{" << std::endl;
      for(boost::property_tree::ptree::const_iterator propertySubTreeIterator = propertyTree.begin();
          propertySubTreeIterator != propertyTree.end(); )  {
         os << indent(level + 1) << "\"" << propertySubTreeIterator->first << "\": ";
         dumpPropertyTree(os, propertySubTreeIterator->second, level + 1);
         propertySubTreeIterator++;
         if(propertySubTreeIterator != propertyTree.end()) {
             os << ",";
         }
         os << std::endl;
      }
      os << indent(level) << "}";
   }
}



class NorNetEdgeMetadataReader : public BasicReader
{
   public:
   NorNetEdgeMetadataReader(const unsigned int workers            = 1,
                            const unsigned int maxTransactionSize = 4,
                            const std::string& table_bins1min     = "node_metadata_bins1min",
                            const std::string& table_event        = "node_metadata_event");
   virtual ~NorNetEdgeMetadataReader();

   virtual const std::string& getIdentification() const;
   virtual const std::regex& getFileNameRegExp() const;

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
                              boost::iostreams::filtering_istream& inputStream);

   private:
   template<typename TimePoint> static TimePoint makeMin(const TimePoint& timePoint);
   template<typename TimePoint> static TimePoint parseTimeStamp(const boost::property_tree::ptree& item,
                                                                const TimePoint&                   now);
   long long parseDelta(const boost::property_tree::ptree& item) const;
   unsigned int parseNodeID(const boost::property_tree::ptree& item) const;
   unsigned int parseNetworkID(const boost::property_tree::ptree& item) const;
   std::string parseMetadataKey(const boost::property_tree::ptree& item) const;
   std::string parseMetadataValue(const boost::property_tree::ptree& item) const;
   std::string parseExtra(const boost::property_tree::ptree& item) const;

   typedef std::chrono::system_clock               FileEntryClock;
   typedef std::chrono::time_point<FileEntryClock> FileEntryTimePoint;
   struct InputFileEntry {
      FileEntryTimePoint    TimeStamp;
      unsigned int          NodeID;
      std::filesystem::path DataFile;
   };
   friend bool operator<(const NorNetEdgeMetadataReader::InputFileEntry& a,
                         const NorNetEdgeMetadataReader::InputFileEntry& b);
   friend std::ostream& operator<<(std::ostream& os, const InputFileEntry& entry);

   static const std::string  Identification;
   static const std::regex   FileNameRegExp;
   const std::string         Table_bins1min;
   const std::string         Table_event;
   std::mutex                Mutex;
   std::set<InputFileEntry>* DataFileSet;
};


const std::string NorNetEdgeMetadataReader::Identification = "Metadata";
const std::regex  NorNetEdgeMetadataReader::FileNameRegExp = std::regex(
   // Format: nne<NodeID>-metadatacollector-<YYYYMMDD>T<HHMMSS>.json
   "^nne([0-9]+)-metadatacollector-([0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]T[0-9][0-9][0-9][0-9][0-9][0-9])\\.json$"
);


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const NorNetEdgeMetadataReader::InputFileEntry& a,
               const NorNetEdgeMetadataReader::InputFileEntry& b)
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
std::ostream& operator<<(std::ostream& os, const NorNetEdgeMetadataReader::InputFileEntry& entry)
{
   os << "("
      << timePointToString<NorNetEdgeMetadataReader::FileEntryTimePoint>(entry.TimeStamp) << ", "
      << entry.NodeID << ", "
      << entry.DataFile
      << ")";
   return os;
}


// ###### Constructor #######################################################
NorNetEdgeMetadataReader::NorNetEdgeMetadataReader(const unsigned int workers,
                                                   const unsigned int maxTransactionSize,
                                                   const std::string& table_bins1min,
                                                   const std::string& table_event)
   : BasicReader(workers, maxTransactionSize),
     Table_bins1min(table_bins1min),
     Table_event(table_event)
{
   // ====== Sanity checks ==================================================
   const unsigned long long t1 = 1666261441000000;
   const unsigned long long t2 = 1000000000000000;
   const unsigned long long t3 = 2000000000000000;
   const unsigned long long t4 = 1000000000123456;
   const std::chrono::time_point<std::chrono::high_resolution_clock> tp1 = microsecondsToTimePoint<std::chrono::time_point<std::chrono::high_resolution_clock>>(t1);
   const std::chrono::time_point<std::chrono::high_resolution_clock> tp2 = microsecondsToTimePoint<std::chrono::time_point<std::chrono::high_resolution_clock>>(t2);
   const std::chrono::time_point<std::chrono::high_resolution_clock> tp3 = microsecondsToTimePoint<std::chrono::time_point<std::chrono::high_resolution_clock>>(t3);
   const std::chrono::time_point<std::chrono::high_resolution_clock> tp4 = microsecondsToTimePoint<std::chrono::time_point<std::chrono::high_resolution_clock>>(t4);
   const std::string ts1 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp1, 0);
   const std::string ts2 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp2, 6);
   const std::string ts3 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp3, 0);
   const std::string ts4 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp4, 6);
   const std::chrono::time_point<std::chrono::high_resolution_clock> dp1 = makeMin<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp1);
   const std::chrono::time_point<std::chrono::high_resolution_clock> dp2 = makeMin<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp2);
   const std::chrono::time_point<std::chrono::high_resolution_clock> dp3 = makeMin<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp3);
   const std::chrono::time_point<std::chrono::high_resolution_clock> dp4 = makeMin<std::chrono::time_point<std::chrono::high_resolution_clock>>(tp4);
   const std::string ds1 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(dp1, 0);
   const std::string ds2 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(dp2, 6);
   const std::string ds3 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(dp3, 0);
   const std::string ds4 = timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(dp4, 6);
/*
   std::cout << t1 << "\t" << ts1 << "\t\t" << ds1 << std::endl;
   std::cout << t2 << "\t" << ts2 << "\t" << ds2 << std::endl;
   std::cout << t3 << "\t" << ts3 << "\t\t" << ds3 << std::endl;
   std::cout << t4 << "\t" << ts4 << "\t" << ds4 << std::endl;
*/

   assert(ts1 == "2022-10-20 10:24:01");
   assert(ts2 == "2001-09-09 01:46:40.000000");
   assert(ts3 == "2033-05-18 03:33:20");
   assert(ts4 == "2001-09-09 01:46:40.123456");

   assert(ds1 == "2022-10-20 10:24:00");
   assert(ds2 == "2001-09-09 01:46:00.000000");
   assert(ds3 == "2033-05-18 03:33:00");
   assert(ds4 == "2001-09-09 01:46:00.000000");

   // ====== Initialise =====================================================
   DataFileSet = new std::set<InputFileEntry>[Workers];
   assert(DataFileSet != nullptr);
}


// ###### Destructor ########################################################
NorNetEdgeMetadataReader::~NorNetEdgeMetadataReader()
{
   delete [] DataFileSet;
   DataFileSet = nullptr;
}


// ###### Get identification of reader ######################################
const std::string& NorNetEdgeMetadataReader::getIdentification() const
{
   return Identification;
}


// ###### Get input file name regular expression ############################
const std::regex& NorNetEdgeMetadataReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


// ###### Add input file to reader ##########################################
int NorNetEdgeMetadataReader::addFile(const std::filesystem::path& dataFile,
                                      const std::smatch            match)
{
   if(match.size() == 3) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[2].str(), timeStamp, "%Y%m%dT%H%M%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.TimeStamp = timeStamp;
         inputFileEntry.NodeID    = atol(match[1].str().c_str());
         inputFileEntry.DataFile  = dataFile;
         const int workerID = inputFileEntry.NodeID % Workers;

         std::unique_lock lock(Mutex);
         if(DataFileSet[workerID].insert(inputFileEntry).second) {
            HPCT_LOG(trace) << Identification << ": Added data file " << dataFile << " to reader";
            TotalFiles++;
            return workerID;
         }
      }
      else {
         HPCT_LOG(warning) << Identification << ": Bad time stamp " << match[2].str();
      }
   }
   return -1;
}


// ###### Remove input file from reader #####################################
bool NorNetEdgeMetadataReader::removeFile(const std::filesystem::path& dataFile,
                                          const std::smatch            match)
{
   if(match.size() == 3) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[2].str(), timeStamp, "%Y%m%dT%H%M%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.TimeStamp = timeStamp;
         inputFileEntry.NodeID    = atol(match[1].str().c_str());
         inputFileEntry.DataFile  = dataFile;
         const int workerID = inputFileEntry.NodeID % Workers;

         HPCT_LOG(trace) << Identification << ": Removing data file " << dataFile << " from reader";
         std::unique_lock lock(Mutex);
         if(DataFileSet[workerID].erase(inputFileEntry) == 1) {
            assert(TotalFiles > 0);
            TotalFiles--;
            return true;
         }
      }
   }
   return false;
}


// ###### Fetch list of input files #########################################
unsigned int NorNetEdgeMetadataReader::fetchFiles(std::list<std::filesystem::path>& dataFileList,
                                                  const unsigned int                worker,
                                                  const unsigned int                limit)
{
   assert(worker < Workers);
   dataFileList.clear();

   std::unique_lock lock(Mutex);

   for(const InputFileEntry& inputFileEntry : DataFileSet[worker]) {
      dataFileList.push_back(inputFileEntry.DataFile);
      if(dataFileList.size() >= limit) {
         break;
      }
   }
   return dataFileList.size();
}


// ###### Begin parsing #####################################################
void NorNetEdgeMetadataReader::beginParsing(DatabaseClientBase& databaseClient,
                                            unsigned long long& rows)
{
   rows = 0;
}


// ###### Finish parsing ####################################################
bool NorNetEdgeMetadataReader::finishParsing(DatabaseClientBase& databaseClient,
                                             unsigned long long& rows)
{
   if(rows > 0) {
      return true;
   }
   return false;
}


// ###### Helper function to calculate "min" value ##########################
template<typename TimePoint> TimePoint NorNetEdgeMetadataReader::makeMin(const TimePoint& timePoint)
{
   unsigned long long us = timePointToMicroseconds<std::chrono::time_point<std::chrono::high_resolution_clock>>(timePoint);
   us = us - (us % (60000000ULL));   // floor to minute
   return  microsecondsToTimePoint<std::chrono::time_point<std::chrono::high_resolution_clock>>(us);
}


// ###### Parse time stamp ##################################################
template<typename TimePoint> TimePoint NorNetEdgeMetadataReader::parseTimeStamp(
                                          const boost::property_tree::ptree& item,
                                          const TimePoint&                   now)
{
   const unsigned long long ts        = (unsigned long long)rint(1000000.0 * item.get<double>("ts"));
   const TimePoint          timeStamp = microsecondsToTimePoint<std::chrono::time_point<std::chrono::high_resolution_clock>>(ts);
   if( (timeStamp < now - std::chrono::hours(365 * 24)) ||   /* 1 year in the past  */
       (timeStamp > now + std::chrono::hours(24)) ) {        /* 1 day in the future */
      throw ImporterReaderException("Bad time stamp " + std::to_string(ts));
   }
   return timeStamp;
}


// ###### Parse delta #######################################################
long long NorNetEdgeMetadataReader::parseDelta(const boost::property_tree::ptree& item) const
{
   const unsigned int delta = round(item.get<double>("delta"));
   if( (delta < 0) || (delta > 4294967295.0) ) {
      throw ImporterReaderException("Bad delta " + delta);
   }
   return delta;
}


// ###### Parse node ID #####################################################
unsigned int NorNetEdgeMetadataReader::parseNodeID(const boost::property_tree::ptree& item) const
{
   const std::string& nodeName = item.get<std::string>("node");
   if(nodeName.substr(0, 3) != "nne") {
      throw ImporterReaderException("Bad node name " + nodeName);
   }
   const unsigned int nodeID = atol(nodeName.substr(3, nodeName.size() -3).c_str());
   if( (nodeID < 1) || (nodeID > 9999) ) {
      throw ImporterReaderException("Bad node ID " + nodeID);
   }
   return nodeID;
}


// ###### Parse network ID ##################################################
unsigned int NorNetEdgeMetadataReader::parseNetworkID(const boost::property_tree::ptree& item) const
{
   const unsigned int networkID = item.get<unsigned int>("network_id");
   if(networkID > 99) {   // MNC is a two-digit number
      throw ImporterReaderException("Bad network ID " + networkID);
   }
   return networkID;
}


// ###### Parse metadata key ################################################
std::string NorNetEdgeMetadataReader::parseMetadataKey(const boost::property_tree::ptree& item) const
{
   const std::string& metadataKey = item.get<std::string>("key");
   if(metadataKey.size() > 45) {
      throw ImporterReaderException("Too long metadata key " + metadataKey);
   }
   return metadataKey;
}


// ###### Parse metadata value ##############################################
std::string NorNetEdgeMetadataReader::parseMetadataValue(const boost::property_tree::ptree& item) const
{
   const std::string& metadataValue = item.get<std::string>("value");
   if(metadataValue.size() > 500) {
      throw ImporterReaderException("Too long metadata value " + metadataValue);
   }
   return metadataValue;
}


// ###### Parse extra data ##################################################
std::string NorNetEdgeMetadataReader::parseExtra(const boost::property_tree::ptree& item) const
{
   const std::string& extra = item.get<std::string>("extra");
   if(extra.size() > 500) {
      throw ImporterReaderException("Too long extra " + extra);
   }
   return extra;
}


// ###### Parse input file ##################################################
void NorNetEdgeMetadataReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        boost::iostreams::filtering_istream& inputStream)
{
   const DatabaseBackendType   backend = databaseClient.getBackend();
   boost::property_tree::ptree propertyTreeRoot;

   try {
      boost::property_tree::read_json(inputStream, propertyTreeRoot);
   }
   catch(const boost::property_tree::json_parser::json_parser_error& exception) {
      throw ImporterReaderDataErrorException(exception.what());
   }

   // dumpPropertyTree(std::cerr, propertyTreeRoot);

   // ====== Process all metadata items =====================================
   std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
   for(boost::property_tree::ptree::const_iterator itemIterator = propertyTreeRoot.begin();
       itemIterator != propertyTreeRoot.end(); itemIterator++)  {
      const boost::property_tree::ptree& item = itemIterator->second;
      const std::string& itemType             = item.get<std::string>("type");

      if(itemType == "bins-1min") {
         const std::chrono::time_point<std::chrono::high_resolution_clock> ts = parseTimeStamp<std::chrono::time_point<std::chrono::high_resolution_clock>>(item, now);
         const long long    delta         = parseDelta(item);
         const unsigned int nodeID        = parseNodeID(item);
         const unsigned int networkID     = parseNetworkID(item);
         const std::string  metadataKey   = parseMetadataKey(item);
         const std::string  metadataValue = parseMetadataValue(item);
         if(backend & DatabaseBackendType::SQL_Generic) {
            assert(databaseClient.statementIsEmpty());
            databaseClient.getStatement()
               << "INSERT INTO " << Table_bins1min
               << "(ts, delta, node_id, network_id, metadata_key, metadata_value) VALUES ("
               << "'" << timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(ts) << "', "
               << delta                << ", "
               << nodeID               << ", "
               << networkID            << ", "
               << "'" << metadataKey   << "', "
               << "'" << metadataValue << "' )\n";
            databaseClient.executeStatement();
            databaseClient.clearStatement();
            rows++;
         }
      }
      else if(itemType == "event") {
         const std::chrono::time_point<std::chrono::high_resolution_clock> ts  = parseTimeStamp<std::chrono::time_point<std::chrono::high_resolution_clock>>(item, now);
         const std::chrono::time_point<std::chrono::high_resolution_clock> min = makeMin<std::chrono::time_point<std::chrono::high_resolution_clock>>(ts);
         const unsigned int nodeID        = parseNodeID(item);
         const unsigned int networkID     = parseNetworkID(item);
         const std::string  metadataKey   = parseMetadataKey(item);
         const std::string  metadataValue = parseMetadataValue(item);
         const std::string  extra         = parseExtra(item);
         if(backend & DatabaseBackendType::SQL_Generic) {
            assert(databaseClient.statementIsEmpty());
            databaseClient.getStatement()
               << "INSERT INTO " << Table_event
               << "(ts, node_id, network_id, metadata_key, metadata_value, extra, min) VALUES ("
               << "'" << timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(ts) << "', "
               << nodeID               << ", "
               << networkID            << ", "
               << "'" << metadataKey   << "', "
               << "'" << metadataValue << "', "
               << "'" << extra         << "', "
               << "'" << timePointToString<std::chrono::time_point<std::chrono::high_resolution_clock>>(min) << "');\n";   // FROM_UNIXTIME(UNIX_TIMESTAMP(ts) DIV 60*60)
            databaseClient.executeStatement();
            databaseClient.clearStatement();
            rows++;
         }
      }
      else {
         throw ImporterReaderException("Got unknown metadata type " + itemType);
      }
   }
}


// ###### Print reader status ###############################################
void NorNetEdgeMetadataReader::printStatus(std::ostream& os)
{
   os << "NorNetEdgeMetadata:" << std::endl;
   for(unsigned int w = 0; w < Workers; w++) {
      os << " - Work Queue #" << w + 1 << ": " << DataFileSet[w].size() << std::endl;
      // for(const InputFileEntry& inputFileEntry : DataFileSet[w]) {
      //    os << "  - " <<  inputFileEntry.DataFile << std::endl;
      // }
   }
}
