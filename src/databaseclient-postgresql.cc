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

#include "databaseclient-postgresql.h"
#include "importer-exception.h"
#include "logger.h"


REGISTER_BACKEND(DatabaseBackendType::SQL_PostgreSQL, "PostgreSQL", PostgreSQLClient)


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
      " sslmode="     + ssl_mode +
      " sslrootcert=" + Configuration.getCAFile() +
      " sslcrl="      + Configuration.getCRLFile() +
      " sslcert="     + Configuration.getCertFile() +
      " sslkey="      + Configuration.getKeyFile() +
      " user="        + Configuration.getUser() +
      " password="    + Configuration.getPassword() +
      " dbname="      + Configuration.getDatabase();

   assert(Connection == nullptr);
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


// ###### Reconnect connection to database ##################################
void PostgreSQLClient::reconnect()
{
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
   if( (Connection->is_open()) && (sqlError != nullptr) ) {
      // For this type, the input file should be moved to the bad directory.
      throw ImporterDatabaseDataErrorException(what);
   }
   // Other error
   else {
      throw ImporterDatabaseException(what);
   }
}


// ###### Begin transaction #################################################
void PostgreSQLClient::startTransaction()
{
   assert(Transaction == nullptr);

   // ====== Create transaction =============================================
   try {
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
