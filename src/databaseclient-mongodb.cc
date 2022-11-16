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

#include <vector>


// MongoDB C driver example:
// http://mongoc.org/libmongoc/current/tutorial.html#starting-mongodb


REGISTER_BACKEND(DatabaseBackendType::NoSQL_MongoDB, "MongoDB", MongoDBClient)


// ###### Constructor #######################################################
MongoDBClient::MongoDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   Connection = nullptr;
   mongoc_init();
}


// ###### Destructor ########################################################
MongoDBClient::~MongoDBClient()
{
   close();

   if(Connection) {
      mongoc_client_destroy(Connection);
      Connection = nullptr;
   }
   mongoc_cleanup();
}


// ###### Get backend #######################################################
const DatabaseBackendType MongoDBClient::getBackend() const
{
   return DatabaseBackendType::NoSQL_MongoDB;
}


// ###### Prepare connection to database ####################################
bool MongoDBClient::open()
{
   const std::string url = "mongodb://" +
      Configuration.getUser() + ":" + Configuration.getPassword() + "@" +
      Configuration.getServer() + ":" + std::to_string(Configuration.getPort()) +
      "/" + Configuration.getDatabase() + "?authMechanism=SCRAM-SHA-256";

   // ====== Connect to database ============================================
   Connection = mongoc_client_new(url.c_str());
   if(Connection == nullptr) {
      HPCT_LOG(error) << "Unable to create MongoDB Client";
      return false;
   }
   mongoc_client_set_appname(Connection, "UniversalImporter");

   // ====== Check connection ===============================================
   bson_error_t error;
   bson_t* command = BCON_NEW("ping", BCON_INT32(1));
   bson_t  reply;
   bool    retval = mongoc_client_command_simple(Connection, "admin", command, nullptr, &reply, &error);
   if(!retval) {
      HPCT_LOG(error) << "Connection to MongoDB " << url << " failed: " << error.message;
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

   // ====== Prepare BSON ===================================================
   bson_error_t error;
   bson_t       bson;
   if(!bson_init_from_json(&bson, "[ { \"test\": 1 }, { \"test\": 2 } ]", -1, &error)) {
      throw ImporterDatabaseDataErrorException(std::string("Data error: ") + error.message);
   }


   char* json;
   if((json = bson_as_canonical_extended_json(&bson, nullptr))) {
      printf ("%s\n", json);
      bson_free(json);
   }


   // ====== Split BSON into rows ===========================================
   std::vector<_bson_t> objectVector;
   objectVector.reserve(65536);   // FIXME! statement->rows()

   bson_iter_t iterator;
   if(bson_iter_init(&iterator, &bson)) {
      while(bson_iter_next(&iterator)) {
         puts("--");
         printf("T=%d\n", bson_iter_type(&iterator));   // 3 = BSON_TYPE_DOCUMENT


         if(bson_iter_type(&iterator) == BSON_TYPE_DOCUMENT) {
            // http://mongoc.org/libbson/current/bson_iter_t.html
            // https://github.com/mongodb/libbson/blob/master/src/bson/bson-iter.c
            bson_t         sub;
            uint32_t       len;
            const uint8_t* data;
            bson_iter_document(&iterator, &len, &data);
            if(bson_init_static (&sub, data, len)) {
               objectVector.push_back(sub);
            }
         }
         else {
            throw ImporterDatabaseDataErrorException("Data error: Unexpected format");
         }
      }
   }
   if(objectVector.size() == 0) {
      // No array of documents, just a single document
      objectVector.push_back(bson);
   }

   for(bson_t sub : objectVector) {
      if((json = bson_as_canonical_extended_json(&sub, nullptr))) {
         printf ("%s\n", json);
         bson_free(json);
      }
   }


   // ====== Insert rows ====================================================
   mongoc_collection_t* collection = mongoc_client_get_collection(Connection, Configuration.getDatabase().c_str(), "xxxx");
   assert(collection != nullptr);

   bool success = mongoc_collection_insert_one(collection, &bson, nullptr, nullptr, &error);

   mongoc_collection_destroy(collection);
   bson_destroy(&bson);
   if(!success) {
      throw ImporterDatabaseException(std::string("Insert error: ") +  error.message);
   }

   abort();
}
