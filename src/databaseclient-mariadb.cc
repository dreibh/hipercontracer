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

#include "databaseclient-mariadb.h"
#include "logger.h"


// NOTE: The registration was moved to database-configuration.cc, due to linking issues!
// REGISTER_BACKEND(DatabaseBackendType::SQL_MariaDB, "MariaDB", MariaDBClient)
// REGISTER_BACKEND_ALIAS(DatabaseBackendType::SQL_MariaDB, "MySQL", MariaDBClient, 2)


// ###### Constructor #######################################################
MariaDBClient::MariaDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   mysql_init(&Connection);
   ResultCursor  = nullptr;
   ResultColumns = 0;
}


// ###### Destructor ########################################################
MariaDBClient::~MariaDBClient()
{
   close();
   mysql_close(&Connection);
}


// ###### Get backend #######################################################
const DatabaseBackendType MariaDBClient::getBackend() const
{
   return DatabaseBackendType::SQL_MariaDB;
}


// ###### Prepare connection to database ####################################
bool MariaDBClient::open()
{
   // ====== Prepare parameters =============================================
   my_bool sslEnforce = true;
   my_bool sslVerify  = true;
   if(Configuration.getConnectionFlags() & DisableTLS) {
      sslEnforce = false;
      HPCT_LOG(warning) << "TLS explicitly disabled. CONFIGURE TLS PROPERLY!!";
   }
   else if(Configuration.getConnectionFlags() & (ConnectionFlags::AllowInvalidCertificate|ConnectionFlags::AllowInvalidHostname)) {
      sslVerify = false;
      HPCT_LOG(warning) << "TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!";
   }

   mysql_options(&Connection, MYSQL_OPT_SSL_ENFORCE,            &sslEnforce);
   mysql_options(&Connection, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &sslVerify);
   mysql_options(&Connection, MYSQL_OPT_TLS_VERSION,            "TLSv1.3");
   if(Configuration.getCAFile().size() > 0) {
      mysql_options(&Connection, MYSQL_OPT_SSL_CA, Configuration.getCAFile().c_str());
   }
   if( (sslVerify) && (Configuration.getCRLFile().size() > 0) ) {
      mysql_options(&Connection, MYSQL_OPT_SSL_CRL, Configuration.getCRLFile().c_str());
   }
   if(Configuration.getCertFile().size() > 0) {
      mysql_options(&Connection, MYSQL_OPT_SSL_CERT, Configuration.getCertFile().c_str());
   }
   if(Configuration.getCertKeyFile().size() > 0) {
      mysql_options(&Connection, MYSQL_OPT_SSL_KEY, Configuration.getCertKeyFile().c_str());
   }

   // ====== Connect to database =========================================
   mysql_autocommit(&Connection, 0);
   mysql_options(&Connection, MYSQL_OPT_COMPRESS, (void*)1);
   mysql_options(&Connection, MYSQL_INIT_COMMAND,"SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED");
   if(!mysql_real_connect(&Connection,
                          Configuration.getServer().c_str(),
                          Configuration.getUser().c_str(),
                          Configuration.getPassword().c_str(),
                          Configuration.getDatabase().c_str(),
                          (Configuration.getPort() != 0) ? Configuration.getPort() : 3306,
                          nullptr,
                          CLIENT_FOUND_ROWS)) {
      HPCT_LOG(error) << "Unable to connect MySQL/MariaDB client to "
                      << Configuration.getServer() << ": " << mysql_error(&Connection);
      close();
      return false;
   }
   HPCT_LOG(debug) << "MySQL/MariaDB Server Info: " << mysql_get_server_info(&Connection);

   return true;
}


// ###### Close connection to database ######################################
void MariaDBClient::close()
{
   if(ResultCursor) {
      mysql_free_result(ResultCursor);
      ResultCursor = nullptr;
   }
   mysql_close(&Connection);
   mysql_init(&Connection);
}


