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

#ifndef DATABASECLIENT_MONGODB_H
#define DATABASECLIENT_MONGODB_H

#include "databaseclient-base.h"

// Ubuntu: libmongoc-dev
#include <mongoc.h>


class MongoDBClient : public DatabaseClientBase
{
   public:
   MongoDBClient(const DatabaseConfiguration& databaseConfiguration);
   virtual ~MongoDBClient();

   virtual const DatabaseBackendType getBackend() const;
   virtual bool open();
   virtual void close();

   virtual void startTransaction();
   virtual void executeUpdate(Statement& statement);
   virtual void executeQuery(Statement& statement);
   virtual void endTransaction(const bool commit);

   virtual bool fetchNextTuple();
   virtual int32_t getInteger(const char* column) const;
   virtual int64_t getBigInt(const char* column) const;
   virtual std::string getString(const char* column) const;

   virtual void getArrayBegin(const char* column);
   virtual void getArrayEnd();
   virtual bool fetchNextArrayTuple();

   inline mongoc_client_t* getConnection() { return Connection; }

   private:
   void freeResults();

   mongoc_uri_t*        URI;
   mongoc_client_t*     Connection;
   mongoc_collection_t* ResultCollection;
   mongoc_cursor_t*     ResultCursor;
   const bson_t*        ResultDoc;

   bson_iter_t          ResultArrayIterator;
   bson_t               ResultArrayDoc;
   const bson_t*        ResultArrayParentDoc;
};

#endif
