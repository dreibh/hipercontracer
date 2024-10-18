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

#include "databaseclient-base.h"
#include "logger.h"

#include <boost/asio/ip/address.hpp>
#include <boost/beast/core/detail/base64.hpp>


// ###### Constructor #######################################################
DatabaseClientBase::DatabaseClientBase(const DatabaseConfiguration& configuration)
   : Configuration(configuration)
{
}


// ###### Destructor ########################################################
DatabaseClientBase::~DatabaseClientBase()
{
   std::map<std::string, Statement*>::iterator iterator = StatementMap.begin();
   while(iterator != StatementMap.end()) {
      delete iterator->second;
      StatementMap.erase(iterator);
      iterator = StatementMap.begin();
   }
}


// ###### Reconnect connection to database ##################################
void DatabaseClientBase::reconnect()
{
   HPCT_LOG(debug) << "Reconnect ...";
   close();
   open();
}


// ###### Get or create new statement #######################################
Statement& DatabaseClientBase::getStatement(const std::string& name,
                                            const bool         mustExist,
                                            const bool         clearStatement)
{
   Statement* statement;
   std::map<std::string, Statement*>::iterator found = StatementMap.find(name);
   if(found == StatementMap.end()) {
      assert(mustExist == false);
      statement = new Statement(getBackend());
      assert(statement != nullptr);
      StatementMap.insert(std::pair<std::string, Statement*>(name, statement));
   }
   else {
      statement = found->second;
      if(clearStatement) {
         statement->clear();
      }
   }
   return *statement;
}


// ###### Check whether column exists #######################################
bool DatabaseClientBase::hasColumn(const char* column) const
{
   abort();   // To be implemented by subclass!
}


// ###### Get integer value #################################################
int32_t DatabaseClientBase::getInteger(unsigned int column) const
{
   abort();   // To be implemented by subclass!
}


// ###### Get integer value #################################################
int32_t DatabaseClientBase::getInteger(const char* column) const
{
   abort();   // To be implemented by subclass!
}


// ###### Get big integer value #############################################
int64_t DatabaseClientBase::getBigInt(unsigned int column) const
{
   abort();   // To be implemented by subclass!
}


// ###### Get big integer value #############################################
int64_t DatabaseClientBase::getBigInt(const char* column) const
{
   abort();   // To be implemented by subclass!
}


// ###### Get string value ##################################################
std::string DatabaseClientBase::getString(unsigned int column) const
{
   abort();   // To be implemented by subclass!
}


// ###### Get string value ##################################################
std::string DatabaseClientBase::getString(const char* column) const
{
   abort();   // To be implemented by subclass!
}


// ###### Get array #########################################################
void DatabaseClientBase::getArrayBegin(const char* column)
{
   abort();   // To be implemented by subclass!
}


// ###### Get array #########################################################
void DatabaseClientBase::getArrayEnd()
{
   abort();   // To be implemented by subclass!
}


// ###### Fetch next array tuple ############################################
bool DatabaseClientBase::fetchNextArrayTuple()
{
   abort();   // To be implemented by subclass!
}
