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


REGISTER_BACKEND(DatabaseBackendType::NoSQL_MongoDB, "MongoDB", MongoDBClient)


// ###### Constructor #######################################################
MongoDBClient::MongoDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   mongo::client::initialize();
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
   const std::string url = Configuration.getServer() + ":" + std::to_string(Configuration.getPort());
   std::string       errorMessage;

   // ====== Connect to database ============================================
   try {
      Connection.connect(url);
      Connection.auth(Configuration.getDatabase(),
                      Configuration.getUser(),
                      Configuration.getPassword(),
                      errorMessage);
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
}


// ###### Reconnect connection to database ##################################
void MongoDBClient::reconnect()
{
}


// ###### Begin transaction #################################################
void MongoDBClient::startTransaction()
{
}


// ###### End transaction ###################################################
void MongoDBClient::endTransaction(const bool commit)
{
}


// ###### Execute statement #################################################
void MongoDBClient::executeUpdate(const std::string& statement)
{
   printf("=> %s\n", statement.c_str());
   puts("");
   fflush(stdout);


   mongo::BSONObj bson;
   try {
//       mongo::fromjson(statement);
      mongo::fromjson("{ \"test\": 1 }");
   }
   catch(const mongo::DBException& exception) {
      throw ImporterDatabaseDataErrorException(std::string("Data error: ") + exception.what());
   }

   try {
       Connection.insert(Configuration.getDatabase() + ".xxxx",
                         bson);
   }
   catch(const mongo::DBException& exception) {
      throw ImporterDatabaseException(std::string("Execute error: ") + exception.what());
   }
}
