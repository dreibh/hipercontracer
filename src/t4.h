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
   virtual void removeFile(const std::filesystem::path& dataFile,
                           const std::smatch            match);
   virtual unsigned int fetchFiles(std::list<const std::filesystem::path*>& dataFileList,
                                   const unsigned int                       worker,
                                   const unsigned int                       limit = 1);
   virtual void printStatus(std::ostream& os = std::cout);

   virtual void beginParsing(std::stringstream&     statement,
                             unsigned long long&    rows,
                             const DatabaseBackend  outputFormat);
   virtual bool finishParsing(std::stringstream&     statement,
                              unsigned long long&    rows,
                              const DatabaseBackend  outputFormat);
   virtual void parseContents(std::stringstream&                   statement,
                              unsigned long long&                  rows,
                              boost::iostreams::filtering_istream& inputStream,
                              const DatabaseBackend                outputFormat);

   private:
   template<class Clock> static std::chrono::time_point<Clock> makeMin(std::chrono::time_point<Clock> timePoint);
   template<class Clock> static std::chrono::time_point<Clock> parseTimeStamp(const boost::property_tree::ptree&         item,
                                                                              const std::chrono::time_point<Clock>& now);
   long long parseDelta(const boost::property_tree::ptree& item) const;
   unsigned int parseNodeID(const boost::property_tree::ptree& item) const;
   unsigned int parseNetworkID(const boost::property_tree::ptree& item) const;
   std::string parseMetadataKey(const boost::property_tree::ptree& item) const;
   std::string parseMetadataValue(const boost::property_tree::ptree& item) const;
   std::string parseExtra(const boost::property_tree::ptree& item) const;

   struct InputFileEntry {
      std::string           TimeStamp;
      unsigned int          NodeID;
      std::filesystem::path DataFile;
   };
   friend bool operator<(const NorNetEdgeMetadataReader::InputFileEntry& a,
                         const NorNetEdgeMetadataReader::InputFileEntry& b);

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
bool operator<(const NorNetEdgeMetadataReader::InputFileEntry& a,
               const NorNetEdgeMetadataReader::InputFileEntry& b) {
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   if(a.NodeID < b.NodeID) {
      return true;
   }
   return false;
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
   const unsigned long long t1 = 1666261441;
   const unsigned long long t2 = 1000000000;
   const unsigned long long t3 = 2000000000;
   const std::chrono::time_point<std::chrono::system_clock> tp1 = microsecondsToTimePoint<std::chrono::system_clock>(1000000ULL * t1);
   const std::chrono::time_point<std::chrono::system_clock> tp2 = microsecondsToTimePoint<std::chrono::system_clock>(1000000ULL * t2);
   const std::chrono::time_point<std::chrono::system_clock> tp3 = microsecondsToTimePoint<std::chrono::system_clock>(1000000ULL * t3);
   const std::string ts1 = timePointToUTCTimeString<std::chrono::system_clock>(tp1);
   const std::string ts2 = timePointToUTCTimeString<std::chrono::system_clock>(tp2);
   const std::string ts3 = timePointToUTCTimeString<std::chrono::system_clock>(tp3);
   const std::chrono::time_point<std::chrono::system_clock> dp1 = makeMin<std::chrono::system_clock>(tp1);
   const std::chrono::time_point<std::chrono::system_clock> dp2 = makeMin<std::chrono::system_clock>(tp2);
   const std::chrono::time_point<std::chrono::system_clock> dp3 = makeMin<std::chrono::system_clock>(tp3);
   const std::string ds1 = timePointToUTCTimeString<std::chrono::system_clock>(dp1);
   const std::string ds2 = timePointToUTCTimeString<std::chrono::system_clock>(dp2);
   const std::string ds3 = timePointToUTCTimeString<std::chrono::system_clock>(dp3);

   // std::cout << t1 << "\t" << ts1 << "\t" << ds1 << std::endl;
   // std::cout << t2 << "\t" << ts2 << "\t" << ds2 << std::endl;
   // std::cout << t3 << "\t" << ts3 << "\t" << ds3 << std::endl;

   assert(ts1 == "2022-10-20 10:24:01");
   assert(ts2 == "2001-09-09 01:46:40");
   assert(ts3 == "2033-05-18 03:33:20");

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
      InputFileEntry inputFileEntry;
      inputFileEntry.TimeStamp  = match[2];
      inputFileEntry.NodeID     = atol(match[1].str().c_str());
      inputFileEntry.DataFile   = dataFile;
      const unsigned int worker = inputFileEntry.NodeID % Workers;

      HPCT_LOG(trace) << Identification << ": Adding data file " << dataFile;
      std::unique_lock lock(Mutex);
      DataFileSet[worker].insert(inputFileEntry);

      return (int)worker;
   }
   return -1;
}


