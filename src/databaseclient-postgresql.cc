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


// ###### Constructor #######################################################
PostgreSQLClient::PostgreSQLClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   Driver = get_driver_instance();
   assert(Driver != nullptr);
   Connection = nullptr;
   Statement  = nullptr;
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
      "host="     + Configuration.getServer() +
      " port="     + boost::str(boost::format("%d") % schedulerDBPort) +
      " user="     + Configuration.getUser() +
      " password=" + Configuration.getPassword() +
      " dbname="   + Configuration.getDatabase();

   assert(Connection == nullptr);
   try {
      // ====== Connect to database =========================================
      Connection = new pqxx::lazyconnection(parameters.c_str());
      assert(Connection != nullptr);

      // ====== Create statement ============================================
      Statement = new pqxx::work(*Connection);
      assert(Statement != nullptr);
   }
   catch(const pqxx::pqxx_exception& e) {
      HPCT_LOG(error) << "Unable to connect PostgreSQL client to " << parameters << ": " << e.what();
      close();
      return false;
   }

   return true;
}


// ###### Close connection to database ######################################
void PostgreSQLClient::close()
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


// ###### Reconnect connection to database ##################################
void PostgreSQLClient::reconnect()
{
   Connection->reconnect();
}


// ###### Handle SQLException ###############################################
void PostgreSQLClient::handleDatabaseException(const pqxx::pqxx_exception& exception,
                                               const std::string&       where,
                                               const std::string&       statement)
{
   // ====== Log error ======================================================
   const std::string what = where + " error " +
                               exception.getSQLState() + "/E" +
                               std::to_string(exception.getErrorCode()) + ": " +
                               exception.what();
   HPCT_LOG(error) << what;
   HPCT_LOG(debug) << statement;

   // ====== Throw exception ================================================
   const std::string e = exception.getSQLState().substr(0, 2);
   // Integrity Error, according to mysql/connector/errors.py
   if( (e == "23") || (e == "22") || (e == "XA")) {
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
   try {
      Statement->execute("START TRANSACTION");
   }
   catch(const pqxx::pqxx_exception& exception) {
      handleSQLException(exception, "Start of transaction");
   }
}


// ###### End transaction ###################################################
void PostgreSQLClient::endTransaction(const bool commit)
{
   // ====== Commit transaction =============================================
   if(commit) {
      try {
         Connection->commit();
      }
      catch(const pqxx::pqxx_exception& exception) {
         handleSQLException(exception, "Commit");
      }
   }

   // ====== Commit transaction =============================================
   else {
      try {
         Connection->rollback();
      }
      catch(const pqxx::pqxx_exception& exception) {
         handleSQLException(exception, "Rollback");
      }
   }
}


// ###### Execute statement #################################################
void PostgreSQLClient::executeUpdate(const std::string& statement)
{
   try {
      Statement->executeUpdate(statement);
   }
   catch(const pqxx::pqxx_exception& exception) {
      handleSQLException(exception, "Execute", statement);
   }
}
