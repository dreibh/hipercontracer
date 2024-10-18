// ==========================================================================
//     _   _ _ ____            ____          _____
//    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
//    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
//    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
//    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
//
//       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
//                 https://www.nntb.no/~dreibh/hipercontracer/
// ==========================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2025 by Thomas Dreibholz
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

#include "results-exception.h"


class DatabaseClientBase
{
   public:
   DatabaseClientBase(const DatabaseConfiguration& configuration);
   virtual ~DatabaseClientBase();

   virtual const DatabaseBackendType getBackend() const = 0;
   virtual bool open()  = 0;
   virtual void close() = 0;
   virtual void reconnect();

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
   virtual bool hasColumn(const char* column) const;
   virtual int32_t getInteger(unsigned int column) const;
   virtual int32_t getInteger(const char* column) const;
   virtual int64_t getBigInt(unsigned int column) const;
   virtual int64_t getBigInt(const char* column) const;
   virtual std::string getString(unsigned int column) const;
   virtual std::string getString(const char* column) const;

   virtual void getArrayBegin(const char* column);
   virtual void getArrayEnd();
   virtual bool fetchNextArrayTuple();

   Statement& getStatement(const std::string& name,
                           const bool         mustExist      = true,
                           const bool         clearStatement = false);

   protected:
   const DatabaseConfiguration&      Configuration;
   std::map<std::string, Statement*> StatementMap;
};

#endif
