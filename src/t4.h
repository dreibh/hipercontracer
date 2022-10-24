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
                            const unsigned int maxTransactionSize = 4);
   ~NorNetEdgeMetadataReader();

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

   virtual void beginParsing(std::stringstream&  statement,
                             unsigned long long& rows,
                             const DatabaseType  outputFormat);
   virtual bool finishParsing(std::stringstream&  statement,
                              unsigned long long& rows,
                              const DatabaseType  outputFormat);
   virtual void parseContents(std::stringstream&                   statement,
                              unsigned long long&                  rows,
                              boost::iostreams::filtering_istream& inputStream,
                              const DatabaseType                   outputFormat);

   private:
   std::string parseTimeStamp(const boost::property_tree::ptree&           item,
                              const std::chrono::system_clock::time_point& now) const;
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
   std::mutex                Mutex;
   std::set<InputFileEntry>* DataFileSet;
};


const std::string  NorNetEdgeMetadataReader::Identification = "Metadata";
const std::regex NorNetEdgeMetadataReader::FileNameRegExp = std::regex(
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
                                                   const unsigned int maxTransactionSize)
   : BasicReader(workers, maxTransactionSize)
{
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
                                        const DatabaseType  outputFormat)
{
   rows = 0;
   statement.str(std::string());

//    // ====== Generate import statement ======================================
//    if(outputFormat & DatabaseType::SQL_Generic) {
//       statement << "INSERT INTO measurement_generic_data ("
//                    "ts, mi_id, seq, xml_data, crc, stats"
//                    ") VALUES (\n";
//    }
//    else {
//       throw std::invalid_argument("Unknown output format");
//    }
}


// ###### Finish parsing ####################################################
bool NorNetEdgeMetadataReader::finishParsing(std::stringstream&  statement,
                                         unsigned long long& rows,
                                         const DatabaseType  outputFormat)
{
   if(rows > 0) {
//       // ====== Generate import statement ===================================
//       if(outputFormat & DatabaseType::SQL_Generic) {
//          if(rows > 0) {
//             statement << "\n) ON DUPLICATE KEY UPDATE stats=stats;\n";
//          }
//       }
//       else {
//          throw std::invalid_argument("Unknown output format");
//       }
      return true;
   }
   statement.str(std::string());
   return false;
}


std::string timepoint_to_string(const std::chrono::system_clock::time_point& time, const std::string& format)
{
    const std::time_t tt = std::chrono::system_clock::to_time_t(time);
    const std::tm     tm = *std::gmtime(&tt);
    std::stringstream ss;
    ss << std::put_time( &tm, format.c_str());
    return ss.str();
}


// ###### Parse time stamp ##################################################
std::string NorNetEdgeMetadataReader::parseTimeStamp(const boost::property_tree::ptree&           item,
                                                     const std::chrono::system_clock::time_point& now) const
{
   const unsigned int ts = item.get<unsigned int>("ts");
   const std::chrono::system_clock::time_point timeStamp = std::chrono::system_clock::from_time_t(ts);
   if( (timeStamp < now - std::chrono::hours(365 * 24)) ||   /* 1 year in the past  */
       (timeStamp > now + std::chrono::hours(24)) ) {        /* 1 day in the future */
      throw std::invalid_argument("Bad time stamp " + ts);
   }
   // return timepoint_to_string(timeStamp, " %Y-%m-%d %H:%M:%S");
   return std::to_string(ts);
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
        const DatabaseType                   outputFormat)
{
   boost::property_tree::ptree propertyTreeRoot;
   boost::property_tree::read_json(inputStream, propertyTreeRoot);

   dumpPropertyTree(std::cerr, propertyTreeRoot);

   // ====== Process all metadata items =====================================
   std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
   for(boost::property_tree::ptree::const_iterator itemIterator = propertyTreeRoot.begin();
       itemIterator != propertyTreeRoot.end(); itemIterator++)  {
      const boost::property_tree::ptree& item = itemIterator->second;
      const std::string& itemType             = item.get<std::string>("type");

      if(itemType == "bins-1min") {
         const std::string ts             = parseTimeStamp(item, now);
         const long long    delta         = parseDelta(item);
         const unsigned int nodeID        = parseNodeID(item);
         const unsigned int networkID     = parseNetworkID(item);
         const std::string  metadataKey   = parseMetadataKey(item);
         const std::string  metadataValue = parseMetadataValue(item);
         if(outputFormat & DatabaseType::SQL_Generic) {
            statement << "INSERT INTO node_metadata_bins1min (ts, delta, node_id, network_id, metadata_key, metadata_value) VALUES ("
                      << "\"" << ts << "\", " << delta << ", " << nodeID << ", " << networkID << ", "
                      << "\"" << metadataKey   << "\", "
                      << "\"" << metadataValue << "\" );" << std::endl;
            rows++;
         }
      }
      else if(itemType == "event") {
         const std::string ts             = parseTimeStamp(item, now);
         const unsigned int nodeID        = parseNodeID(item);
         const unsigned int networkID     = parseNetworkID(item);
         const std::string  metadataKey   = parseMetadataKey(item);
         const std::string  metadataValue = parseMetadataValue(item);
         const std::string  extra         = parseExtra(item);
         if(outputFormat & DatabaseType::SQL_Generic) {
            statement << "INSERT INTO node_metadata_event (ts, node_id, network_id, metadata_key, metadata_value, extra, min) VALUES ("
                      << "\"" << ts << "\", " << nodeID << ", " << networkID << ", "
                      << "\"" << metadataKey   << "\", "
                      << "\"" << metadataValue << "\", "
                      << "\"" << extra         << "\", "
                      << "!!! TIMESTAMP *TBD* !!! );" << std::endl;
            rows++;
         }
      }
      else {
         throw std::invalid_argument("Got unknown metadata type " + itemType);
      }
      std::cerr << "c=" << itemType << "\n";
   }



   std::cerr << statement.str();
   exit(1);

//    static const unsigned int NorNetEdgeMetadataColumns   = 4;
//    static const char         NorNetEdgeMetadataDelimiter = '\t';
//
//
//
//    std::string inputLine;
//    std::string tuple[NorNetEdgeMetadataColumns];
//    while(std::getline(inputStream, inputLine)) {
//       // ====== Parse line ==================================================
//       size_t columns = 0;
//       size_t start;
//       size_t end = 0;
//       while((start = inputLine.find_first_not_of(NorNetEdgeMetadataDelimiter, end)) != std::string::npos) {
//          end = inputLine.find(NorNetEdgeMetadataDelimiter, start);
//
//          if(columns == NorNetEdgeMetadataColumns) {
//             throw std::invalid_argument("Too many columns in input file");
//          }
//          tuple[columns++] = inputLine.substr(start, end - start);
//       }
//       if(columns != NorNetEdgeMetadataColumns) {
//          throw std::invalid_argument("Too few columns in input file");
//       }
/*
      // ====== Generate import statement ===================================
      if(outputFormat & DatabaseType::SQL_Generic) {
//          if(rows > 0) {
//             statement << ",\n";
//          }
//          statement << " ("
//                    << "'" << tuple[0] << "', "
//                    << std::stoul(tuple[1]) << ", "
//                    << std::stoul(tuple[2]) << ", "
//                    << "'" << tuple[3] << "', crc32(xml_data), 10 + mi_id MOD 10)";
//          rows++;
      }
      else {
         throw std::invalid_argument("Unknown output format");
      }*/
//    }
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
