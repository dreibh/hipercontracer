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

#include "databaseclient-mongodb.h"
#include "importer-exception.h"
#include "logger.h"

// MongoDB C++ driver example:
// https://mongodb-documentation.readthedocs.io/en/latest/ecosystem/tutorial/getting-started-with-cpp-driver.html


// ###### Constructor #######################################################
MongoDBClient::MongoDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
}


// ###### Destructor ########################################################
MongoDBClient::~MongoDBClient()
{
   close();
}


// ###### Get backend #######################################################
const DatabaseBackendType MongoDBClient::getBackend() const
{
   return DatabaseBackendType::NoSQL_MongoDB;
}


// ###### Prepare connection to database ####################################
bool MongoDBClient::open()
{
   const std::string url = "tcp://" + Configuration.getServer() + ":" + std::to_string(Configuration.getPort());

   assert(Connection == nullptr);
   try {
      // ====== Connect to database =========================================
//       Connection = Driver->connect(url.c_str(),
//                                    Configuration.getUser().c_str(),
//                                    Configuration.getPassword().c_str());
//       assert(Connection != nullptr);
//       Connection->setSchema(Configuration.getDatabase().c_str());
//       // SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED
//       Connection->setTransactionIsolation(sql::TRANSACTION_READ_COMMITTED);
//       Connection->setAutoCommit(false);
//
//       // ====== Create statement ============================================
//       Statement = Connection->createStatement();
//       assert(Statement != nullptr);
   }
   catch(const mongo::DBException &e) {
      HPCT_LOG(error) << "Unable to connect MongoDB client to " << url << ": " << e.what();
      close();
      return false;
   }

   return true;
}


// ###### Close connection to database ######################################
void MongoDBClient::close()
{
//    if(Statement) {
//       delete Statement;
//       Statement = nullptr;
//    }
//    if(Connection != nullptr) {
//       delete Connection;
//       Connection = nullptr;
//    }
}


// ###### Reconnect connection to database ##################################
void MongoDBClient::reconnect()
{
   Connection->reconnect();
}


// ###### Handle SQLException ###############################################
void MongoDBClient::handleDatabaseException(const mongo::DBException& exception,
                                            const std::string&        where,
                                            const std::string&        statement)
{
   // ====== Log error ======================================================
   const std::string what = where + " error E" +
                               std::to_string(exception.getCode()) + ": " +
                               exception.what();
   HPCT_LOG(error) << what;
   HPCT_LOG(debug) << statement;

   // ====== Throw exception ================================================
//    const std::string e = exception.getSQLState().substr(0, 2);
//    // Integrity Error, according to mysql/connector/errors.py
//    if( (e == "23") || (e == "22") || (e == "XA")) {
//       // For this type, the input file should be moved to the bad directory.
//       throw ImporterDatabaseDataErrorException(what);
//    }
//    // Other error
//    else {
      throw ImporterDatabaseException(what);
//    }
}


// ###### Begin transaction #################################################
void MongoDBClient::startTransaction()
{
   try {
      Statement->execute("START TRANSACTION");
   }
   catch(const sql::SQLException& exception) {
      handleDatabaseException(exception, "Start of transaction");
   }
}


// ###### End transaction ###################################################
void MongoDBClient::endTransaction(const bool commit)
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
void MongoDBClient::executeUpdate(const std::string& statement)
{
   try {
      Statement->executeUpdate(statement);
   }
   catch(const sql::SQLException& exception) {
      handleDatabaseException(exception, "Execute", statement);
   }
}