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

#include "logger.h"
#include "tools.h"

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
#include <boost/program_options.hpp>

#include <unistd.h>
#ifdef __linux__
#include <sys/inotify.h>
#endif


// g++ t5.cc -o t5 -std=c++17
// g++ t5.cc -o t5 -std=c++17 -O0 -g -Wall -lpthread && rm -f core && ./t5


//  Base class for all importer problems (logic, reader, database)
class ImporterException : public std::runtime_error
{
   public:
   ImporterException(const std::string& error) : std::runtime_error(error) { }
};


// Program logic exception
class ImporterLogicException : public ImporterException
{
   public:
   ImporterLogicException(const std::string& error) : ImporterException(error) { }
};


// Generic reader problem
class ImporterReaderException : public ImporterException
{
   public:
   ImporterReaderException(const std::string& error) : ImporterException(error) { }
};


// Problem with input data (syntax error, etc.) => invalid data
class ImporterReaderDataErrorException : public ImporterReaderException
{
   public:
   ImporterReaderDataErrorException(const std::string& error) : ImporterReaderException(error) { }
};


// Generic database problem
class ImporterDatabaseException : public ImporterException
{
   public:
   ImporterDatabaseException(const std::string& error) : ImporterException(error) { }
};


// Problem with database transaction (syntax error, etc.) => invalid data
class ImporterDatabaseDataErrorException : public ImporterDatabaseException
{
   public:
   ImporterDatabaseDataErrorException(const std::string& error) : ImporterDatabaseException(error) { }
};



enum DatabaseBackendType {
   Invalid        = 0,

   SQL_Generic    = (1 << 0),
   NoSQL_Generic  = (1 << 1),

   SQL_Debug      = SQL_Generic | (1 << 16),
   SQL_MariaDB    = SQL_Generic | (1 << 17),
   SQL_PostgreSQL = SQL_Generic | (1 << 18),
   SQL_Cassandra  = SQL_Generic | (1 << 19),

   NoSQL_Debug    = NoSQL_Generic | (1 << 24),
   NoSQL_MongoDB  = NoSQL_Generic | (1 << 25)
};


enum ImportModeType {
   KeepImportedFiles   = 0,   // Keep the files where they are
   MoveImportedFiles   = 1,   // Move into "good file" directory
   DeleteImportedFiles = 2    // Delete
};


class DatabaseClientBase;

class DatabaseConfiguration
{
   public:
   DatabaseConfiguration();
   ~DatabaseConfiguration();

   inline DatabaseBackendType getBackend()    const { return Backend;    }
   inline const std::string&  getServer()     const { return Server;     }
   inline const uint16_t      getPort()       const { return Port;       }
   inline const std::string&  getUser()       const { return User;       }
   inline const std::string&  getPassword()   const { return Password;   }
   inline const std::string&  getCAFile()     const { return CAFile;     }
   inline const std::string&  getDatabase()   const { return Database;   }
   inline ImportModeType      getImportMode() const { return ImportMode; }
   inline const std::filesystem::path& getImportFilePath() const { return ImportFilePath; }
   inline const std::filesystem::path& getBadFilePath()    const { return BadFilePath;    }
   inline const std::filesystem::path& getGoodFilePath()   const { return GoodFilePath;   }

   inline void setBackend(const DatabaseBackendType backend)  { Backend = backend;       }
   inline void setImportMode(const ImportModeType importMode) { ImportMode = importMode; }

   bool readConfiguration(const std::filesystem::path& configurationFile);
   void printConfiguration(std::ostream& os) const;
   DatabaseClientBase* createClient();

   private:
   boost::program_options::options_description OptionsDescription;
   std::string           BackendName;
   DatabaseBackendType   Backend;
   std::string           Server;
   uint16_t              Port;
   std::string           User;
   std::string           Password;
   std::string           CAFile;
   std::string           Database;
   std::string           ImportModeName;
   ImportModeType        ImportMode;
   std::filesystem::path ImportFilePath;
   std::filesystem::path BadFilePath;
   std::filesystem::path GoodFilePath;
};



class DatabaseClientBase
{
   public:
   DatabaseClientBase(const DatabaseConfiguration& configuration);
   virtual ~DatabaseClientBase();

   virtual const DatabaseBackendType getBackend() const = 0;
   virtual bool open()  = 0;
   virtual void close() = 0;

   virtual void startTransaction() = 0;
   virtual void execute(const std::string& statement) = 0;
   virtual void endTransaction(const bool commit) = 0;

   inline void commit()   { endTransaction(true);  }
   inline void rollback() { endTransaction(false); }

   inline void clearStatement()             { Statement.str(std::string());               }
   inline bool statementIsEmpty() const     { return Statement.str().size() == 0;         }
   inline std::stringstream& getStatement() { return Statement;                           }
   inline void executeStatement()           { execute(Statement.str()); clearStatement(); }

   protected:
   const DatabaseConfiguration& Configuration;
   std::stringstream            Statement;
};


// ###### Constructor #######################################################
DatabaseClientBase::DatabaseClientBase(const DatabaseConfiguration& configuration)
   : Configuration(configuration)
{
}


// ###### Destructor ########################################################
DatabaseClientBase::~DatabaseClientBase()
{
}



class DebugClient : public  DatabaseClientBase
{
   public:
   DebugClient(const DatabaseConfiguration& configuration);
   virtual ~DebugClient();

   virtual const DatabaseBackendType getBackend() const;
   virtual bool open();
   virtual void close();

   virtual void startTransaction();
   virtual void execute(const std::string& statement);
   virtual void endTransaction(const bool commit);
};


// ###### Constructor #######################################################
DebugClient::DebugClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
}


// ###### Destructor ########################################################
DebugClient::~DebugClient()
{
}


// ###### Get backend #######################################################
const DatabaseBackendType DebugClient::getBackend() const
{
   return Configuration.getBackend();
}


// ###### Prepare connection to database ####################################
bool DebugClient::open()
{
   return true;
}


// ###### Finish connection to database #####################################
void DebugClient::close()
{
}


// ###### Begin transaction #################################################
void DebugClient::startTransaction()
{
   std::cout << "START TRANSACTION;" << std::endl;
}


