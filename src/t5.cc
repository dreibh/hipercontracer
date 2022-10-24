#include "logger.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <list>
#include <mutex>
#include <vector>
#include <thread>
#include <cassert>
#include <condition_variable>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>

#include <unistd.h>
#include <sys/signalfd.h>
#include <sys/inotify.h>


// g++ t5.cc -o t5 -std=c++17
// g++ t5.cc -o t5 -std=c++17 -O0 -g -Wall -lpthread && rm -f core && ./t5


enum DatabaseType {
   SQL_Generic    = (1 << 0),
   NoSQL_Generic  = (1 << 1),

   SQL_MariaDB    = SQL_Generic | (1 << 16),
   SQL_PostgreSQL = SQL_Generic | (1 << 17),
   SQL_Cassandra  = SQL_Generic | (1 << 18),

   NoSQL_MongoDB  = NoSQL_Generic | (1 << 24)
};


class DatabaseClientBase
{
   public:
   virtual const DatabaseType getType() const = 0;
   virtual void beginTransaction() = 0;
   virtual void execute(const std::string& statement) = 0;
   virtual void endTransaction(const bool commit) = 0;

   inline void commit()   { endTransaction(true);  }
   inline void rollback() { endTransaction(false); }
};



class MariaDBClient : public virtual  DatabaseClientBase
{
   public:
   MariaDBClient();
   virtual ~MariaDBClient();

   virtual const DatabaseType getType() const;
   virtual void beginTransaction();
   virtual void execute(const std::string& statement);
   virtual void endTransaction(const bool commit);
};


// ###### Constructor #######################################################
MariaDBClient::MariaDBClient()
{
}


// ###### Destructor ########################################################
MariaDBClient::~MariaDBClient()
{
}


const DatabaseType MariaDBClient::getType() const
{
   return DatabaseType::SQL_MariaDB;
}


void MariaDBClient::beginTransaction()
{
}


void MariaDBClient::endTransaction(const bool commit)
{
}


void MariaDBClient::execute(const std::string& statement)
{
   std::cout << "S=" << statement << "\n";
   throw std::runtime_error("TEST EXCEPTION!");
}



class BasicReader
{
   public:
   BasicReader(const unsigned int workers,
               const unsigned int maxTransactionSize);
   ~BasicReader();

   virtual const std::string& getIdentification() const = 0;
   virtual const std::regex& getFileNameRegExp() const = 0;

   virtual int addFile(const std::filesystem::path& dataFile,
                       const std::smatch            match) = 0;
   virtual unsigned int fetchFiles(std::list<const std::filesystem::path*>& dataFileList,
                                   const unsigned int                       worker,
                                   const unsigned int                       limit = 1) = 0;
   virtual void printStatus(std::ostream& os = std::cout) = 0;

   virtual void beginParsing(std::stringstream&  statement,
                             unsigned long long& rows,
                             const DatabaseType  outputFormat) = 0;
   virtual bool finishParsing(std::stringstream&  statement,
                              unsigned long long& rows,
                              const DatabaseType  outputFormat) = 0;
   virtual void parseContents(std::stringstream&                   statement,
                              unsigned long long&                  rows,
                              boost::iostreams::filtering_istream& inputStream,
                              const DatabaseType                   outputFormat) = 0;

   inline const unsigned int getWorkers() const { return Workers; }
   inline const unsigned int getMaxTransactionSize() const { return MaxTransactionSize; }

   protected:
   const unsigned int Workers;
   const unsigned int MaxTransactionSize;
};


// ###### Constructor #######################################################
BasicReader::BasicReader(const unsigned int workers,
                         const unsigned int maxTransactionSize)
   : Workers(workers),
     MaxTransactionSize(maxTransactionSize)
{
   assert(Workers > 0);
   assert(MaxTransactionSize > 0);
}


// ###### Destructor ########################################################
BasicReader::~BasicReader()
{
}



