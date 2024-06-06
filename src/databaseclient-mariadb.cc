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
// Copyright (C) 2015-2024 by Thomas Dreibholz
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
#include "logger.h"


// NOTE: The registration was moved to database-configuration.cc, due to linking issues!
// REGISTER_BACKEND(DatabaseBackendType::SQL_MariaDB, "MariaDB", MariaDBClient)
// REGISTER_BACKEND_ALIAS(DatabaseBackendType::SQL_MariaDB, "MySQL", MariaDBClient, 2)


// ###### Constructor #######################################################
MariaDBClient::MariaDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   mysql_thread_init();

//    Driver = get_driver_instance();
//    assert(Driver != nullptr);
   Connection  = nullptr;
//    Transaction = nullptr;
//    ResultSet   = nullptr;
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
   bool sslEnforce = true;
   bool sslVerify  = true;
   if(Configuration.getConnectionFlags() & DisableTLS) {
      sslEnforce = false;
      HPCT_LOG(warning) << "TLS explicitly disabled. CONFIGURE TLS PROPERLY!!";
   }
   else if(Configuration.getConnectionFlags() & (ConnectionFlags::AllowInvalidCertificate|ConnectionFlags::AllowInvalidHostname)) {
      sslVerify = false;
      HPCT_LOG(warning) << "TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!";
   }

   Connection = mysql_init(nullptr);
   assert(Connection != nullptr);

abort();
#if 0
   sql::ConnectOptionsMap connectionProperties;
   connectionProperties["hostName"] = Configuration.getServer();
   connectionProperties["userName"] = Configuration.getUser();
   connectionProperties["password"] = Configuration.getPassword();
   connectionProperties["schema"]   = Configuration.getDatabase();
   connectionProperties["port"]     = (Configuration.getPort() != 0) ? Configuration.getPort() : 3306;
   if(Configuration.getCAFile().size() > 0) {
      connectionProperties["sslCA"]   = Configuration.getCAFile();
   }
   if(Configuration.getCertFile().size() > 0) {
      connectionProperties["sslCert"] = Configuration.getCertFile();
   }
   if(Configuration.getCertKeyFile().size() > 0) {
      HPCT_LOG(error) << "MySQL/MariaDB backend expects separate certificate and key files, not one certificate+key file!";
      return false;
   }
   if( (sslVerify) && (Configuration.getCRLFile().size() > 0) ) {
      connectionProperties["sslCRL"] = Configuration.getCRLFile();
   }
   connectionProperties["sslVerify"]       = sslVerify;
   connectionProperties["sslEnforce"]      = sslEnforce;
   connectionProperties["OPT_TLS_VERSION"] = "TLSv1.3";
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
      HPCT_LOG(error) << "Unable to connect MySQL/MariaDB client to "
                      << Configuration.getServer() << ": " << e.what();
      close();
      return false;
   }

   return true;
#endif
}


// ###### Close connection to database ######################################
void MariaDBClient::close()
{
//    if(ResultSet != nullptr) {
//       delete ResultSet;
//       ResultSet = nullptr;
//    }
//    if(Transaction) {
//       delete Transaction;
//       Transaction = nullptr;
//    }
   if(Connection != nullptr) {
      mysql_close(Connection);
      Connection = nullptr;
   }
}


// ###### Reconnect connection to database ##################################
void MariaDBClient::reconnect()
{
   Connection->reconnect();
}


#if 0
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
      throw ResultsDatabaseDataErrorException(what);
   }
   // Other error
   else {
      throw ResultsDatabaseException(what);
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


// ###### Execute statement #################################################
void MariaDBClient::executeQuery(Statement& statement)
{
   assert(statement.isValid());

   if(ResultSet != nullptr) {
      delete ResultSet;
      ResultSet = nullptr;
   }
   try {
      ResultSet = Transaction->executeQuery(statement.str());
   }
   catch(const sql::SQLException& exception) {
      handleDatabaseException(exception, "Execute", statement.str());
   }

   statement.clear();
}


// ###### Fetch next tuple ##################################################
bool MariaDBClient::fetchNextTuple()
{
   if(ResultSet != nullptr) {
      return ResultSet->next();
   }
   return false;
}


// ###### Get integer value #################################################
int32_t MariaDBClient::getInteger(unsigned int column) const
{
   assert(ResultSet != nullptr);
   return ResultSet->getInt(column);
}


// ###### Get big integer value #############################################
int64_t MariaDBClient::getBigInt(unsigned int column) const
{
   assert(ResultSet != nullptr);
   return ResultSet->getInt64(column);
}


// ###### Get string value ##################################################
std::string MariaDBClient::getString(unsigned int column) const
{
   assert(ResultSet != nullptr);
   return ResultSet->getString(column);
}
#endif
