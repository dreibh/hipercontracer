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

#ifndef DATABASECLIENT_MARIADB_H
#define DATABASECLIENT_MARIADB_H

#include "databaseclient-base.h"

// Ubuntu: libmysqlcppconn-dev
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>


class MariaDBClient : public DatabaseClientBase
{
   public:
   MariaDBClient(const DatabaseConfiguration& databaseConfiguration);
   virtual ~MariaDBClient();

   virtual const DatabaseBackendType getBackend() const;
   virtual bool open();
   virtual void close();
   virtual void reconnect();

   virtual void startTransaction();
   virtual void executeUpdate(const std::string& statement);
   virtual void endTransaction(const bool commit);

   inline sql::Driver*     getDriver()     { return Driver;     }
   inline sql::Connection* getConnection() { return Connection; }

   private:
   void handleSQLException(const sql::SQLException& exception,
                           const std::string&       where,
                           const std::string&       statement = std::string());

   sql::Driver*     Driver;
   sql::Connection* Connection;
   sql::Statement*  Statement;
};

#endif