// ###### Remove input file from reader #####################################
void NorNetEdgeMetadataReader::removeFile(const std::filesystem::path& dataFile,
                                          const std::smatch            match)
{
   assert(false);
   if(match.size() == 3) {
      InputFileEntry inputFileEntry;
      inputFileEntry.TimeStamp  = match[2];
      inputFileEntry.NodeID     = atol(match[1].str().c_str());
      inputFileEntry.DataFile   = dataFile;
      const unsigned int worker = inputFileEntry.NodeID % Workers;

      HPCT_LOG(trace) << Identification << ": Removing data file " << dataFile;
      std::unique_lock lock(Mutex);
      DataFileSet[worker].erase(inputFileEntry);
   }
}


// ###### Fetch list of input files #########################################
unsigned int NorNetEdgeMetadataReader::fetchFiles(std::list<const std::filesystem::path*>& dataFileList,
                                                  const unsigned int                       worker,
                                                  const unsigned int                       limit)
{
   assert(worker < Workers);

   std::unique_lock lock(Mutex);

   unsigned int n = 0;
   for(const InputFileEntry& inputFileEntry : DataFileSet[worker]) {
      dataFileList.push_back(&inputFileEntry.DataFile);
      n++;
      if(n >= limit) {
         break;
      }
   }
   return n;
}


// ###### Begin parsing #####################################################
void NorNetEdgeMetadataReader::beginParsing(std::stringstream&  statement,
                                        unsigned long long& rows,
                                        const DatabaseBackend  outputFormat)
{
   rows = 0;
   statement.str(std::string());
}


// ###### Finish parsing ####################################################
bool NorNetEdgeMetadataReader::finishParsing(std::stringstream&  statement,
                                         unsigned long long& rows,
                                         const DatabaseBackend  outputFormat)
{
   if(rows > 0) {
      return true;
   }
   statement.str(std::string());
   return false;
}


// ###### Helper function to calculate "min" value ##########################
template<class Clock> std::chrono::time_point<Clock> NorNetEdgeMetadataReader::makeMin(std::chrono::time_point<Clock> timePoint)
{
   unsigned long long us = timePointToMicroseconds<Clock>(timePoint);
   us = us - (us % (60000000ULL));   // floor to minute
   return  microsecondsToTimePoint<Clock>(us);
}


// ###### Parse time stamp ##################################################
template<class Clock> std::chrono::time_point<Clock> NorNetEdgeMetadataReader::parseTimeStamp(
                                                        const boost::property_tree::ptree&         item,
                                                        const std::chrono::time_point<Clock>& now)
{
   const unsigned long long             ts        = (unsigned long long)rint(1000000.0 * item.get<double>("ts"));
   const std::chrono::time_point<Clock> timeStamp = microsecondsToTimePoint<Clock>(ts);
   if( (timeStamp < now - std::chrono::hours(365 * 24)) ||   /* 1 year in the past  */
       (timeStamp > now + std::chrono::hours(24)) ) {        /* 1 day in the future */
      throw std::invalid_argument("Bad time stamp " + std::to_string(ts));
   }
   return timeStamp; // std::to_string(ts); // timePointToUTCTimeString<Clock>(timeStamp);
}


// ###### Parse delta #######################################################
long long NorNetEdgeMetadataReader::parseDelta(const boost::property_tree::ptree& item) const
{
   const unsigned int delta = round(item.get<double>("delta"));
   if( (delta < 0) || (delta > 4294967295.0) ) {
      throw std::invalid_argument("Bad delta " + delta);
   }
   return delta;
}


// ###### Parse node ID #####################################################
unsigned int NorNetEdgeMetadataReader::parseNodeID(const boost::property_tree::ptree& item) const
{
   const std::string& nodeName = item.get<std::string>("node");
   if(nodeName.substr(0, 3) != "nne") {
      throw std::invalid_argument("Bad node name " + nodeName);
   }
   const unsigned int nodeID = atol(nodeName.substr(3, nodeName.size() -3).c_str());
   if( (nodeID < 1) || (nodeID > 9999) ) {
      throw std::invalid_argument("Bad node ID " + nodeID);
   }
   return nodeID;
}


