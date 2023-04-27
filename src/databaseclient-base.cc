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

#include "databaseclient-base.h"

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