// ###### End transaction ###################################################
void DebugClient::endTransaction(const bool commit)
{
   if(commit) {
      std::cout << "COMMIT;" << std::endl;
      throw ImporterDatabaseException("DEBUG CLIENT ONLY");
   }
   else {
      std::cout << "ROLLBACK;" << std::endl;
   }
}


// ###### Execute statement #################################################
void DebugClient::execute(const std::string& statement)
{
   std::cout << statement << std::endl;
}



// Ubuntu: libmysqlcppconn-dev
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>


class MariaDBClient : public DatabaseClientBase
{
   public:
   MariaDBClient(const DatabaseConfiguration& databaseConfiguration);
   virtual ~MariaDBClient();

   virtual const DatabaseBackendType getBackend() const;
   virtual bool open();
   virtual void close();

   virtual void startTransaction();
   virtual void execute(const std::string& statement);
   virtual void endTransaction(const bool commit);

   private:
   void handleSQLException(const sql::SQLException& exception,
                           const std::string&       where,
                           const std::string&       statement = std::string());

   sql::Driver*     Driver;
   sql::Connection* Connection;
   sql::Statement*  Statement;
};


// ###### Constructor #######################################################
MariaDBClient::MariaDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   Driver = get_driver_instance();
   assert(Driver != nullptr);
   Connection = nullptr;
   Statement  = nullptr;
}


// ###### Destructor ########################################################
MariaDBClient::~MariaDBClient()
{
   close();
}


// ###### Get backend #######################################################
const DatabaseBackendType MariaDBClient::getBackend() const
{
   return DatabaseBackendType::SQL_MariaDB;
}


// ###### Prepare connection to database ####################################
bool MariaDBClient::open()
{
   const std::string url = "tcp://" + Configuration.getServer() + ":" + std::to_string(Configuration.getPort());

   assert(Connection == nullptr);
   try {
      // ====== Connect to database =========================================
      Connection = Driver->connect(url.c_str(),
                                   Configuration.getUser().c_str(),
                                   Configuration.getPassword().c_str());
      assert(Connection != nullptr);
      Connection->setSchema(Configuration.getDatabase().c_str());
      // SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED
      Connection->setTransactionIsolation(sql::TRANSACTION_READ_COMMITTED);
      Connection->setAutoCommit(false);

      // ====== Create statement ============================================
      Statement = Connection->createStatement();
      assert(Statement != nullptr);
   }
   catch(const sql::SQLException& e) {
      HPCT_LOG(error) << "Unable to connect MariaDB client to " << url << ": " << e.what();
      close();
      return false;
   }

   return true;
}


// ###### Finish connection to database #####################################
void MariaDBClient::close()
{
   if(Statement) {
      delete Statement;
      Statement = nullptr;
   }
   if(Connection != nullptr) {
      delete Connection;
      Connection = nullptr;
   }
}


// ###### Handle SQLException ###############################################
void MariaDBClient::handleSQLException(const sql::SQLException& exception,
                                       const std::string&       where,
                                       const std::string&       statement)
{
   // ====== Log error ======================================================
   const std::string what = where + " error " +
                               exception.getSQLState() + "/E" +
                               std::to_string(exception.getErrorCode()) + ": " +
                               exception.what();
   HPCT_LOG(error) << what;
   std::cerr << statement << "\n";

   // ====== Throw exception ================================================
   const std::string e = exception.getSQLState().substr(0, 2);
   // Integrity Error, according to mysql/connector/errors.py
   if( (e == "23") || (e == "XA")) {
      // For this type, the input file should be moved to the bad directory.
      throw ImporterDatabaseDataErrorException(what);
   }
   // Other error
   else {
      throw ImporterDatabaseException(what);
   }
}


// ###### Begin transaction #################################################
void MariaDBClient::startTransaction()
{
   try {
      if(Connection->isClosed()) {
         Connection->reconnect();
      }
      Statement->execute("START TRANSACTION");
   }
   catch(const sql::SQLException& exception) {
      handleSQLException(exception, "Start of transaction");
   }
}


// ###### End transaction ###################################################
void MariaDBClient::endTransaction(const bool commit)
{
   // ====== Commit transaction =============================================
   if(commit) {
      try {
         Connection->commit();
      }
      catch(const sql::SQLException& exception) {
         handleSQLException(exception, "Commit");
      }
   }

   // ====== Commit transaction =============================================
   else {
      try {
         Connection->rollback();
      }
      catch(const sql::SQLException& exception) {
         handleSQLException(exception, "Rollback");
      }
   }
}


// ###### Execute statement #################################################
void MariaDBClient::execute(const std::string& statement)
{
   try {
      Statement->execute(statement);
   }
   catch(const sql::SQLException& exception) {
      handleSQLException(exception, "Execute", statement);
   }
}



// ###### Constructor #######################################################
DatabaseConfiguration::DatabaseConfiguration()
   : OptionsDescription("Options")
{
   OptionsDescription.add_options()
      ("dbserver",          boost::program_options::value<std::string>(&Server),                   "database server")
      ("dbport",            boost::program_options::value<uint16_t>(&Port),                        "database port")
      ("dbuser",            boost::program_options::value<std::string>(&User),                     "database username")
      ("dbpassword",        boost::program_options::value<std::string>(&Password),                 "database password")
      ("dbcafile",          boost::program_options::value<std::string>(&CAFile),                   "database CA file")
      ("database",          boost::program_options::value<std::string>(&Database),                 "database name")
      ("dbbackend",         boost::program_options::value<std::string>(&BackendName),              "database backend")
      ("import_mode",       boost::program_options::value<std::string>(&ImportModeName),           "import mode")
      ("import_file_path",  boost::program_options::value<std::filesystem::path>(&ImportFilePath), "path for input data")
      ("bad_file_path",     boost::program_options::value<std::filesystem::path>(&BadFilePath),    "path for bad files")
      ("good_file_path",    boost::program_options::value<std::filesystem::path>(&GoodFilePath),   "path for good files")
   ;
   BackendName    = "Invalid";
   Backend        = DatabaseBackendType::Invalid;
   ImportModeName = "KeepImportedFiles";
   ImportMode     = ImportModeType::KeepImportedFiles;
}


// ###### Destructor ########################################################
DatabaseConfiguration::~DatabaseConfiguration()
{
}


