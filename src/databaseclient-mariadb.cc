#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cassert>

#include <mysql.h>

int main(int argc, char **argv)
{
  MYSQL mysql;
  mysql_init(&mysql);

  my_bool onoff;
  onoff = true;
  mysql_options(&mysql, MYSQL_OPT_SSL_ENFORCE, &onoff);
  onoff = false;
  mysql_options(&mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &onoff);
  mysql_options(&mysql, MYSQL_OPT_TLS_VERSION, "TLSv1.3");
  mysql_options(&mysql, MYSQL_OPT_COMPRESS, nullptr);

   mysql_optionsv(&mysql, MYSQL_OPT_SSL_CA, "/home/dreibh/TestLevel1.crt");
   mysql_optionsv(&mysql, MYSQL_OPT_SSL_CRL, "/home/dreibh/TestGlobal.crl");

   mysql_autocommit(&mysql, 0);
   mysql_options(&mysql, MYSQL_INIT_COMMAND,"SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED");

   puts("connecting...");
  if (!mysql_real_connect(&mysql,
                          "10.193.4.69",
                          "importer",         /* mysql user */
                          "2TlTz3z0voJFK9CmOyBG2YjC6lm1I96SACdgB3dbAjt2nTxlGpOXgoPRmN0lMttGsAnKFKdLC7yg3aK7CYtrF3j49yTfvSEM7lVyPB2h4GIlnGPjAE3q8ulGeJKQ4L9T",          /* password */
                          "testonly",               /* default database to use, NULL for none */
                          3306,           /* port number, 0 for default */
                          nullptr,        /* socket file or named pipe name */
                          CLIENT_FOUND_ROWS /* connection flags */ )) {
    puts("Connect failed\n");
  exit(1);
  }
  puts("OK!");


  printf("MySQL Server Info: %s\n", mysql_get_server_info(&mysql));


   MYSQL_STMT* stmt = mysql_stmt_init(&mysql);
   assert(stmt != nullptr);


   mysql_stmt_close(stmt);



   mysql_close(&mysql);

   return 0;
}


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
   mysql_init(&Connection);
   Transaction = nullptr;
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

   mysql_options(&Connection, MYSQL_OPT_SSL_ENFORCE, &sslEnforce);
   mysql_options(&Connection, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &sslVerify);
   mysql_options(&Connection, MYSQL_OPT_TLS_VERSION, "TLSv1.3");
   mysql_options(&Connection, MYSQL_OPT_COMPRESS, nullptr);
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

   mysql_autocommit(&Connection, 0);
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

   return true;
}


// ###### Close connection to database ######################################
void MariaDBClient::close()
{
//    if(ResultSet != nullptr) {
//       delete ResultSet;
//       ResultSet = nullptr;
//    }
   if(Transaction) {
      mysql_stmt_close(Transaction);
      Transaction = nullptr;
   }
   mysql_close(&Connection);
}


// ###### Reconnect connection to database ##################################
void MariaDBClient::reconnect()
{
   close();
   open();
}


// ###### Handle non-statement error ########################################
void MariaDBClient::handleDatabaseError(const std::string& where)
{
   const unsigned int errorCode = mysql_errno(&Connection);
   const std::string  sqlState  = mysql_sqlstate(&Connection);

   // ====== Log error ======================================================
   const std::string what = where + " error " +
                               sqlState + "/E" +
                               std::to_string(errorCode) + ": " +
                               mysql_error(&Connection);
   HPCT_LOG(error) << what;

   // ====== Throw exception ================================================
   const std::string e = sqlState.substr(0, 2);
   if( (e == "42") || (e == "23") || (e == "22") || (e == "XA")) {
      //  Based on mysql/connector/errors.py:
      // For this type, the input file should be moved to the bad directory.
      throw ResultsDatabaseDataErrorException(what);
   }
   else {
      // Other error
      throw ResultsDatabaseException(what);
   }
}


// ###### Handle statement error ########################################
void MariaDBClient::handleDatabaseError(const std::string& where,
                                        const Statement&   statement)
{
   const unsigned int errorCode = mysql_stmt_errno(Transaction);
   const std::string  sqlState  = mysql_stmt_sqlstate(Transaction);

   // ====== Log error ======================================================
   const std::string what = where + " statement error " +
                               sqlState + "/E" +
                               std::to_string(errorCode) + ": " +
                               mysql_stmt_error(Transaction);
   HPCT_LOG(error) << what;

   // ====== Throw exception ================================================
   const std::string e = sqlState.substr(0, 2);
   if( (e == "42") || (e == "23") || (e == "22") || (e == "XA")) {
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
   if(!mysql_stmt_execute(Transaction)) {
      handleDatabaseError("Start of transaction");
   }
}


// ###### End transaction ###################################################
void MariaDBClient::endTransaction(const bool commit)
{
   // ====== Commit transaction =============================================
   if(commit) {
      if(!mysql_commit(&Connection)) {
         handleDatabaseError("Commit");
      }
   }

   // ====== Commit transaction =============================================
   else {
      if(!mysql_rollback(&Connection)) {
         handleDatabaseError("Rollback");
      }
   }
}


// ###### Execute statement #################################################
void MariaDBClient::executeUpdate(Statement& statement)
{
   assert(statement.isValid());

   if(!mysql_stmt_prepare(Transaction, statement.str().c_str(), statement.str().size())) {
      handleDatabaseError("Prepare", statement);
   }
   if(!mysql_stmt_execute(Transaction)) {
      handleDatabaseError("Execute", statement);
   }

   statement.clear();
}


#if 0

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
