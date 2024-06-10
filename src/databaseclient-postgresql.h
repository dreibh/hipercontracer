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

#ifndef DATABASECLIENT_POSTGRESQL_H
#define DATABASECLIENT_POSTGRESQL_H

#include "databaseclient-base.h"

// Ubuntu: libpqxx-dev
#include <pqxx/pqxx>


class PostgreSQLClient : public DatabaseClientBase
{
   public:
   PostgreSQLClient(const DatabaseConfiguration& databaseConfiguration);
   virtual ~PostgreSQLClient();

   virtual const DatabaseBackendType getBackend() const;
   virtual bool open();
   virtual void close();
   virtual void reconnect();

   virtual void startTransaction();
   virtual void executeUpdate(Statement& statement);
   virtual void executeQuery(Statement& statement);
   virtual void endTransaction(const bool commit);

   virtual bool fetchNextTuple();
   virtual int32_t getInteger(unsigned int column) const;
   virtual int64_t getBigInt(unsigned int column) const;
   virtual std::string getString(unsigned int column) const;

   inline pqxx::connection* getConnection() { return Connection; }

   private:
   void handleDatabaseException(const pqxx::failure& exception,
                                const std::string&   where,
                                const std::string&   statement = std::string());

   pqxx::connection*       Connection;
   pqxx::work*             Transaction;
   pqxx::result            ResultSet;
   pqxx::result::size_type ResultIndex;
};

#endif
