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

#include "databaseclient-debug.h"
#include "importer-exception.h"


// ###### Constructor #######################################################
DebugClient::DebugClient(const DatabaseConfiguration& configuration)
   : DatabaseClientBase(configuration)
{
}


// ###### Destructor ########################################################
DebugClient::~DebugClient()
{
}


// ###### Get backend #######################################################
const DatabaseBackendType DebugClient::getBackend() const
{
   return Configuration.getBackend();
}


// ###### Prepare connection to database ####################################
bool DebugClient::open()
{
   return true;
}


// ###### Finish connection to database #####################################
void DebugClient::close()
{
}


// ###### Begin transaction #################################################
void DebugClient::startTransaction()
{
   std::cout << "START TRANSACTION" << std::endl;
}


// ###### End transaction ###################################################
void DebugClient::endTransaction(const bool commit)
{
   if(commit) {
      std::cout << "COMMIT" << std::endl;
      throw ImporterDatabaseException("DEBUG CLIENT ONLY");
   }
   else {
      std::cout << "ROLLBACK" << std::endl;
   }
}


// ###### Execute statement #################################################
void DebugClient::execute(const std::string& statement)
{
   std::cout << statement;
}
