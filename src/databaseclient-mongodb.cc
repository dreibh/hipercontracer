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

#include <boost/algorithm/string.hpp>


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
   bson_destroy(&reply);
   bson_destroy(command);

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
// Expected input format of the statement:
// JSON object 'collection_name': [ item1, item2, ..., itemN ]
void MongoDBClient::executeUpdate(Statement& statement)
{
   assert(statement.isValid());
   printf("S: %s\n", statement.str().c_str());

   // ====== Prepare BSON ===================================================
   bson_error_t error;
   bson_t       bson;
   if(!bson_init_from_json(&bson, statement.str().c_str(), -1, &error)) {
      const std::string errorMessage = std::string("Data error ") +
                                          std::to_string(error.domain) + "." +
                                          std::to_string(error.code) +
                                          ": " + error.message;
      printf("ERROR: <%s>\n", errorMessage.c_str());
    abort();
      throw ImporterDatabaseDataErrorException(errorMessage);
   }
/*
   char* json;
   if((json = bson_as_canonical_extended_json(&bson, nullptr))) {
      printf ("%s\n", json);
      bson_free(json);
   }
*/

   // ====== Find collection ================================================
   bson_iter_t iterator;
   bson_t      rowsToInsert;
   const char* key;
   std::string collectionName;
   if( (bson_iter_init(&iterator, &bson)) &&
       (bson_iter_next(&iterator)) &&
       ( (key = bson_iter_key(&iterator)) != nullptr ) &&
       (bson_iter_type(&iterator) == BSON_TYPE_ARRAY) ) {
      collectionName = boost::to_lower_copy<std::string>(key);
      uint32_t       len;
      const uint8_t* data;
      bson_iter_array(&iterator, &len, &data);
      assert(bson_init_static(&rowsToInsert, data, len));
      assert(!bson_iter_next(&iterator));   // Only one collection is supported!
   }
   else {
      throw ImporterDatabaseDataErrorException("Data error: Unexpected format (not collection -> [ ... ])");
   }


   // ====== Split BSON into rows ===========================================
   std::vector<_bson_t> documentVector(statement.getRows());
   if(bson_iter_init(&iterator, &rowsToInsert)) {
      unsigned int i = 0;
      while(bson_iter_next(&iterator)) {
         if(bson_iter_type(&iterator) == BSON_TYPE_DOCUMENT) {
            // http://mongoc.org/libbson/current/bson_iter_t.html
            // https://github.com/mongodb/libbson/blob/master/src/bson/bson-iter.c
            _bson_t        sub;
            uint32_t       len;
            const uint8_t* data;
            bson_iter_document(&iterator, &len, &data);
            assert(i < statement.getRows());
            assert(bson_init_static(&sub, data, len));
            documentVector[i++] = sub;
         }
         else {
            throw ImporterDatabaseDataErrorException("Data error: Unexpected format (not list of documents)");
         }
      }
   }
   assert(documentVector.size() == statement.getRows());


   // ====== Get pointer to each row ========================================
   bson_t* documentPtrArray[statement.getRows()];
   unsigned int i = 0;
   for(bson_t sub : documentVector) {
      documentPtrArray[i++] = &sub;
   }
   assert(i == statement.getRows());


   // ====== Insert rows ====================================================
   mongoc_collection_t* collection =
      mongoc_client_get_collection(Connection,
                                   Configuration.getDatabase().c_str(),
                                   collectionName.c_str());
   assert(collection != nullptr);
   bool success = mongoc_collection_insert_many(collection,
                                                (const bson_t**)&documentPtrArray,
                                                statement.getRows(),
                                                nullptr, nullptr, &error);
   mongoc_collection_destroy(collection);
   bson_destroy(&bson);
   if(!success) {
      const std::string errorMessage = std::string("Insert error ") +
                                          std::to_string(error.domain) + "." +
                                          std::to_string(error.code) +
                                          ": " + error.message;
      if(error.domain == 12) {
         throw ImporterDatabaseDataErrorException(errorMessage);
      }
      else {
         throw ImporterDatabaseException(errorMessage);
      }
   }

   statement.clear();
}
