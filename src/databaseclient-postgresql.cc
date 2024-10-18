// ==========================================================================
//     _   _ _ ____            ____          _____
//    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
//    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
//    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
//    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
//
//       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
//                 https://www.nntb.no/~dreibh/hipercontracer/
// ==========================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2025 by Thomas Dreibholz
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

#include "databaseclient-postgresql.h"
#include "logger.h"


// NOTE: The registration was moved to database-configuration.cc, due to linking issues!
// REGISTER_BACKEND(DatabaseBackendType::SQL_PostgreSQL, "PostgreSQL", PostgreSQLClient)


// ###### Constructor #######################################################
PostgreSQLClient::PostgreSQLClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   Connection  = nullptr;
   Transaction = nullptr;
}


// ###### Destructor ########################################################
PostgreSQLClient::~PostgreSQLClient()
{
   close();
}


// ###### Get backend #######################################################
const DatabaseBackendType PostgreSQLClient::getBackend() const
{
   return DatabaseBackendType::SQL_PostgreSQL;
}


// ###### Prepare connection to database ####################################
bool PostgreSQLClient::open()
{
   assert(Connection == nullptr);

   // ====== Prepare parameters =============================================
   const char* ssl_mode = "verify-full";
   if(Configuration.getConnectionFlags() & DisableTLS) {
      ssl_mode = "disable";
      HPCT_LOG(warning) << "TLS explicitly disabled. CONFIGURE TLS PROPERLY!!";
   }
   else if(Configuration.getConnectionFlags() & ConnectionFlags::AllowInvalidCertificate) {
      ssl_mode = "require";
      HPCT_LOG(warning) << "TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!";
   }
   else if(Configuration.getConnectionFlags() & ConnectionFlags::AllowInvalidHostname) {
      ssl_mode = "verify-ca";
      HPCT_LOG(warning) << "TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!";
   }
   if(Configuration.getCertKeyFile().size() > 0) {
      HPCT_LOG(error) << "PostgreSQL backend expects separate certificate and key files, not one certificate+key file!";
      return false;
   }

   const std::string parameters =
      "host="         + Configuration.getServer() +
      " port="        + std::to_string((Configuration.getPort() != 0) ? Configuration.getPort() : 5432) +
      " user="        + Configuration.getUser() +
      " password="    + Configuration.getPassword() +
      " dbname="      + Configuration.getDatabase() +
      " sslmode="     + ssl_mode +
      " sslrootcert=" + Configuration.getCAFile() +
      " sslcrl="      + Configuration.getCRLFile() +
      " sslcert="     + Configuration.getCertFile() +
      " sslkey="      + Configuration.getKeyFile();

   try {
      // ====== Connect to database =========================================
      Connection = new pqxx::connection(parameters.c_str());
      assert(Connection != nullptr);
   }
   catch(const pqxx::failure& e) {
      HPCT_LOG(error) << "Unable to connect PostgreSQL client to " << parameters
                      << ": " << e.what();
      close();
      return false;
   }

   return true;
}


// ###### Close connection to database ######################################
void PostgreSQLClient::close()
{
   if(Transaction) {
      delete Transaction;
      Transaction = nullptr;
   }
   if(Connection != nullptr) {
      delete Connection;
      Connection = nullptr;
   }
}


// ###### Handle SQLException ###############################################
void PostgreSQLClient::handleDatabaseException(const pqxx::failure& exception,
                                               const std::string&   where,
                                               const std::string&   statement)
{
   // ====== Log error ======================================================
   std::string what = where + ": " + std::string(exception.what());
   const pqxx::sql_error* sqlError = dynamic_cast<const pqxx::sql_error*>(&exception);
   if(sqlError != nullptr) {
      what += "; SQL: " + std::string(sqlError->query());
   }
   HPCT_LOG(error) << what;
   HPCT_LOG(debug) << statement;

   // ====== Throw exception ================================================
   // Query error
   if( (Connection != nullptr) && (Connection->is_open()) && (sqlError != nullptr) ) {
      // For this type, the input file should be moved to the bad directory.
      throw ResultsDatabaseDataErrorException(what);
   }
   // Other error
   else {
      throw ResultsDatabaseException(what);
   }
}


// ###### Begin transaction #################################################
void PostgreSQLClient::startTransaction()
{
   assert(Transaction == nullptr);

   // ====== Create transaction =============================================
   try {
      if(Connection == nullptr) {
         throw ResultsDatabaseException("Not connected");
      }
      Transaction = new pqxx::work(*Connection);
   }
   catch(const pqxx::failure& exception) {
      handleDatabaseException(exception, "New Transaction");
   }
   assert(Transaction != nullptr);
}


// ###### End transaction ###################################################
void PostgreSQLClient::endTransaction(const bool commit)
{
   if(Transaction != nullptr) {
      // ====== Commit transaction ==========================================
      if(commit) {
         try {
            Transaction->commit();
         }
         catch(const pqxx::failure& exception) {
            handleDatabaseException(exception, "Commit");
         }
      }

      // ====== Commit transaction ==========================================
      else {
         try {
            Transaction->abort();
         }
         catch(const pqxx::failure& exception) {
            handleDatabaseException(exception, "Rollback");
         }
      }

      delete Transaction;
      Transaction = nullptr;
   }
   else {
      // Only rollback without transaction (i.e. nothing to do) is okay here!
      assert(commit == false);
   }
}


// ###### Execute statement #################################################
void PostgreSQLClient::executeUpdate(Statement& statement)
{
   assert(statement.isValid());
   assert(Transaction != nullptr);

   try {
      Transaction->exec(statement.str());
   }
   catch(const pqxx::failure& exception) {
      handleDatabaseException(exception, "Execute", statement.str());
   }

   statement.clear();
}


// ###### Execute statement #################################################
void PostgreSQLClient::executeQuery(Statement& statement)
{
   assert(statement.isValid());

   try {
      if(Transaction == nullptr) {
         Transaction = new pqxx::work(*Connection);
      }
      ResultSet   = Transaction->exec(statement.str());
      ResultIndex = ~((pqxx::result::size_type)0);
   }
   catch(const pqxx::failure& exception) {
      handleDatabaseException(exception, "Execute", statement.str());
   }

   statement.clear();
}


// ###### Fetch next tuple ##################################################
bool PostgreSQLClient::fetchNextTuple()
{
   if(ResultIndex == ~((pqxx::result::size_type)0)) {
      ResultIndex = 0;
   }
   else {
      ResultIndex++;
   }
   return ResultIndex < ResultSet.size();
}


// ###### Get integer value #################################################
int32_t PostgreSQLClient::getInteger(unsigned int column) const
{
   assert(ResultIndex < ResultSet.size());
   assert(column > 0);
   return ResultSet[ResultIndex][column - 1].as<int32_t>();
}


// ###### Get big integer value #############################################
int64_t PostgreSQLClient::getBigInt(unsigned int column) const
{
   assert(ResultIndex < ResultSet.size());
   assert(column > 0);
   return ResultSet[ResultIndex][column - 1].as<int64_t>();
}


// ###### Get string value ##################################################
std::string PostgreSQLClient::getString(unsigned int column) const
{
   assert(ResultIndex < ResultSet.size());
   assert(column > 0);
   return ResultSet[ResultIndex][column - 1].as<std::string>();
}
