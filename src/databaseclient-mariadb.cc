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

#include "databaseclient-mariadb.h"
#include "importer-exception.h"
#include "logger.h"


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


// ###### Close connection to database ######################################
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


// ###### Reconnect connection to database ##################################
void MariaDBClient::reconnect()
{
   Connection->reconnect();
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
   std::cerr << statement;

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
void MariaDBClient::startTransaction()
{
   try {
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
void MariaDBClient::executeUpdate(const std::string& statement)
{
   try {
      Statement->executeUpdate(statement);
   }
   catch(const sql::SQLException& exception) {
      handleSQLException(exception, "Execute", statement);
   }
}
