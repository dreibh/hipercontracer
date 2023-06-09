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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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

#ifndef DATABASECLIENT_BASE_H
#define DATABASECLIENT_BASE_H

#include "database-configuration.h"
#include "database-statement.h"


class DatabaseClientBase
{
   public:
   DatabaseClientBase(const DatabaseConfiguration& configuration);
   virtual ~DatabaseClientBase();

   virtual const DatabaseBackendType getBackend() const = 0;
   virtual bool open()  = 0;
   virtual void close() = 0;
   virtual void reconnect() = 0;

   virtual void startTransaction() = 0;
   virtual void executeUpdate(Statement& statement) = 0;
   virtual void executeQuery(Statement& statement) = 0;
   virtual void endTransaction(const bool commit) = 0;

   inline void commit()   { endTransaction(true);  }
   inline void rollback() { endTransaction(false); }

   inline void executeUpdate(const char* statement) {
      executeUpdate(std::string(statement));
   }
   inline void executeUpdate(const std::string& statement) {
      Statement s(Configuration.getBackend());
      s << statement;
      executeUpdate(s);
   }

   inline void executeQuery(const char* statement) {
      executeQuery(std::string(statement));
   }
   inline void executeQuery(const std::string& statement) {
      Statement s(Configuration.getBackend());
      s << statement;
      executeQuery(s);
   }

   virtual bool fetchNextTuple() = 0;
   virtual int32_t getInteger(unsigned int column) const = 0;
   virtual int64_t getBigInt(unsigned int column) const = 0;
   virtual std::string getString(unsigned int column) const = 0;

   Statement& getStatement(const std::string& name,
                           const bool         mustExist      = true,
                           const bool         clearStatement = false);

   protected:
   const DatabaseConfiguration&      Configuration;
   std::map<std::string, Statement*> StatementMap;
};

#endif