// ###### Parse network ID ##################################################
unsigned int NorNetEdgeMetadataReader::parseNetworkID(const boost::property_tree::ptree& item) const
{
   const unsigned int networkID = item.get<unsigned int>("network_id");
   if(networkID > 99) {   // MNC is a two-digit number
      throw std::invalid_argument("Bad network ID " + networkID);
   }
   return networkID;
}


// ###### Parse metadata key ################################################
std::string NorNetEdgeMetadataReader::parseMetadataKey(const boost::property_tree::ptree& item) const
{
   const std::string& metadataKey = item.get<std::string>("key");
   if(metadataKey.size() > 45) {
      throw std::invalid_argument("Too long metadata key " + metadataKey);
   }
   return metadataKey;
}


// ###### Parse metadata value ##############################################
std::string NorNetEdgeMetadataReader::parseMetadataValue(const boost::property_tree::ptree& item) const
{
   const std::string& metadataValue = item.get<std::string>("value");
   if(metadataValue.size() > 500) {
      throw std::invalid_argument("Too long metadata value " + metadataValue);
   }
   return metadataValue;
}


// ###### Parse extra data ##################################################
std::string NorNetEdgeMetadataReader::parseExtra(const boost::property_tree::ptree& item) const
{
   const std::string& extra = item.get<std::string>("extra");
   if(extra.size() > 500) {
      throw std::invalid_argument("Too long extra " + extra);
   }
   return extra;
}


// ###### Parse input file ##################################################
void NorNetEdgeMetadataReader::parseContents(
        std::stringstream&                   statement,
        unsigned long long&                  rows,
        boost::iostreams::filtering_istream& inputStream,
        const DatabaseBackend                   outputFormat)
{
   boost::property_tree::ptree propertyTreeRoot;
   boost::property_tree::read_json(inputStream, propertyTreeRoot);

   // dumpPropertyTree(std::cerr, propertyTreeRoot);

   // ====== Process all metadata items =====================================
   std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
   for(boost::property_tree::ptree::const_iterator itemIterator = propertyTreeRoot.begin();
       itemIterator != propertyTreeRoot.end(); itemIterator++)  {
      const boost::property_tree::ptree& item = itemIterator->second;
      const std::string& itemType             = item.get<std::string>("type");

      if(itemType == "bins-1min") {
         const std::chrono::time_point<std::chrono::system_clock> ts = parseTimeStamp<std::chrono::system_clock>(item, now);
         const long long    delta         = parseDelta(item);
         const unsigned int nodeID        = parseNodeID(item);
         const unsigned int networkID     = parseNetworkID(item);
         const std::string  metadataKey   = parseMetadataKey(item);
         const std::string  metadataValue = parseMetadataValue(item);
         if(outputFormat & DatabaseBackend::SQL_Generic) {
            statement << "INSERT INTO " << Table_bins1min
                      << "(ts, delta, node_id, network_id, metadata_key, metadata_value) VALUES ("
                      << "\"" << timePointToUTCTimeString<std::chrono::system_clock>(ts) << "\", "
                      << delta                 << ", "
                      << nodeID                << ", "
                      << networkID             << ", "
                      << "\"" << metadataKey   << "\", "
                      << "\"" << metadataValue << "\" );" << std::endl;
            rows++;
         }
      }
      else if(itemType == "event") {
         const std::chrono::time_point<std::chrono::system_clock> ts  = parseTimeStamp<std::chrono::system_clock>(item, now);
         const std::chrono::time_point<std::chrono::system_clock> min = makeMin<std::chrono::system_clock>(ts);
         const unsigned int nodeID        = parseNodeID(item);
         const unsigned int networkID     = parseNetworkID(item);
         const std::string  metadataKey   = parseMetadataKey(item);
         const std::string  metadataValue = parseMetadataValue(item);
         const std::string  extra         = parseExtra(item);
         if(outputFormat & DatabaseBackend::SQL_Generic) {
            statement << "INSERT INTO " << Table_event
                      << "(ts, node_id, network_id, metadata_key, metadata_value, extra, min) VALUES ("
                      << "\"" << timePointToUTCTimeString<std::chrono::system_clock>(ts) << "\", "
                      << nodeID                << ", "
                      << networkID             << ", "
                      << "\"" << metadataKey   << "\", "
                      << "\"" << metadataValue << "\", "
                      << "\"" << extra         << "\", "
                      << "\"" << timePointToUTCTimeString<std::chrono::system_clock>(min) << "\");"   // FROM_UNIXTIME(UNIX_TIMESTAMP(ts) DIV 60*60)
                      << std::endl;
            rows++;
         }
      }
      else {
         throw std::invalid_argument("Got unknown metadata type " + itemType);
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
