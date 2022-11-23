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

// Ubuntu: libmysqlcppconn-dev
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>


REGISTER_BACKEND(DatabaseBackendType::SQL_MariaDB, "MariaDB", MariaDBClient)
REGISTER_BACKEND_ALIAS(DatabaseBackendType::SQL_MariaDB, "MySQL", MariaDBClient, 2)


// ###### Constructor #######################################################
MariaDBClient::MariaDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   Driver = get_driver_instance();
   assert(Driver != nullptr);
   Connection  = nullptr;
   Transaction = nullptr;
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
   sql::ConnectOptionsMap connectionProperties;
   connectionProperties["hostName"] = Configuration.getServer();
   connectionProperties["userName"] = Configuration.getUser();
   connectionProperties["password"] = Configuration.getPassword();
   connectionProperties["schema"]   = Configuration.getDatabase();
   connectionProperties["port"]     = (Configuration.getPort() != 0) ? Configuration.getPort() : 3306;
   if(Configuration.getCAFile().size() > 0) {
      connectionProperties["sslCA"]   = Configuration.getCAFile();
   }
   if(Configuration.getClientCertFile().size() > 0) {
      connectionProperties["sslCert"] = Configuration.getClientCertFile();
   }
   connectionProperties["sslVerify"]       = true;
   connectionProperties["sslEnforce"]      = true;
   connectionProperties["OPT_TLS_VERSION"] = "TLSv1.3";
   connectionProperties["OPT_RECONNECT"]   = true;
   connectionProperties["CLIENT_COMPRESS"] = true;

   assert(Connection == nullptr);
   try {
      // ====== Connect to database =========================================
      Connection = Driver->connect(connectionProperties);
      assert(Connection != nullptr);
      Connection->setSchema(Configuration.getDatabase().c_str());
      // SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED
      Connection->setTransactionIsolation(sql::TRANSACTION_READ_COMMITTED);
      Connection->setAutoCommit(false);

      // ====== Create statement ============================================
      Transaction = Connection->createStatement();
      assert(Transaction != nullptr);
   }
   catch(const sql::SQLException& e) {
      HPCT_LOG(error) << "Unable to connect MariaDB client to " << Configuration.getServer() << ": " << e.what();
      close();
      return false;
   }

   return true;
}


// ###### Close connection to database ######################################
void MariaDBClient::close()
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
void MariaDBClient::reconnect()
{
   Connection->reconnect();
}


// ###### Handle SQLException ###############################################
void MariaDBClient::handleDatabaseException(const sql::SQLException& exception,
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
   //  Based on mysql/connector/errors.py:
   if( (e == "42") || (e == "23") || (e == "22") || (e == "XA")) {
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
      Transaction->execute("START TRANSACTION");
   }
   catch(const sql::SQLException& exception) {
      handleDatabaseException(exception, "Start of transaction");
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
         handleDatabaseException(exception, "Commit");
      }
   }

   // ====== Commit transaction =============================================
   else {
      try {
         Connection->rollback();
      }
      catch(const sql::SQLException& exception) {
         handleDatabaseException(exception, "Rollback");
      }
   }
}


// ###### Execute statement #################################################
void MariaDBClient::executeUpdate(Statement& statement)
{
   assert(statement.isValid());

   try {
      Transaction->executeUpdate(statement.str());
   }
   catch(const sql::SQLException& exception) {
      handleDatabaseException(exception, "Execute", statement.str());
   }

   statement.clear();
}