// ###### Read database configuration #######################################
bool DatabaseConfiguration::readConfiguration(const std::filesystem::path& configurationFile)
{
   std::ifstream                         configurationInputStream(configurationFile);
   boost::program_options::variables_map vm = boost::program_options::variables_map();

   boost::program_options::store(boost::program_options::parse_config_file(configurationInputStream , OptionsDescription), vm);
   boost::program_options::notify(vm);

   // ====== Check options ==================================================
   if(ImportModeName == "KeepImportedFiles") {
      ImportMode = ImportModeType::KeepImportedFiles;
   }
   else if(ImportModeName == "MoveImportedFiles") {
      ImportMode = ImportModeType::MoveImportedFiles;
   }
   else if(ImportModeName == "DeleteImportedFiles") {
      ImportMode = ImportModeType::DeleteImportedFiles;
   }
   else {
      HPCT_LOG(error) << "Invalid import mode name " << ImportModeName;
      return false;
   }

   if( (BackendName == "MySQL") || (BackendName == "MariaDB") ) {
      Backend = DatabaseBackendType::SQL_MariaDB;
   }
   else if(BackendName == "PostgreSQL") {
      Backend = DatabaseBackendType::SQL_PostgreSQL;
   }
   else if(BackendName == "MongoDB") {
      Backend = DatabaseBackendType::NoSQL_MongoDB;
   }
   else if(BackendName == "DebugSQL") {
      Backend = DatabaseBackendType::SQL_Debug;
   }
   else if(BackendName == "DebugNoSQL") {
      Backend = DatabaseBackendType::NoSQL_Debug;
   }
   else {
      HPCT_LOG(error) << "Invalid backend name " << Backend;
      return false;
   }

   // ====== Check directories ==============================================
   std::error_code ec;
   if(!is_directory(ImportFilePath, ec)) {
      HPCT_LOG(error) << "Import file path " << ImportFilePath << " does not exist: " << ec;
      return false;
   }
   if(!is_directory(GoodFilePath, ec)) {
      HPCT_LOG(error) << "Good file path " << GoodFilePath << " does not exist: " << ec;
      return false;
   }
   if(!is_directory(BadFilePath, ec)) {
      HPCT_LOG(error) << "Bad file path " << BadFilePath << " does not exist: " << ec;
      return false;
   }

   if(is_subdir_of(GoodFilePath, ImportFilePath)) {
      HPCT_LOG(error) << "Good file path " << GoodFilePath
                      << " must not be within import file path " << ImportFilePath;
      return false;
   }
   if(is_subdir_of(BadFilePath, ImportFilePath)) {
      HPCT_LOG(error) << "Bad file path " << BadFilePath
                      << " must not be within import file path " << ImportFilePath;
      return false;
   }

   return true;
}


// ###### Print reader status ###############################################
void DatabaseConfiguration::printConfiguration(std::ostream& os) const
{
   os << "Database configuration:"    << std::endl
      << "Backend  = " << BackendName << std::endl
      << "Server   = " << Server      << std::endl
      << "Port     = " << Port        << std::endl
      << "User     = " << User        << std::endl
      << "Password = " << ((Password.size() > 0) ? "****************" : "(none)") << std::endl
      << "CA File  = " << CAFile      << std::endl
      << "Database = " << Database    << std::endl;
}


// ###### Create new database client instance ###############################
DatabaseClientBase* DatabaseConfiguration::createClient()
{
   DatabaseClientBase* databaseClient = nullptr;

   switch(Backend) {
      case SQL_Debug:
      case NoSQL_Debug:
          databaseClient = new DebugClient(*this);
       break;
      case SQL_MariaDB:
          databaseClient = new MariaDBClient(*this);
       break;
      default:
       break;
   }

   return databaseClient;
}



class BasicReader
{
   public:
   BasicReader(const unsigned int workers,
               const unsigned int maxTransactionSize);
   virtual ~BasicReader();

   virtual const std::string& getIdentification() const = 0;
   virtual const std::regex& getFileNameRegExp() const = 0;

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
                              boost::iostreams::filtering_istream& inputStream) = 0;

   inline const unsigned int getWorkers() const { return Workers; }
   inline const unsigned int getMaxTransactionSize() const { return MaxTransactionSize; }

   protected:
   const unsigned int Workers;
   const unsigned int MaxTransactionSize;
   unsigned long long TotalFiles;
};


// ###### Constructor #######################################################
BasicReader::BasicReader(const unsigned int workers,
                         const unsigned int maxTransactionSize)
   : Workers(workers),
     MaxTransactionSize(maxTransactionSize)
{
   assert(Workers > 0);
   assert(MaxTransactionSize > 0);
   TotalFiles = 0;
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
   NorNetEdgePingReader(const unsigned int workers                        = 1,
                        const unsigned int maxTransactionSize             = 4,
                        const std::string& table_measurement_generic_data = "measurement_generic_data");
   virtual ~NorNetEdgePingReader();

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
   typedef std::chrono::system_clock               FileEntryClock;
   typedef std::chrono::time_point<FileEntryClock> FileEntryTimePoint;
   struct InputFileEntry {
      FileEntryTimePoint    TimeStamp;
      unsigned int          MeasurementID;
      std::filesystem::path DataFile;
   };
   friend bool operator<(const NorNetEdgePingReader::InputFileEntry& a,
                         const NorNetEdgePingReader::InputFileEntry& b);
   friend std::ostream& operator<<(std::ostream& os, const InputFileEntry& entry);

   static const std::string  Identification;
   static const std::regex   FileNameRegExp;
   const std::string         Table_measurement_generic_data;
   std::mutex                Mutex;
   std::set<InputFileEntry>* DataFileSet;
};