#if 0
class HiPerConTracerPingReader : public BasicReader
{
   public:
   HiPerConTracerPingReader();
   ~HiPerConTracerPingReader();

   virtual const std::regex& getFileNameRegExp() const;
   virtual void addFile(const std::filesystem::path& dataFile,
                        const std::smatch            match);
   virtual void printStatus(std::ostream& os = std::cout);

   private:
   struct InputFileEntry {
      std::string           Source;
      std::string           TimeStamp;
      unsigned int          SeqNumber;
      std::filesystem::path DataFile;
   };
   friend bool operator<(const HiPerConTracerPingReader::InputFileEntry& a,
                         const HiPerConTracerPingReader::InputFileEntry& b);

   std::set<InputFileEntry> inputFileSet;
   static const std::regex  FileNameRegExp;
};


const std::regex HiPerConTracerPingReader::FileNameRegExp = std::regex(
   // Format: Ping-<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Ping-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


bool operator<(const HiPerConTracerPingReader::InputFileEntry& a,
               const HiPerConTracerPingReader::InputFileEntry& b) {
   if(a.Source < b.Source) {
      return true;
   }
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   if(a.SeqNumber < b.SeqNumber) {
      return true;
   }
   return false;
}


// ###### Constructor #######################################################
HiPerConTracerPingReader::HiPerConTracerPingReader()
{
}


// ###### Destructor ########################################################
HiPerConTracerPingReader::~HiPerConTracerPingReader()
{
}


const std::regex& HiPerConTracerPingReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


void HiPerConTracerPingReader::addFile(const std::filesystem::path& dataFile,
                                       const std::smatch            match)
{
   std::cout << dataFile << " s=" << match.size() << std::endl;
   if(match.size() == 5) {
      InputFileEntry inputFileEntry;
      inputFileEntry.Source    = match[2];
      inputFileEntry.TimeStamp = match[3];
      inputFileEntry.SeqNumber = atol(match[4].str().c_str());

      std::cout << "  s=" << inputFileEntry.Source << std::endl;
      std::cout << "  t=" << inputFileEntry.TimeStamp << std::endl;
      std::cout << "  q=" << inputFileEntry.SeqNumber << std::endl;
      inputFileEntry.DataFile  = dataFile;

      inputFileSet.insert(inputFileEntry);
   }
}


class HiPerConTracerTracerouteReader : public HiPerConTracerPingReader
{
   public:
   HiPerConTracerTracerouteReader();
   ~HiPerConTracerTracerouteReader();

   virtual const std::regex& getFileNameRegExp() const;

   private:
   static const std::regex TracerouteFileNameRegExp;
};


const std::regex HiPerConTracerTracerouteReader::TracerouteFileNameRegExp = std::regex(
   // Format: Traceroute-<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Traceroute-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


HiPerConTracerTracerouteReader::HiPerConTracerTracerouteReader()
{
}


// ###### Destructor ########################################################
HiPerConTracerTracerouteReader::~HiPerConTracerTracerouteReader()
{
}


const std::regex& HiPerConTracerTracerouteReader::getFileNameRegExp() const
{
   return(TracerouteFileNameRegExp);
}

#endif



class NorNetEdgePingReader : public BasicReader
{
   public:
   NorNetEdgePingReader(const unsigned int workers            = 1,
                        const unsigned int maxTransactionSize = 4);
   ~NorNetEdgePingReader();

   virtual const std::string& getIdentification() const;
   virtual const std::regex& getFileNameRegExp() const;

   virtual int addFile(const std::filesystem::path& dataFile,
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
   struct InputFileEntry {
      std::string           TimeStamp;
      unsigned int          MeasurementID;
      std::filesystem::path DataFile;
   };
   friend bool operator<(const NorNetEdgePingReader::InputFileEntry& a,
                         const NorNetEdgePingReader::InputFileEntry& b);

   static const std::string  Identification;
   static const std::regex   FileNameRegExp;
   std::mutex                Mutex;
   std::set<InputFileEntry>* DataFileSet;
};


const std::string  NorNetEdgePingReader::Identification = "UDPPing";
const std::regex NorNetEdgePingReader::FileNameRegExp = std::regex(
   // Format: uping_<MeasurementID>.dat.<YYYY-MM-DD-HH-MM-SS>.xz
   "^uping_([0-9]+)\\.dat\\.([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9][0-9]-[0-9][0-9]-[0-9][0-9])\\.xz$"
);


// ###### < operator for sorting ############################################
bool operator<(const NorNetEdgePingReader::InputFileEntry& a,
               const NorNetEdgePingReader::InputFileEntry& b) {
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   if(a.MeasurementID < b.MeasurementID) {
      return true;
   }
   return false;
}


// ###### Constructor #######################################################
NorNetEdgePingReader::NorNetEdgePingReader(const unsigned int workers,
                                           const unsigned int maxTransactionSize)
   : BasicReader(workers, maxTransactionSize)
{
   DataFileSet = new std::set<InputFileEntry>[Workers];
   assert(DataFileSet != nullptr);
}


// ###### Destructor ########################################################
NorNetEdgePingReader::~NorNetEdgePingReader()
{
   delete [] DataFileSet;
   DataFileSet = nullptr;
}


// ###### Get identification of reader ######################################
const std::string& NorNetEdgePingReader::getIdentification() const
{
   return Identification;
}


// ###### Get input file name regular expression ############################
const std::regex& NorNetEdgePingReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


// ###### Add input file to reader ##########################################
int NorNetEdgePingReader::addFile(const std::filesystem::path& dataFile,
                                  const std::smatch            match)
{
   if(match.size() == 3) {
      InputFileEntry inputFileEntry;
      inputFileEntry.MeasurementID = atol(match[1].str().c_str());
      inputFileEntry.TimeStamp     = match[2];
      inputFileEntry.DataFile      = dataFile;
      const unsigned int worker = inputFileEntry.MeasurementID % Workers;

      std::unique_lock lock(Mutex);
      DataFileSet[worker].insert(inputFileEntry);

      return (int)worker;
   }
   return -1;
}


// ###### Fetch list of input files #########################################
unsigned int NorNetEdgePingReader::fetchFiles(std::list<const std::filesystem::path*>& dataFileList,
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
void NorNetEdgePingReader::beginParsing(std::stringstream&  statement,
                                        unsigned long long& rows,
                                        const DatabaseType  outputFormat)
{
   rows = 0;
   statement.str(std::string());

   // ====== Generate import statement ======================================
   if(outputFormat & DatabaseType::SQL_Generic) {
      statement << "INSERT INTO measurement_generic_data ("
                   "ts, mi_id, seq, xml_data, crc, stats"
                   ") VALUES (\n";
   }
   else {
      throw std::runtime_error("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool NorNetEdgePingReader::finishParsing(std::stringstream&  statement,
                                         unsigned long long& rows,
                                         const DatabaseType  outputFormat)
{
   if(rows > 0) {
      // ====== Generate import statement ===================================
      if(outputFormat & DatabaseType::SQL_Generic) {
         if(rows > 0) {
            statement << "\n) ON DUPLICATE KEY UPDATE stats=stats;\n";
         }
      }
      else {
         throw std::runtime_error("Unknown output format");
      }
      return true;
   }
   statement.str(std::string());
   return false;
}


// ###### Parse input file ##################################################
void NorNetEdgePingReader::parseContents(
        std::stringstream&                   statement,
        unsigned long long&                  rows,
        boost::iostreams::filtering_istream& inputStream,
        const DatabaseType                   outputFormat)
{
   static const unsigned int NorNetEdgePingColumns   = 4;
   static const char         NorNetEdgePingDelimiter = '\t';

   std::string inputLine;
   std::string tuple[NorNetEdgePingColumns];
   while(std::getline(inputStream, inputLine)) {
      // ====== Parse line ==================================================
      size_t columns = 0;
      size_t start;
      size_t end = 0;
      while((start = inputLine.find_first_not_of(NorNetEdgePingDelimiter, end)) != std::string::npos) {
         end = inputLine.find(NorNetEdgePingDelimiter, start);

         if(columns == NorNetEdgePingColumns) {
            throw std::range_error("Too many columns in input file");
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns != NorNetEdgePingColumns) {
         throw std::range_error("Too few columns in input file");
      }

      // ====== Generate import statement ===================================
      if(outputFormat & DatabaseType::SQL_Generic) {
         if(rows > 0) {
            statement << ",\n";
         }
         statement << " ("
                   << "'" << tuple[0] << "', "
                   << std::stoul(tuple[1]) << ", "
                   << std::stoul(tuple[2]) << ", "
                   << "'" << tuple[3] << "', crc32(xml_data), 10 + mi_id MOD 10)";
         rows++;
      }
      else {
         throw std::runtime_error("Unknown output format");
      }
   }
}


void NorNetEdgePingReader::printStatus(std::ostream& os)
{
   os << "NorNetEdgePing:" << std::endl;
   for(unsigned int w = 0; w < Workers; w++) {
      os << " - Work Queue #" << w + 1 << ": " << DataFileSet[w].size() << std::endl;
      // for(const InputFileEntry& inputFileEntry : DataFileSet[w]) {
      //    os << "  - " <<  inputFileEntry.DataFile << std::endl;
      // }
   }
}



class Worker
{
   public:
   Worker(const unsigned int  workerID,
          BasicReader*        reader,
          DatabaseClientBase* databaseClient);
   ~Worker();

   void wakeUp();

   inline const std::string& getIdentification() const { return Identification; }


   private:
   void processFile(std::stringstream&           statement,
                    unsigned long long&          rows,
                    const std::filesystem::path* dataFile);
   void finishedFile(const std::filesystem::path& dataFile);
   void run();

   const unsigned int      WorkerID;
   BasicReader*            Reader;
   DatabaseClientBase*     DatabaseClient;
   const std::string       Identification;
   std::thread             Thread;
   std::mutex              Mutex;
   std::condition_variable Notification;
};


// ###### Constructor #######################################################
Worker::Worker(const unsigned int  workerID,
               BasicReader*        reader,
               DatabaseClientBase* databaseClient)
   : WorkerID(workerID),
     Reader(reader),
     DatabaseClient(databaseClient),
     Identification(reader->getIdentification() + "/" + std::to_string(WorkerID))
{
   Thread = std::thread(&Worker::run, this);
}


// ###### Destructor ########################################################
Worker::~Worker()
{
   Mutex.lock();
   Reader = nullptr;
   Mutex.unlock();
   wakeUp();
   Thread.join();
}


// ###### Wake up worker loop ###############################################
void Worker::wakeUp()
{
   Notification.notify_one();
}


// ###### Get list of input files ###########################################
void Worker::processFile(std::stringstream&           statement,
                         unsigned long long&          rows,
                         const std::filesystem::path* dataFile)
{
   std::ifstream                       inputFile;
   boost::iostreams::filtering_istream inputStream;

   // ====== Prepare input stream ===========================================
   inputFile.open(dataFile->string(), std::ios_base::in | std::ios_base::binary);
   if(dataFile->extension() == ".xz") {
      inputStream.push(boost::iostreams::lzma_decompressor());
   }
   else if(dataFile->extension() == ".bz2") {
      inputStream.push(boost::iostreams::bzip2_decompressor());
   }
   else if(dataFile->extension() == ".gz") {
      inputStream.push(boost::iostreams::gzip_decompressor());
   }
   inputStream.push(inputFile);

   // ====== Read contents ==================================================
   Reader->parseContents(statement, rows, inputStream, DatabaseClient->getType());
}


// ====== Delete finished input file ========================================
void Worker::finishedFile(const std::filesystem::path& dataFile)
{
   HPCT_LOG(trace) << "Deleting " << dataFile;
}


// ###### Worker loop #######################################################
void Worker::run()
{
   std::unique_lock lock(Mutex);

   std::cout << getIdentification() << ": sleeping ..." << std::endl;
   Notification.wait(lock);
   while(Reader != nullptr) {
      // ====== Look for new input files ====================================
      HPCT_LOG(trace) << getIdentification() << ": Looking for new input files ...";
      std::list<const std::filesystem::path*> dataFileList;
      const unsigned int files = Reader->fetchFiles(dataFileList, WorkerID, Reader->getMaxTransactionSize());

      // ====== Fast import: try to combine files ===========================
      if(files > 0) {
         HPCT_LOG(debug) << getIdentification() << ": Trying to import " << files << " files in fast mode ...";

         std::stringstream  statement;
         unsigned long long rows = 0;
         try {
            // ====== Import multiple input files in one transaction ========
            Reader->beginParsing(statement, rows, DatabaseClient->getType());
            for(const std::filesystem::path* dataFile : dataFileList) {
               HPCT_LOG(trace) << getIdentification() << ": Parsing " << *dataFile << " ...";
               processFile(statement, rows, dataFile);
            }
            if(Reader->finishParsing(statement, rows, DatabaseClient->getType())) {
               DatabaseClient->beginTransaction();
               DatabaseClient->execute(statement.str());
               DatabaseClient->commit();
               HPCT_LOG(debug) << getIdentification() << ": Committed " << rows << " rows";
            }
            else {
               std::cout << "Nothing to do!" << std::endl;
               HPCT_LOG(debug) << getIdentification() << ": Nothing to import!";
            }

            // ====== Delete input files ====================================
            HPCT_LOG(debug) << getIdentification() << ": Deleting " << files << " input files ...";
            for(const std::filesystem::path* dataFile : dataFileList) {
               finishedFile(*dataFile);
            }
         }
         catch(const std::exception& exception) {
            HPCT_LOG(warning) << getIdentification() << ": Import in fast mode failed: "
                              << exception.what() << std::endl;
            DatabaseClient->rollback();

            // ====== Slow import: handle files sequentially ================
            if(files > 1) {
               HPCT_LOG(debug) << getIdentification() << ": Trying to import " << files << " files in slow mode ...";

               for(const std::filesystem::path* dataFile : dataFileList) {
                  try {
                     // ====== Import one input file in one transaction =====
                     Reader->beginParsing(statement, rows, DatabaseClient->getType());
                     HPCT_LOG(trace) << getIdentification() << ": Parsing " << *dataFile << " ...";
                     processFile(statement, rows, dataFile);
                     if(Reader->finishParsing(statement, rows, DatabaseClient->getType())) {
                        DatabaseClient->beginTransaction();
                        DatabaseClient->execute(statement.str());
                        DatabaseClient->commit();
                        HPCT_LOG(debug) << getIdentification() << ": Committed " << rows
                                        << " rows from " << *dataFile;
                     }
                     else {
                        std::cout << "Nothing to do!" << std::endl;
                        HPCT_LOG(debug) << getIdentification() << ": Nothing to import!";
                     }

                     // ====== Delete input file ============================
                     finishedFile(*dataFile);
                  }
                  catch(const std::exception& exception) {
                     DatabaseClient->rollback();
                     HPCT_LOG(warning) << getIdentification() << ": Importing " << *dataFile << " in slow mode failed: "
                                       << exception.what() << std::endl;
                  }
               }
            }
         }
      }

      // ====== Wait for new data ===========================================
      HPCT_LOG(trace) << getIdentification() << ": sleeping ...";
      Notification.wait(lock);
   }
}



class UniversalImporter
{
   public:
   UniversalImporter(const std::filesystem::path& dataDirectory,
                     const unsigned int           maxDepth = 5);
   ~UniversalImporter();

   void addReader(BasicReader*         reader,
                  DatabaseClientBase** databaseClientArray,
                  const size_t         databaseClients);
   void removeReader(BasicReader* reader);
   void lookForFiles();
   bool start();
   void stop();
   void run();
   void printStatus(std::ostream& os = std::cout);

   private:
   void lookForFiles(const std::filesystem::path& dataDirectory,
                     const unsigned int           maxDepth);
   void addFile(const std::filesystem::path& dataFile);

   struct WorkerMapping {
      BasicReader* Reader;
      unsigned int WorkerID;
   };
   friend bool operator<(const UniversalImporter::WorkerMapping& a,
                         const UniversalImporter::WorkerMapping& b);

   std::list<BasicReader*>                ReaderList;
   std::map<const WorkerMapping, Worker*> WorkerMap;
   const std::filesystem::path            DataDirectory;
   const unsigned int                     MaxDepth;

   boost::asio::io_service                IOService;
   int                                    SignalFD;
   boost::asio::posix::stream_descriptor  SignalStream;
#ifdef __linux__
   int                                    INotifyFD;
   int                                    INotifyWatchDescriptor;
   boost::asio::posix::stream_descriptor  INotifyStream;
#endif
};


// ###### < operator for sorting ########################################################
bool operator<(const UniversalImporter::WorkerMapping& a,
               const UniversalImporter::WorkerMapping& b) {
   if(a.Reader < b.Reader) {
      return true;
   }
   if(a.WorkerID < b.WorkerID) {
      return true;
   }
   return false;
}


// ###### Constructor #######################################################
UniversalImporter::UniversalImporter(const std::filesystem::path& dataDirectory,
                                     const unsigned int           maxDepth)
 : DataDirectory(dataDirectory),
   MaxDepth(maxDepth),
   SignalStream(IOService),
   INotifyStream(IOService)
{
#ifdef __linux__
   INotifyWatchDescriptor = -1;
   INotifyFD              = -1;
#endif
}


// ###### Destructor ########################################################
UniversalImporter::~UniversalImporter()
{
   stop();
}


// ###### Start importer ####################################################
bool UniversalImporter::start()
{
   // ====== Intercept signals ==============================================
   sigset_t signalMask;
   sigemptyset(&signalMask);
   sigaddset(&signalMask, SIGINT);
   sigaddset(&signalMask, SIGTERM);
   sigprocmask(SIG_BLOCK, &signalMask, nullptr);
   SignalFD = signalfd(-1, &signalMask, SFD_NONBLOCK|SFD_CLOEXEC);
   assert(SignalFD > 0);
   SignalStream.assign(SignalFD);

   // ====== Set up INotify =================================================
#ifdef __linux__
   INotifyFD = inotify_init();
   assert(INotifyFD > 0);
   INotifyStream.assign(INotifyFD);
   INotifyWatchDescriptor = inotify_add_watch(INotifyFD, DataDirectory.c_str(), IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MODIFY);
   if(INotifyWatchDescriptor < 0) {
      HPCT_LOG(error) << "Unable to configure inotify: " <<strerror(errno);
      return false;
   }
#else
#error FIXME!
#endif

   // ====== Look for files =================================================
   lookForFiles();
   printStatus();

   return true;
}


// ###### Stop importer #####################################################
void UniversalImporter::stop()
{
   for(std::list<BasicReader*>::iterator readerIterator = ReaderList.begin(); readerIterator != ReaderList.end(); ) {
      removeReader(*readerIterator);
      readerIterator = ReaderList.begin();
   }
}


// ###### Add reader ########################################################
void UniversalImporter::addReader(BasicReader*         reader,
                                  DatabaseClientBase** databaseClientArray,
                                  const size_t         databaseClients)
{
   ReaderList.push_back(reader);
   for(unsigned int w = 0; w < databaseClients; w++) {
      Worker* worker = new Worker(w, reader, databaseClientArray[w]);
      assert(worker != nullptr);
      WorkerMapping workerMapping;
      workerMapping.Reader  = reader;
      workerMapping.WorkerID = w;
      WorkerMap.insert(std::pair<const WorkerMapping, Worker*>(workerMapping, worker));
   }
}


// ###### Remove reader #####################################################
void UniversalImporter::removeReader(BasicReader* reader)
{
   for(std::list<BasicReader*>::iterator readerIterator = ReaderList.begin();
       readerIterator != ReaderList.end();
       readerIterator++) {
      if(*readerIterator == reader) {
         ReaderList.erase(readerIterator);
         break;
      }
   }

   for(std::map<const WorkerMapping, Worker*>::iterator workerMappingIterator = WorkerMap.begin();
       workerMappingIterator != WorkerMap.end(); ) {
      if(workerMappingIterator->first.Reader == reader) {
         delete workerMappingIterator->second;
         workerMappingIterator = WorkerMap.erase(workerMappingIterator);
      }
      else {
         workerMappingIterator++;
      }
   }
}


// ###### Look for input files (full directory traversal) ###################
void UniversalImporter::lookForFiles()
{
   lookForFiles(DataDirectory, MaxDepth);
}


// ###### Look for input files (limited directory traversal) ################
void UniversalImporter::lookForFiles(const std::filesystem::path& dataDirectory,
                                     const unsigned int           maxDepth)
{
   for(const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(dataDirectory)) {
      if(dirEntry.is_regular_file()) {
         addFile(dirEntry.path());
      }
      else if(dirEntry.is_directory()) {
         if(maxDepth > 1) {
            lookForFiles(dirEntry, maxDepth - 1);
         }
      }
   }
}


// ###### Add input file ####################################################
void UniversalImporter::addFile(const std::filesystem::path& dataFile)
{
   // const std::filesystem::path& subdir = std::filesystem::relative(dataFile, DataDirectory).parent_path();
   // const std::string& filename = dataFile.filename();

   std::smatch        match;
   const std::string& filename = dataFile.filename().string();
   for(BasicReader* reader : ReaderList) {
      if(std::regex_match(filename, match, reader->getFileNameRegExp())) {
         const int worker = reader->addFile(dataFile, match);
         if(worker >= 0) {
            WorkerMapping workerMapping;
            workerMapping.Reader   = reader;
            workerMapping.WorkerID = worker;
            std::map<const WorkerMapping, Worker*>::iterator found = WorkerMap.find(workerMapping);
            if(found != WorkerMap.end()) {
               Worker* worker = found->second;
               worker->wakeUp();
            }
         }
      }
   }
}


// ###### Print importer status #############################################
void UniversalImporter::printStatus(std::ostream& os)
{
   for(BasicReader* reader : ReaderList) {
      reader->printStatus(os);
   }
}



int main(int argc, char** argv)
{
//    UniversalImporter importer(".", 5);
//
//    HiPerConTracerPingReader pingReader;
//    importer.addReader(&pingReader);
//    HiPerConTracerTracerouteReader tracerouteReader;
//    importer.addReader(&tracerouteReader);

   unsigned int logLevel = boost::log::trivial::severity_level::trace;

   initialiseLogger(logLevel);


   const unsigned int pingWorkers = 1;
   MariaDBClient* pingDatabaseClients[pingWorkers];
   for(unsigned int i = 0; i < pingWorkers; i++) {
      pingDatabaseClients[i] = new MariaDBClient();
   }

   NorNetEdgePingReader nnePingReader(pingWorkers);

   UniversalImporter importer("data", 5);

   importer.addReader(&nnePingReader,
                       (DatabaseClientBase**)&pingDatabaseClients, pingWorkers);

   if(importer.start() == false) {
      exit(1);
   }

   importer.stop();

   for(unsigned int i = 0; i < pingWorkers; i++) {
      delete pingDatabaseClients[i];
   }
}
