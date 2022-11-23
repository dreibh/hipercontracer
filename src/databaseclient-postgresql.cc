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
   const std::string parameters =
      "host="      + Configuration.getServer() +
      " port="     + std::to_string((Configuration.getPort() == 0) ? Configuration.getPort() : 5432) +
      " user="     + Configuration.getUser() +
      " password=" + Configuration.getPassword() +
      " dbname="   + Configuration.getDatabase();

   assert(Connection == nullptr);
   try {
      // ====== Connect to database =========================================
      Connection = new pqxx::lazyconnection(parameters.c_str());
      assert(Connection != nullptr);
   }
   catch(const pqxx::pqxx_exception& e) {
      HPCT_LOG(error) << "Unable to connect PostgreSQL client to " << parameters;
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
void PostgreSQLClient::handleDatabaseException(const pqxx::pqxx_exception& exception,
                                               const std::string&          where,
                                               const std::string&          statement)
{
   // ====== Log error ======================================================
   std::string what = where;
   const std::exception* error = dynamic_cast<const std::exception*>(&exception.base());
   if(error != nullptr) {
       what += ": " + std::string(error->what());
   }
   const pqxx::sql_error* sqlError = dynamic_cast<const pqxx::sql_error*>(&exception.base());
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
   Transaction = new pqxx::work(*Connection);
   assert(Transaction != nullptr);
}


// ###### End transaction ###################################################
void PostgreSQLClient::endTransaction(const bool commit)
{
   assert(Transaction != nullptr);

   // ====== Commit transaction =============================================
   if(commit) {
      try {
         Transaction->commit();
      }
      catch(const pqxx::pqxx_exception& exception) {
         handleDatabaseException(exception, "Commit");
      }
   }

   // ====== Commit transaction =============================================
   else {
      try {
         Transaction->abort();
      }
      catch(const pqxx::pqxx_exception& exception) {
         handleDatabaseException(exception, "Rollback");
      }
   }

   delete Transaction;
   Transaction = nullptr;
}


// ###### Execute statement #################################################
void PostgreSQLClient::executeUpdate(Statement& statement)
{
   assert(statement.isValid());
   assert(Transaction != nullptr);

   try {
      Transaction->exec(statement.str());
   }
   catch(const pqxx::pqxx_exception& exception) {
      handleDatabaseException(exception, "Execute", statement.str());
   }

   statement.clear();
}
