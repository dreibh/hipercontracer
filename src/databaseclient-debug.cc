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

#include "databaseclient-debug.h"
#include "importer-exception.h"

#include <iostream>


REGISTER_BACKEND(DatabaseBackendType::SQL_Debug, "DebugSQL", DebugClient)
REGISTER_BACKEND_ALIAS(DatabaseBackendType::NoSQL_Debug, "DebugNoSQL", DebugClient, 2)


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


// ###### Close connection to database ######################################
void DebugClient::close()
{
}


// ###### Reconnect connection to database ##################################
void DebugClient::reconnect()
{
   std::cout << "reconnect ..." << std::endl;
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
void DebugClient::executeUpdate(Statement& statement)
{
   assert(statement.isValid());
   std::cout << statement << std::endl;
   statement.clear();
}


// ###### Execute statement #################################################
void DebugClient::executeQuery(Statement& statement)
{
   assert(statement.isValid());
   std::cout << statement << std::endl;
   statement.clear();
}


// ###### Fetch next tuple ##################################################
bool DebugClient::fetchNextTuple()
{
   abort();
}


// ###### Get integer value #################################################
int32_t DebugClient::getInteger(unsigned int column) const
{
   abort();
}


// ###### Get big integer value #############################################
int64_t DebugClient::getBigInt(unsigned int column) const
{
   abort();
}


// ###### Get string value ##################################################
std::string DebugClient::getString(unsigned int column) const
{
   abort();
}
