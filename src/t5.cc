#include <filesystem>
#include <iostream>
#include <regex>
#include <set>
#include <list>
#include <mutex>
#include <thread>
#include <cassert>
#include <condition_variable>

// g++ t5.cc -o t5 -std=c++17


class DatabaseClientBase
{
   public:
   enum DatabaseType {
      SQL_Generic    = (1 << 0),
      NoSQL_Generic  = (1 << 1),

      SQL_MariaDB    = SQL_Generic | (1 << 16),
      SQL_PostgreSQL = SQL_Generic | (1 << 17),
      SQL_Cassandra  = SQL_Generic | (1 << 18),

      NoSQL_MongoDB  = NoSQL_Generic | (1 << 24)
   };

   virtual const DatabaseClientBase::DatabaseType getType() const = 0;
   virtual bool beginTransaction() = 0;
   virtual bool endTransaction(const bool commit) = 0;

   inline bool commit() { return endTransaction(true); }
   inline bool rollback() { return endTransaction(false); }
};


class MariaDBClient : public virtual  DatabaseClientBase
{
   public:
   MariaDBClient();
   ~MariaDBClient();

   virtual const DatabaseType getType() const;
   virtual bool beginTransaction();
   virtual bool endTransaction(const bool commit);
};


MariaDBClient::MariaDBClient()
{
}


MariaDBClient::~MariaDBClient()
{
}


const DatabaseClientBase::DatabaseType MariaDBClient::getType() const
{
   return DatabaseClientBase::DatabaseType::SQL_MariaDB;
}


bool MariaDBClient::beginTransaction()
{
   return false;
}


bool MariaDBClient::endTransaction(const bool commit)
{
   return false;
}



class BasicReader
{
   public:
   BasicReader(const unsigned int workers,
               const unsigned int maxTransactionSize);
   ~BasicReader();

   virtual const std::string& getIdentification() const = 0;
   virtual const std::regex& getFileNameRegExp() const = 0;
   virtual void addFile(const std::filesystem::path& dataFile,
                        const std::smatch            match) = 0;
   virtual void printStatus(std::ostream& os = std::cout) = 0;
   virtual unsigned int fetchFiles(std::list<const std::filesystem::path*>& dataFileList,
                                   const unsigned int                       worker,
                                   const unsigned int                       limit = 1) = 0;

   inline const unsigned int getWorkers() const { return Workers; }
   inline const unsigned int getMaxTransactionSize() const { return MaxTransactionSize; }

   protected:
   const unsigned int Workers;
   const unsigned int MaxTransactionSize;
};


BasicReader::BasicReader(const unsigned int workers,
                         const unsigned int maxTransactionSize)
   : Workers(workers),
     MaxTransactionSize(maxTransactionSize)
{
   assert(Workers > 0);
   assert(MaxTransactionSize > 0);
}


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

      inline bool operator<(const InputFileEntry& cmp) {
         if(Source < cmp.Source) {
            return true;
         }
         if(TimeStamp < cmp.TimeStamp) {
            return true;
         }
         if(SeqNumber < cmp.SeqNumber) {
            return true;
         }
         return false;
      }
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


HiPerConTracerPingReader::HiPerConTracerPingReader()
{
}


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
   virtual void addFile(const std::filesystem::path& dataFile,
                        const std::smatch            match);
   virtual unsigned int fetchFiles(std::list<const std::filesystem::path*>& dataFileList,
                                   const unsigned int                       worker,
                                   const unsigned int                       limit = 1);
   virtual void printStatus(std::ostream& os = std::cout);

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
   std::set<InputFileEntry>* InputFileSet;
};


const std::string  NorNetEdgePingReader::Identification = "UDPPing";
const std::regex NorNetEdgePingReader::FileNameRegExp = std::regex(
   // Format: uping_<MeasurementID>.dat.<YYYY-MM-DD-HH-MM-SS>.xz
   "^uping_([0-9]+)\\.dat\\.([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9][0-9]-[0-9][0-9]-[0-9][0-9])\\.xz$"
);


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


NorNetEdgePingReader::NorNetEdgePingReader(const unsigned int workers,
                                           const unsigned int maxTransactionSize)
   : BasicReader(workers, maxTransactionSize)
{
   InputFileSet = new std::set<InputFileEntry>[Workers];
   assert(InputFileSet != nullptr);
}


NorNetEdgePingReader::~NorNetEdgePingReader()
{
   delete [] InputFileSet;
   InputFileSet = nullptr;
}


const std::string& NorNetEdgePingReader::getIdentification() const
{
   return Identification;
}


const std::regex& NorNetEdgePingReader::getFileNameRegExp() const
{
   return(FileNameRegExp);
}


void NorNetEdgePingReader::addFile(const std::filesystem::path& dataFile,
                                   const std::smatch            match)
{
   if(match.size() == 3) {
      InputFileEntry inputFileEntry;
      inputFileEntry.MeasurementID = atol(match[1].str().c_str());
      inputFileEntry.TimeStamp     = match[2];
      inputFileEntry.DataFile      = dataFile;

//       std::cout << "  t=" << inputFileEntry.TimeStamp << std::endl;
//       std::cout << "  m=" << inputFileEntry.MeasurementID << std::endl;

      const unsigned int worker = inputFileEntry.MeasurementID % Workers;
      InputFileSet[worker].insert(inputFileEntry);
   }
}