const std::string NorNetEdgePingReader::Identification = "UDPPing";
const std::regex  NorNetEdgePingReader::FileNameRegExp = std::regex(
   // Format: uping_<MeasurementID>.dat.<YYYY-MM-DD_HH-MM-SS>.xz
   "^uping_([0-9]+)\\.dat\\.([0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9][0-9]-[0-9][0-9]-[0-9][0-9])\\.xz$"
);


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const NorNetEdgePingReader::InputFileEntry& a,
               const NorNetEdgePingReader::InputFileEntry& b)
{
   // ====== Level 1: TimeStamp =============================================
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   else if(a.TimeStamp == b.TimeStamp) {
      // ====== Level 2: MeasurementID ======================================
      if(a.MeasurementID < b.MeasurementID) {
         return true;
      }
      else if(a.MeasurementID == b.MeasurementID) {
         // ====== Level 3: DataFile ========================================
         if(a.DataFile < b.DataFile) {
            return true;
         }
      }
   }
   return false;
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const NorNetEdgePingReader::InputFileEntry& entry)
{
   os << "("
      << timePointToString<NorNetEdgePingReader::FileEntryTimePoint>(entry.TimeStamp) << ", "
      << entry.MeasurementID << ", "
      << entry.DataFile
      << ")";
   return os;
}


