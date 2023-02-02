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


#if !MONGOC_CHECK_VERSION(1, 16, 1)
#error The mongo-c-driver library is out-of-date. At least version 1.16.1 is required!
#endif

// NOTE:
// This backend uses the C library of MongoDB, instead of the C++ library.
// The C++ library in Debian, Ubuntu, etc. is *highly* out of date, not working
// with the latest MongoDB server, and with no progress in getting updated.
// Depending on including upstream C++ library sources would introduce a
// security maintenance problem!
// => Just using the C library, which is working and maintained.
//
// MongoDB C driver example code:
// http://mongoc.org/libmongoc/current/tutorial.html#starting-mongodb


REGISTER_BACKEND(DatabaseBackendType::NoSQL_MongoDB, "MongoDB", MongoDBClient)


// ###### Constructor #######################################################
MongoDBClient::MongoDBClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
   URI        = nullptr;
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
   if(URI) {
      mongoc_uri_destroy(URI);
      URI = nullptr;
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
   assert(Connection == nullptr);

   // ====== Create URI =====================================================
   const std::string url = "mongodb://" +
      Configuration.getServer() + ":" +
      std::to_string((Configuration.getPort() != 0) ? Configuration.getPort() : 27017) +
      "/" + Configuration.getDatabase();

   if(URI) {
      mongoc_uri_destroy(URI);
   }
   URI = mongoc_uri_new(url.c_str());
   assert(URI != nullptr);

   // Set options (http://mongoc.org/libmongoc/1.12.0/mongoc_uri_t.html):
   mongoc_uri_set_username(URI, Configuration.getUser().c_str());
   mongoc_uri_set_password(URI, Configuration.getPassword().c_str());
   mongoc_uri_set_auth_mechanism(URI, "SCRAM-SHA-256");

   mongoc_uri_set_option_as_utf8(URI, MONGOC_URI_APPNAME,       "UniversalImporter");
   mongoc_uri_set_option_as_utf8(URI, MONGOC_URI_COMPRESSORS,   "snappy,zlib,zstd");
   if(!(Configuration.getConnectionFlags() & ConnectionFlags::DisableTLS)) {
      mongoc_uri_set_option_as_bool(URI, MONGOC_URI_TLS, true);
   }
   else {
      HPCT_LOG(warning) << "TLS explicitly disabled. CONFIGURE TLS PROPERLY!!";
   }
   if(Configuration.getCAFile().size() > 0) {
      mongoc_uri_set_option_as_utf8(URI, MONGOC_URI_TLSCAFILE,
                                    Configuration.getCAFile().c_str());
   }
   if(Configuration.getCertKeyFile().size() > 0) {
     mongoc_uri_set_option_as_utf8(URI, MONGOC_URI_TLSCERTIFICATEKEYFILE,
                                   Configuration.getCertKeyFile().c_str());
   }
   if( (Configuration.getCertFile().size() > 0) ||
       (Configuration.getKeyFile().size() > 0) ) {
      HPCT_LOG(error) << "MongoDB backend expects one certificate+key file, not separate certificate and key files!";
      return false;
   }
   if(Configuration.getConnectionFlags() & ConnectionFlags::AllowInvalidCertificate) {
      mongoc_uri_set_option_as_bool(URI, MONGOC_URI_TLS, MONGOC_URI_TLSALLOWINVALIDCERTIFICATES);
      HPCT_LOG(warning) << "TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!";
   }
   if(Configuration.getConnectionFlags() & ConnectionFlags::AllowInvalidHostname) {
      mongoc_uri_set_option_as_bool(URI, MONGOC_URI_TLS, MONGOC_URI_TLSALLOWINVALIDHOSTNAMES);
      HPCT_LOG(warning) << "TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!";
   }

   // ====== Connect to database ============================================
   Connection = mongoc_client_new_from_uri(URI);
   if(Connection == nullptr) {
      HPCT_LOG(error) << "Unable to create MongoDB Client";
      return false;
   }

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
   // printf("JSON: %s\n", statement.str().c_str());

   // ====== Prepare BSON ===================================================
   bson_error_t error;
   bson_t       bson;
   if(!bson_init_from_json(&bson, statement.str().c_str(), -1, &error)) {
      const std::string errorMessage = std::string("Data error ") +
                                          std::to_string(error.domain) + "." +
                                          std::to_string(error.code) +
                                          ": " + error.message;
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
      const bool success = bson_init_static(&rowsToInsert, data, len);
      assert(success);
      assert(!bson_iter_next(&iterator));   // Only one collection is supported!
   }
   else {
      throw ImporterDatabaseDataErrorException("Data error: Unexpected format (not collection -> [ ... ])");
   }


   // ====== Split BSON into rows ===========================================
   bson_t  documentArray[statement.getRows()];
   bson_t* documentPtrArray[statement.getRows()];
   if(bson_iter_init(&iterator, &rowsToInsert)) {
      unsigned int i = 0;
      while(bson_iter_next(&iterator)) {
         if(bson_iter_type(&iterator) == BSON_TYPE_DOCUMENT) {
            // http://mongoc.org/libbson/current/bson_iter_t.html
            // https://github.com/mongodb/libbson/blob/master/src/bson/bson-iter.c
            uint32_t       len;
            const uint8_t* data;
            bson_iter_document(&iterator, &len, &data);
            assert(i < statement.getRows());
            const bool success = bson_init_static(&documentArray[i], data, len);
            assert(success);
            documentPtrArray[i] = &documentArray[i];
            i++;
         }
         else {
            throw ImporterDatabaseDataErrorException("Data error: Unexpected format (not list of documents)");
         }
      }
   }


   // ====== Print rows =====================================================
/*
   char* json;
   for(unsigned int i = 0;i < statement.getRows(); i++) {
      if((json = bson_as_canonical_extended_json(documentPtrArray[i], nullptr))) {
         printf ("%s\n", json);
         bson_free(json);
      }
   }
*/

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