unsigned int NorNetEdgePingReader::fetchFiles(std::list<const std::filesystem::path*>& dataFileList,
                                              const unsigned int                       worker,
                                              const unsigned int                       limit)
{
   assert(worker < Workers);

   unsigned int n = 0;
   for(const InputFileEntry& inputFileEntry : InputFileSet[worker]) {
      dataFileList.push_back(&inputFileEntry.DataFile);
      n++;
      if(n >= limit) {
         break;
      }
   }

   return n;
}


void NorNetEdgePingReader::printStatus(std::ostream& os)
{
   os << "NorNetEdgePing:" << std::endl;
   for(unsigned int w = 0; w < Workers; w++) {
      os << " - Worker #" << w + 1 << ": " << InputFileSet[w].size() << std::endl;
//       for(const InputFileEntry& inputFileEntry : InputFileSet[w]) {
//          os << "  - " <<  inputFileEntry.DataFile << std::endl;
//       }
   }
}


class Worker
{
   public:
   Worker(const unsigned int  workerID,
          BasicReader*        reader,
          DatabaseClientBase* databaseClient);
   ~Worker();

   inline const std::string& getIdentification() const { return Identification; }


   private:
   void run();

   const unsigned int      WorkerID;
   BasicReader*            Reader;
   DatabaseClientBase*     DatabaseClient;
   const std::string       Identification;
   std::thread             Thread;
   std::mutex              Mutex;
   std::condition_variable Notification;
};


Worker::Worker(const unsigned int  workerID,
               BasicReader*        reader,
               DatabaseClientBase* databaseClient)
   : WorkerID(workerID),
     Reader(reader),
     DatabaseClient(databaseClient),
     Identification(reader->getIdentification() + "/" + std::to_string(WorkerID))
{
}


Worker::~Worker()
{
}


void Worker::run()
{
   std::unique_lock lock(Mutex);

   std::cout << "Worker running!" << std::endl;
   while(Reader != nullptr) {
      std::list<const std::filesystem::path*> dataFileList;

      // ====== Fast import: try to combine files ===========================
      const unsigned int files = Reader->fetchFiles(dataFileList, WorkerID, Reader->getMaxTransactionSize());
      if(files > 0) {
         if(DatabaseClient->beginTransaction()) {

         }
      }

      // ====== Wait for new data ===========================================
      Notification.wait(lock);
   }
}



class Collector
{
   public:
   Collector(const std::filesystem::path& dataDirectory,
             const unsigned int           maxDepth = 5);
   ~Collector();

   void addReader(BasicReader*      reader,
                  DatabaseClientBase** databaseClientArray,
                  const size_t         databaseClients);
   void lookForFiles();
   void printStatus(std::ostream& os = std::cout);

   private:
   void lookForFiles(const std::filesystem::path& dataDirectory,
                     const unsigned int           maxDepth);
   void addFile(const std::filesystem::path& dataFile);

   std::list<BasicReader*>     ReaderList;
   std::list<Worker*>          WorkerList;
   const std::filesystem::path DataDirectory;
   const unsigned int          MaxDepth;
};


Collector::Collector(const std::filesystem::path& dataDirectory,
                     const unsigned int           maxDepth)
 : DataDirectory(dataDirectory),
   MaxDepth(maxDepth)
{
}


Collector::~Collector()
{
}


void Collector::addReader(BasicReader*         reader,
                          DatabaseClientBase** databaseClientArray,
                          const size_t         databaseClients)
{
   ReaderList.push_back(reader);
   for(unsigned int w = 0; w < databaseClients; w++) {
      Worker* worker = new Worker(w, reader, databaseClientArray[w]);
      WorkerList.push_back(worker);
   }
}


void Collector::lookForFiles()
{
   lookForFiles(DataDirectory, MaxDepth);
}


void Collector::lookForFiles(const std::filesystem::path& dataDirectory,
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


void Collector::addFile(const std::filesystem::path& dataFile)
{
   // const std::filesystem::path& subdir = std::filesystem::relative(dataFile, DataDirectory).parent_path();
   // const std::string& filename = dataFile.filename();

   std::smatch        match;
   const std::string& filename = dataFile.filename().string();
   for(BasicReader* reader : ReaderList) {
      if(std::regex_match(filename, match, reader->getFileNameRegExp())) {
         reader->addFile(dataFile, match);
      }
   }
}


void Collector::printStatus(std::ostream& os)
{
   for(BasicReader* reader : ReaderList) {
      reader->printStatus(os);
   }
}



int main(int argc, char** argv)
{
//    Collector collector(".", 5);
//
//    HiPerConTracerPingReader pingReader;
//    collector.addReader(&pingReader);
//    HiPerConTracerTracerouteReader tracerouteReader;
//    collector.addReader(&tracerouteReader);

   const unsigned int pingWorkers = 4;
   MariaDBClient* pingDatabaseClients[pingWorkers];
   for(unsigned int i = 0; i < pingWorkers; i++) {
      pingDatabaseClients[i] = new MariaDBClient();
   }

   Collector collector("data", 5);

   NorNetEdgePingReader nnePingReader(pingWorkers);
   collector.addReader(&nnePingReader,
                       (DatabaseClientBase**)&pingDatabaseClients, pingWorkers);

   collector.lookForFiles();
   collector.printStatus();

   for(unsigned int i = 0; i < pingWorkers; i++) {
      delete pingDatabaseClients[i];
   }
}