// ###### Handle database error #############################################
void MariaDBClient::handleDatabaseError(const std::string& where,
                                        const std::string& statement)
{
   const unsigned int errorCode = mysql_errno(&Connection);
   const std::string  sqlState  = mysql_sqlstate(&Connection);

   // ====== Log error ======================================================
   const std::string what = where + " error " +
                               sqlState + "/E" +
                               std::to_string(errorCode) + ": " +
                               mysql_error(&Connection);
   HPCT_LOG(error) << what;
   if(statement.size() > 0) {
      HPCT_LOG(debug) << statement;
   }

   // ====== Throw exception ================================================
   const std::string e = sqlState.substr(0, 2);
   if( (e == "42") || (e == "23") || (e == "22") || (e == "XA") || (e == "99") ) {
      //  Based on mysql/connector/errors.py:
      // For this type, the input file should be moved to the bad directory.
      throw ResultsDatabaseDataErrorException(what);
   }
   else {
      // Other error
      throw ResultsDatabaseException(what);
   }
}


// ###### Begin transaction #################################################
void MariaDBClient::startTransaction()
{
   static const std::string statement("START TRANSACTION");
   if(mysql_query(&Connection, statement.c_str())) {
      handleDatabaseError("Start Transaction", statement);
   }
}


// ###### End transaction ###################################################
void MariaDBClient::endTransaction(const bool commit)
{
   // ====== Commit transaction =============================================
   if(commit) {
      if(mysql_commit(&Connection)) {
         handleDatabaseError("Commit");
      }
   }

   // ====== Roll back transaction ==========================================
   else {
      if(mysql_rollback(&Connection)) {
         handleDatabaseError("Rollback");
      }
   }
}


// ###### Execute statement #################################################
void MariaDBClient::executeUpdate(Statement& statement)
{
   assert(statement.isValid());
   if(ResultCursor) {
      mysql_free_result(ResultCursor);
      ResultCursor = nullptr;
   }

   if(mysql_query(&Connection, statement.str().c_str())) {
      handleDatabaseError("Query", statement.str());
   }

   statement.clear();
}


// ###### Execute statement #################################################
void MariaDBClient::executeQuery(Statement& statement)
{
   assert(statement.isValid());
   if(ResultCursor) {
      mysql_free_result(ResultCursor);
      ResultCursor = nullptr;
   }

   if(mysql_query(&Connection, statement.str().c_str())) {
      handleDatabaseError("Query", statement.str());
   }
   ResultCursor = mysql_store_result(&Connection);
   if(ResultCursor == nullptr) {
      handleDatabaseError("Store", statement.str());
   }
   ResultColumns = mysql_num_fields(ResultCursor);

   statement.clear();
}


// ###### Fetch next tuple ##################################################
bool MariaDBClient::fetchNextTuple()
{
   assert(ResultCursor != nullptr);

   ResultRow = mysql_fetch_row(ResultCursor);
   if(ResultRow != nullptr) {
      return true;
   }

   if(mysql_errno(&Connection) != 0) {
      handleDatabaseError("Fetch");
   }
   mysql_free_result(ResultCursor);
   ResultCursor = nullptr;
   return false;
}


// ###### Get integer value #################################################
int32_t MariaDBClient::getInteger(unsigned int column) const
{
   assert(ResultCursor != nullptr);
   assert( (column >= 1) && (column <= ResultColumns) );
   return atoi(ResultRow[column - 1]);
}


// ###### Get big integer value #############################################
int64_t MariaDBClient::getBigInt(unsigned int column) const
{
   assert(ResultCursor != nullptr);
   assert( (column >= 1) && (column <= ResultColumns) );
   return atoll(ResultRow[column - 1]);
}


// ###### Get string value ##################################################
std::string MariaDBClient::getString(unsigned int column) const
{
   assert(ResultCursor != nullptr);
   assert( (column >= 1) && (column <= ResultColumns) );
   return std::string(ResultRow[column - 1]);
}