// ###### Constructor #######################################################
NorNetEdgePingReader::NorNetEdgePingReader(const unsigned int workers,
                                           const unsigned int maxTransactionSize,
                                           const std::string& table_measurement_generic_data)
   : BasicReader(workers, maxTransactionSize),
     Table_measurement_generic_data(table_measurement_generic_data)
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
//  static int n=0;n++;
//  if(n>20) {
//   puts("??????");
//   return -1;  // ??????
//  }

   if(match.size() == 3) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[2].str(), timeStamp, "%Y-%m-%d_%H-%M-%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.TimeStamp     = timeStamp;
         inputFileEntry.MeasurementID = atol(match[1].str().c_str());
         inputFileEntry.DataFile      = dataFile;
         const int workerID = inputFileEntry.MeasurementID % Workers;

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
bool NorNetEdgePingReader::removeFile(const std::filesystem::path& dataFile,
                                      const std::smatch            match)
{
   if(match.size() == 3) {
      FileEntryTimePoint timeStamp;
      if(stringToTimePoint<FileEntryTimePoint>(match[2].str(), timeStamp, "%Y-%m-%d_%H-%M-%S")) {
         InputFileEntry inputFileEntry;
         inputFileEntry.TimeStamp     = timeStamp;
         inputFileEntry.MeasurementID = atol(match[1].str().c_str());
         inputFileEntry.DataFile      = dataFile;
         const int workerID = inputFileEntry.MeasurementID % Workers;

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
unsigned int NorNetEdgePingReader::fetchFiles(std::list<std::filesystem::path>& dataFileList,
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
void NorNetEdgePingReader::beginParsing(DatabaseClientBase& databaseClient,
                                        unsigned long long& rows)
{
   rows = 0;

   // ====== Generate import statement ======================================
   const DatabaseBackendType backend = databaseClient.getBackend();
   if(backend & DatabaseBackendType::SQL_Generic) {
      assert(databaseClient.statementIsEmpty());
      databaseClient.getStatement()
         << "INSERT INTO " << Table_measurement_generic_data
         << "(ts, mi_id, seq, xml_data, crc, stats) VALUES \n";
   }
   else {
      throw ImporterLogicException("Unknown output format");
   }
}


// ###### Finish parsing ####################################################
bool NorNetEdgePingReader::finishParsing(DatabaseClientBase& databaseClient,
                                         unsigned long long& rows)
{
   if(rows > 0) {
      // ====== Generate import statement ===================================
      const DatabaseBackendType backend = databaseClient.getBackend();
      if(backend & DatabaseBackendType::SQL_Generic) {
         if(rows > 0) {
            databaseClient.getStatement()
               << "\nON DUPLICATE KEY UPDATE stats=stats;\n";
            databaseClient.executeStatement();
         }
         else {
            databaseClient.clearStatement();
         }
      }
      else {
         throw ImporterLogicException("Unknown output format");
      }
      return true;
   }
   return false;
}


// ###### Parse input file ##################################################
void NorNetEdgePingReader::parseContents(
        DatabaseClientBase&                  databaseClient,
        unsigned long long&                  rows,
        boost::iostreams::filtering_istream& inputStream)
{
   const DatabaseBackendType backend = databaseClient.getBackend();
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
            throw ImporterReaderException("Too many columns in input file");
         }
         tuple[columns++] = inputLine.substr(start, end - start);
      }
      if(columns != NorNetEdgePingColumns) {
         throw ImporterReaderException("Too few columns in input file");
      }

      // ====== Generate import statement ===================================
      if(backend & DatabaseBackendType::SQL_Generic) {

         if(rows > 0) {
            databaseClient.getStatement() << ",\n";
         }
         databaseClient.getStatement()
            << "("
            << "'" << tuple[0] << "', "
            << std::stoul(tuple[1]) << ", "
            << std::stoul(tuple[2]) << ", "
            << "'" << tuple[3] << "', CRC32(xml_data), 10 + mi_id MOD 10)";
         rows++;
      }
      else {
         throw ImporterLogicException("Unknown output format");
      }
   }
}


// ###### Print reader status ###############################################
void NorNetEdgePingReader::printStatus(std::ostream& os)
{
   os << "NorNetEdgePing:" << std::endl;
   for(unsigned int w = 0; w < Workers; w++) {
      os << " - Work Queue #" << w + 1 << ": " << DataFileSet[w].size() << std::endl;
      // for(const InputFileEntry& inputFileEntry : DataFileSet[w]) {
      //    os << "  - " <<  inputFileEntry << std::endl;
      // }
   }
}



class Worker
{
   public:
   Worker(const unsigned int           workerID,
          BasicReader&                 reader,
          DatabaseClientBase&          databaseClient,
          const std::filesystem::path& importFilePath,
          const std::filesystem::path& goodFilePath,
          const std::filesystem::path& badFilePath,
          const ImportModeType         importMode);
   ~Worker();

   void start();
   void requestStop();
   void wakeUp();

   inline const std::string& getIdentification() const { return Identification; }


   private:
   void processFile(DatabaseClientBase&          databaseClient,
                    unsigned long long&          rows,
                    const std::filesystem::path& dataFile);
   void finishedFile(const std::filesystem::path& dataFile,
                     const bool                   success = true);
   void deleteEmptyDirectories(std::filesystem::path path);
   void deleteImportedFile(const std::filesystem::path& dataFile);
   void moveImportedFile(const std::filesystem::path& dataFile);
   void moveBadFile(const std::filesystem::path& dataFile);
   bool importFiles(const std::list<std::filesystem::path>& dataFileList);
   void run();

   std::atomic<bool>            StopRequested;
   const unsigned int           WorkerID;
   BasicReader&                 Reader;
   DatabaseClientBase&          DatabaseClient;
   const std::filesystem::path& ImportFilePath;
   const std::filesystem::path& GoodFilePath;
   const std::filesystem::path& BadFilePath;
   const ImportModeType         ImportMode;
   const std::string            Identification;
   std::thread                  Thread;
   std::mutex                   Mutex;
   std::condition_variable      Notification;
};


// ###### Constructor #######################################################
Worker::Worker(const unsigned int           workerID,
               BasicReader&                 reader,
               DatabaseClientBase&          databaseClient,
               const std::filesystem::path& importFilePath,
               const std::filesystem::path& goodFilePath,
               const std::filesystem::path& badFilePath,
               const ImportModeType         importMode)
   : WorkerID(workerID),
     Reader(reader),
     DatabaseClient(databaseClient),
     ImportFilePath(importFilePath),
     GoodFilePath(goodFilePath),
     BadFilePath(badFilePath),
     ImportMode(importMode),
     Identification(Reader.getIdentification() + "/" + std::to_string(WorkerID))
{
   StopRequested.exchange(false);
}


// ###### Destructor ########################################################
Worker::~Worker()
{
   requestStop();
   if(Thread.joinable()) {
      Thread.join();
   }
}


// ###### Start worker ######################################################
void Worker::start()
{
   StopRequested.exchange(false);
   Thread = std::thread(&Worker::run, this);
}


// ###### Stop worker #######################################################
void Worker::requestStop()
{
   StopRequested.exchange(true);
   wakeUp();
}


// ###### Wake up worker loop ###############################################
void Worker::wakeUp()
{
   Notification.notify_one();
}


// ###### Get list of input files ###########################################
void Worker::processFile(DatabaseClientBase&          databaseClient,
                         unsigned long long&          rows,
                         const std::filesystem::path& dataFile)
{
   std::ifstream                       inputFile;
   boost::iostreams::filtering_istream inputStream;

   // ====== Prepare input stream ===========================================
   inputFile.open(dataFile.string(), std::ios_base::in | std::ios_base::binary);
   if(dataFile.extension() == ".xz") {
      inputStream.push(boost::iostreams::lzma_decompressor());
   }
   else if(dataFile.extension() == ".bz2") {
      inputStream.push(boost::iostreams::bzip2_decompressor());
   }
   else if(dataFile.extension() == ".gz") {
      inputStream.push(boost::iostreams::gzip_decompressor());
   }
   inputStream.push(inputFile);

   // ====== Read contents ==================================================
   Reader.parseContents(databaseClient, rows, inputStream);
}


// ###### Remove empty directories ##########################################
void Worker::deleteEmptyDirectories(std::filesystem::path path)
{
   assert(is_subdir_of(path, ImportFilePath));

   std::error_code ec;
   while(path.parent_path() != ImportFilePath) {
      std::filesystem::remove(path, ec);
      if(ec) {
         break;
      }
      HPCT_LOG(trace) << getIdentification() << ": Deleted empty directory " << path;
      path = path.parent_path();
   }
}


// ###### Delete successfully imported file #################################
void Worker::deleteImportedFile(const std::filesystem::path& dataFile)
{
   try {
      std::filesystem::remove(dataFile);
      HPCT_LOG(trace) << getIdentification() << ": Deleted imported file " << dataFile;
      deleteEmptyDirectories(dataFile.parent_path());
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Deleting imported file " << dataFile << " failed: " << e.what();
   }
}


// ###### Move successfully imported file to good files #####################
void Worker::moveImportedFile(const std::filesystem::path& dataFile)
{
   assert(is_subdir_of(dataFile, ImportFilePath));
   const std::filesystem::path subdirs    = std::filesystem::relative(dataFile.parent_path(), ImportFilePath);
   const std::filesystem::path targetPath = GoodFilePath / subdirs;
   try {
      std::filesystem::create_directories(targetPath);
      std::filesystem::rename(dataFile, targetPath / dataFile.filename());
      HPCT_LOG(trace) << getIdentification() << ": Moved imported file "
                      << dataFile << " to " << targetPath;
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Moving imported file " << dataFile
                        << " to " << targetPath << " failed: " << e.what();
   }
}


// ###### Move bad file to bad files ########################################
void Worker::moveBadFile(const std::filesystem::path& dataFile)
{
   assert(is_subdir_of(dataFile, ImportFilePath));
   const std::filesystem::path subdirs    = std::filesystem::relative(dataFile.parent_path(), ImportFilePath);
   const std::filesystem::path targetPath = BadFilePath / subdirs;
   try {
      std::filesystem::create_directories(targetPath);
      std::filesystem::rename(dataFile, targetPath / dataFile.filename());
      HPCT_LOG(trace) << getIdentification() << ": Moved bad file "
                      << dataFile << " to " << targetPath;
   }
   catch(std::filesystem::filesystem_error& e) {
      HPCT_LOG(warning) << getIdentification() << ": Moving bad file " << dataFile
                        << " to " << targetPath << " failed: " << e.what();
   }
}


// ###### Delete finished input file ########################################
void Worker::finishedFile(const std::filesystem::path& dataFile,
                          const bool                   success)
{
   // ====== File has been imported successfully ===============================
   if(success) {
      // ====== Delete imported file ===========================================
      if(ImportMode == ImportModeType::DeleteImportedFiles) {
         deleteImportedFile(dataFile);
      }
      // ====== Move imported file =============================================
      else if(ImportMode == ImportModeType::MoveImportedFiles) {
         moveImportedFile(dataFile);
      }
      // ====== Keep imported file where it is =================================
      else  if(ImportMode == ImportModeType::KeepImportedFiles) {
         // Nothing to do here!
      }
   }
   // ====== File is bad ====================================================
   else {
      moveBadFile(dataFile);
   }

   // ====== Remove file from the reader ====================================
   // Need to extract the file name parts again, in order to find the entry:
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
   assert(std::regex_match(filename, match, Reader.getFileNameRegExp()));
   assert(Reader.removeFile(dataFile, match) == 1);
}


// ###### Import list of files ##############################################
bool Worker::importFiles(const std::list<std::filesystem::path>& dataFileList)
{
   const bool fastMode = (dataFileList.size() > 1);
   if(fastMode) {
      HPCT_LOG(debug) << getIdentification() << ": Trying to import "
                      << dataFileList.size() << " files in fast mode ...";
   }
   // for(const std::filesystem::path& d : dataFileList) {
   //    std::cout << "- " << d << "\n";
   // }

   unsigned long long rows = 0;
   try {
      // ====== Import multiple input files in one transaction ========
      DatabaseClient.startTransaction();
      Reader.beginParsing(DatabaseClient, rows);
      for(const std::filesystem::path& dataFile : dataFileList) {
         if(StopRequested) {
            break;
         }
         HPCT_LOG(trace) << getIdentification() << ": Parsing " << dataFile << " ...";
         try {
            processFile(DatabaseClient, rows, dataFile);
         }
         catch(ImporterReaderDataErrorException& exception) {
            HPCT_LOG(warning) << getIdentification() << ": Import in fast mode failed with reader data error: "
                              << exception.what();
            DatabaseClient.rollback();
            if(!fastMode) {
               finishedFile(dataFile, false);
            }
            return false;
         }
         catch(ImporterDatabaseDataErrorException& exception) {
            HPCT_LOG(warning) << getIdentification() << ": Import in fast mode failed with database data error: "
                              << exception.what();
            DatabaseClient.rollback();
            if(!fastMode) {
               finishedFile(dataFile, false);
            }
            return false;
         }
      }
      if(Reader.finishParsing(DatabaseClient, rows)) {
         DatabaseClient.commit();
         HPCT_LOG(debug) << getIdentification() << ": Committed " << rows << " rows";
      }
      else {
         std::cout << "Nothing to do!" << std::endl;
         HPCT_LOG(debug) << getIdentification() << ": Nothing to import!";
      }

      // ====== Delete input files ====================================
      HPCT_LOG(debug) << getIdentification() << ": Finishing " << dataFileList.size() << " input files ...";
      for(const std::filesystem::path& dataFile : dataFileList) {
         finishedFile(dataFile);
      }
      return true;
   }
   catch(ImporterDatabaseException& exception) {
      HPCT_LOG(warning) << getIdentification() << ": Import in "
                        << ((fastMode == true) ? "fast" : "slow") << " mode failed: "
                        << exception.what();
      DatabaseClient.rollback();
   }
   return false;
}


// ###### Worker loop #######################################################
void Worker::run()
{
   std::unique_lock lock(Mutex);

   while(!StopRequested) {
      // ====== Look for new input files ====================================
      HPCT_LOG(trace) << getIdentification() << ": Looking for new input files ...";

      // ====== Fast import: try to combine files ===========================
      std::list<std::filesystem::path> dataFileList;
      unsigned int files = Reader.fetchFiles(dataFileList, WorkerID, Reader.getMaxTransactionSize());
      while( (files > 0) && (!StopRequested) ) {
         // ====== Fast Mode ================================================
         if(!importFiles(dataFileList)) {

            // ====== Slow Mode =============================================
            if(files > 1) {
               HPCT_LOG(debug) << getIdentification() << ": Trying to import "
                               << dataFileList.size() << " files in slow mode ...";
               for(const std::filesystem::path& dataFile : dataFileList) {
                  std::list<std::filesystem::path> singleFileList;
                  if(StopRequested) {
                     break;
                  }
                  singleFileList.push_back(dataFile);
                  assert(singleFileList.size() == 1);
                  importFiles(singleFileList);
               }
            }
         }

        files = Reader.fetchFiles(dataFileList, WorkerID, Reader.getMaxTransactionSize());
//         puts("????");
//         files = 0;
      }

      // ====== Wait for new data ===========================================
      if(!StopRequested) {
         HPCT_LOG(trace) << getIdentification() << ": Sleeping ...";
         Notification.wait(lock);
         HPCT_LOG(trace) << getIdentification() << ": Wakeup!";
      }
   }
}



class UniversalImporter
{
   public:
   UniversalImporter(boost::asio::io_service&     ioService,
                     const std::filesystem::path& importFilePath,
                     const std::filesystem::path& goodFilePath,
                     const std::filesystem::path& badFilePath,
                     const ImportModeType         importMode,
                     const unsigned int           maxDepth = 5);
   ~UniversalImporter();

   void addReader(BasicReader&         reader,
                  DatabaseClientBase** databaseClientArray,
                  const size_t         databaseClients);
   void removeReader(BasicReader& reader);
   void lookForFiles();
   bool start();
   void stop();
   void run();
   void printStatus(std::ostream& os = std::cout);

   private:
   void handleSignalEvent(const boost::system::error_code& ec,
                          const int                        signalNumber);
#ifdef __linux__
   void handleINotifyEvent(const boost::system::error_code& ec,
                           const std::size_t                length);
#endif
   void lookForFiles(const std::filesystem::path& importFilePath,
                     const unsigned int           maxDepth);
   bool addFile(const std::filesystem::path& dataFile);
   bool removeFile(const std::filesystem::path& dataFile);

   struct WorkerMapping {
      BasicReader* Reader;
      unsigned int WorkerID;
   };
   friend bool operator<(const UniversalImporter::WorkerMapping& a,
                         const UniversalImporter::WorkerMapping& b);

   boost::asio::io_service&               IOService;
   const std::filesystem::path            ImportFilePath;
   const std::filesystem::path            GoodFilePath;
   const std::filesystem::path            BadFilePath;
   const ImportModeType                   ImportMode;
   const unsigned int                     MaxDepth;

   boost::asio::signal_set                Signals;
   std::list<BasicReader*>                ReaderList;
   std::map<const WorkerMapping, Worker*> WorkerMap;
#ifdef __linux__
   int                                    INotifyFD;
   std::set<int>                          INotifyWatchDescriptors;
   boost::asio::posix::stream_descriptor  INotifyStream;
   char                                   INotifyEventBuffer[65536 * sizeof(inotify_event)];
#endif
};


// ###### < operator for sorting ############################################
bool operator<(const UniversalImporter::WorkerMapping& a,
               const UniversalImporter::WorkerMapping& b) {
   // ====== Level 1: Reader ================================================
   if(a.Reader < b.Reader) {
      return true;
   }
   else if(a.Reader == b.Reader) {
      // ====== Level 2: WorkerID ===========================================
      if(a.WorkerID < b.WorkerID) {
         return true;
      }
   }
   return false;
}


// ###### Constructor #######################################################
UniversalImporter::UniversalImporter(boost::asio::io_service&     ioService,
                                     const std::filesystem::path& importFilePath,
                                     const std::filesystem::path& goodFilePath,
                                     const std::filesystem::path& badFilePath,
                                     const ImportModeType         importMode,
                                     const unsigned int           maxDepth)
 : IOService(ioService),
   ImportFilePath(std::filesystem::canonical(std::filesystem::absolute(importFilePath))),
   GoodFilePath(std::filesystem::canonical(std::filesystem::absolute(goodFilePath))),
   BadFilePath(std::filesystem::canonical(std::filesystem::absolute(badFilePath))),
   ImportMode(importMode),
   MaxDepth(maxDepth),
   Signals(IOService, SIGINT, SIGTERM),
   INotifyStream(IOService)
{
#ifdef __linux__
   INotifyFD = -1;
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
   Signals.async_wait(std::bind(&UniversalImporter::handleSignalEvent, this,
                                std::placeholders::_1,
                                std::placeholders::_2));

   // ====== Set up INotify =================================================
#ifdef __linux__
   INotifyFD = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
   assert(INotifyFD > 0);
   INotifyStream.assign(INotifyFD);
   int wd = inotify_add_watch(INotifyFD, ImportFilePath.c_str(),
                              IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
   if(wd < 0) {
      HPCT_LOG(error) << "Unable to configure inotify: " <<strerror(errno);
      return false;
   }
   INotifyWatchDescriptors.insert(wd);

   INotifyStream.async_read_some(boost::asio::buffer(&INotifyEventBuffer, sizeof(INotifyEventBuffer)),
                                 std::bind(&UniversalImporter::handleINotifyEvent, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
#else
#error FIXME!
#endif

   // ====== Look for files =================================================
   HPCT_LOG(info) << "Looking for input files ...";
   lookForFiles();
   printStatus();

   // ====== Start workers ==================================================
   HPCT_LOG(info) << "Starting " << WorkerMap.size() << " worker threads ...";
   for(std::map<const WorkerMapping, Worker*>::iterator workerMappingIterator = WorkerMap.begin();
       workerMappingIterator != WorkerMap.end(); workerMappingIterator++) {
      Worker* worker = workerMappingIterator->second;
      worker->start();
   }


   return true;
}


// ###### Stop importer #####################################################
void UniversalImporter::stop()
{
   // ====== Remove INotify =================================================
#ifdef __linux__
   if(INotifyFD >= 0) {
      std::set<int>::iterator iterator = INotifyWatchDescriptors.begin();
      while(iterator != INotifyWatchDescriptors.end()) {
         inotify_rm_watch(INotifyFD, *iterator);
         INotifyWatchDescriptors.erase(iterator);
         iterator = INotifyWatchDescriptors.begin();
      }
      close(INotifyFD);
      INotifyFD = -1;
   }
#endif

   // ====== Remove readers =================================================
   for(std::list<BasicReader*>::iterator readerIterator = ReaderList.begin(); readerIterator != ReaderList.end(); ) {
      removeReader(**readerIterator);
      readerIterator = ReaderList.begin();
   }
}


// ###### Handle signal #####################################################
void UniversalImporter::handleSignalEvent(const boost::system::error_code& ec,
                                          const int                        signalNumber)
{
   if(ec != boost::asio::error::operation_aborted) {
      puts("\n*** Shutting down! ***\n");   // Avoids a false positive from Helgrind.
      IOService.stop();
   }
}


#ifdef __linux
// ###### Handle signal #####################################################
void UniversalImporter::handleINotifyEvent(const boost::system::error_code& ec,
                                           const std::size_t                length)
{
   // ====== Handle events ==================================================
   unsigned long p = 0;
   while(p < length) {
      const inotify_event* event = (const inotify_event*)&INotifyEventBuffer[p];

      // ====== Event for directory =========================================
      if(event->mask & IN_ISDIR) {
         if(event->mask & IN_CREATE) {
            const std::filesystem::path importFilePath = ImportFilePath / event->name;
            HPCT_LOG(trace) << "INotify for new data directory: " << importFilePath;
            const int wd = inotify_add_watch(INotifyFD, importFilePath.c_str(),
                                             IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
            INotifyWatchDescriptors.insert(wd);
         }
         else if(event->mask & IN_DELETE) {
            const std::filesystem::path importFilePath = ImportFilePath / event->name;
            HPCT_LOG(trace) << "INotify for deleted data directory: " << importFilePath;
            INotifyWatchDescriptors.erase(event->wd);
         }
      }

      // ====== Event for file ==============================================
      else {
         if(event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
            const std::filesystem::path dataFile(event->name);
            HPCT_LOG(trace) << "INotify event for new file " << dataFile;
            addFile(dataFile);
         }
         else if(event->mask & IN_DELETE) {
            const std::filesystem::path dataFile(event->name);
            HPCT_LOG(trace) << "INotify event for deleted file " << dataFile;
            removeFile(dataFile);
         }
      }

      p += sizeof(inotify_event) + event->len;
   }

   // ====== Wait for more events ===========================================
   INotifyStream.async_read_some(boost::asio::buffer(&INotifyEventBuffer, sizeof(INotifyEventBuffer)),
                                 std::bind(&UniversalImporter::handleINotifyEvent, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
}
#endif


// ###### Add reader ########################################################
void UniversalImporter::addReader(BasicReader&         reader,
                                  DatabaseClientBase** databaseClientArray,
                                  const size_t         databaseClients)
{
   ReaderList.push_back(&reader);
   for(unsigned int w = 0; w < databaseClients; w++) {
      Worker* worker = new Worker(w, reader, *databaseClientArray[w],
                                  ImportFilePath, GoodFilePath, BadFilePath,
                                  ImportMode);
      assert(worker != nullptr);
      WorkerMapping workerMapping;
      workerMapping.Reader   = &reader;
      workerMapping.WorkerID = w;
      WorkerMap.insert(std::pair<const WorkerMapping, Worker*>(workerMapping, worker));
   }
}


// ###### Remove reader #####################################################
void UniversalImporter::removeReader(BasicReader& reader)
{
   // ====== Remove Reader from reader list =================================
   for(std::list<BasicReader*>::iterator readerIterator = ReaderList.begin();
       readerIterator != ReaderList.end();
       readerIterator++) {
      if(*readerIterator == &reader) {
         ReaderList.erase(readerIterator);
         break;
      }
   }

   // ====== Remove worker mapping ==========================================
   for(std::map<const WorkerMapping, Worker*>::iterator workerMappingIterator = WorkerMap.begin();
       workerMappingIterator != WorkerMap.end(); ) {
      if(workerMappingIterator->first.Reader == &reader) {
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
   lookForFiles(ImportFilePath, MaxDepth);
}


// ###### Look for input files (limited directory traversal) ################
void UniversalImporter::lookForFiles(const std::filesystem::path& importFilePath,
                                     const unsigned int           maxDepth)
{
   for(const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(importFilePath)) {
      if(dirEntry.is_regular_file()) {
         addFile(dirEntry.path());
      }
      else if(dirEntry.is_directory()) {
#ifdef __linux
         const int wd = inotify_add_watch(INotifyFD, dirEntry.path().c_str(),
                                          IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO);
         INotifyWatchDescriptors.insert(wd);
#endif
         if(maxDepth > 1) {
            lookForFiles(dirEntry, maxDepth - 1);
         }
      }
   }
}


// ###### Add input file ####################################################
bool UniversalImporter::addFile(const std::filesystem::path& dataFile)
{
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
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
               return true;
            }
         }
      }
   }
   return false;
}


// ###### Add input file ####################################################
bool UniversalImporter::removeFile(const std::filesystem::path& dataFile)
{
   const std::string& filename = dataFile.filename().string();
   std::smatch        match;
   for(BasicReader* reader : ReaderList) {
      if(std::regex_match(filename, match, reader->getFileNameRegExp())) {
         if(reader->removeFile(dataFile, match)) {
            return true;
         }
         break;
      }
   }
   return false;
}


// ###### Print importer status #############################################
void UniversalImporter::printStatus(std::ostream& os)
{
   for(BasicReader* reader : ReaderList) {
      reader->printStatus(os);
   }
}



#include "t4.h"



int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   boost::asio::io_service ioService;

   unsigned int          logLevel                  = boost::log::trivial::severity_level::trace;
   unsigned int          pingWorkers               = 1;
   unsigned int          metadataWorkers           = 1;
   unsigned int          pingTransactionSize       = 4;
   unsigned int          metadataTransactionSize   = 256;
   std::filesystem::path databaseConfigurationFile = "/home/dreibh/soyuz.conf";


   // ====== Read database configuration ====================================
   DatabaseConfiguration databaseConfiguration;
   if(!databaseConfiguration.readConfiguration(databaseConfigurationFile)) {
      exit(1);
   }

// ????
// databaseConfiguration.setBackend(DatabaseBackendType::SQL_Debug);

   databaseConfiguration.printConfiguration(std::cout);


//    UniversalImporter importer(".", 5);
//
//    HiPerConTracerPingReader pingReader;
//    importer.addReader(&pingReader);
//    HiPerConTracerTracerouteReader tracerouteReader;
//    importer.addReader(&tracerouteReader);


   // ====== Initialise importer ============================================
   initialiseLogger(logLevel);
   UniversalImporter importer(ioService,
                              databaseConfiguration.getImportFilePath(),
                              databaseConfiguration.getGoodFilePath(),
                              databaseConfiguration.getBadFilePath(),
                              databaseConfiguration.getImportMode(),
                              5);


   // ====== Initialise database clients and readers ========================
   // ------ NorNet Edge Ping -----------------------------
   DatabaseClientBase*   pingDatabaseClients[pingWorkers];
   NorNetEdgePingReader* nnePingReader = nullptr;
   if(pingWorkers > 0) {
      for(unsigned int i = 0; i < pingWorkers; i++) {
         pingDatabaseClients[i] = databaseConfiguration.createClient();
         assert(pingDatabaseClients[i] != nullptr);
         if(!pingDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      nnePingReader = new NorNetEdgePingReader(pingWorkers, pingTransactionSize);
      assert(nnePingReader != nullptr);
      importer.addReader(*nnePingReader,
                         (DatabaseClientBase**)&pingDatabaseClients, pingWorkers);
   }

   // ------ NorNet Edge Metadata -------------------------
   DatabaseClientBase*       metadataDatabaseClients[metadataWorkers];
   NorNetEdgeMetadataReader* nneMetadataReader = nullptr;
   if(metadataWorkers > 0) {
      for(unsigned int i = 0; i < metadataWorkers; i++) {
         metadataDatabaseClients[i] = databaseConfiguration.createClient();
         assert(metadataDatabaseClients[i] != nullptr);
         if(!metadataDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      nneMetadataReader = new NorNetEdgeMetadataReader(metadataWorkers, metadataTransactionSize);
      assert(nneMetadataReader != nullptr);
      importer.addReader(*nneMetadataReader,
                         (DatabaseClientBase**)&metadataDatabaseClients, metadataWorkers);
   }


   // ====== Main loop ======================================================
   if(importer.start() == false) {
      exit(1);
   }
   ioService.run();
   importer.stop();


   // ====== Clean up =======================================================
   if(metadataWorkers > 0) {
      delete nneMetadataReader;
      nneMetadataReader = nullptr;
      for(unsigned int i = 0; i < metadataWorkers; i++) {
         delete metadataDatabaseClients[i];
         metadataDatabaseClients[i] = nullptr;
      }
   }
   if(pingWorkers > 0) {
      delete nnePingReader;
      nnePingReader = nullptr;
      for(unsigned int i = 0; i < pingWorkers; i++) {
         delete pingDatabaseClients[i];
         pingDatabaseClients[i] = nullptr;
      }
   }
   return 0;
}
