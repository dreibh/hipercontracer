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

#include "databaseclient-debug.h"

#include <iostream>


// NOTE: The registration was moved to database-configuration.cc, due to linking issues!
// REGISTER_BACKEND(DatabaseBackendType::SQL_Debug, "DebugSQL", DebugClient)
// REGISTER_BACKEND_ALIAS(DatabaseBackendType::NoSQL_Debug, "DebugNoSQL", DebugClient, 2)


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
      throw ResultsDatabaseException("DEBUG CLIENT ONLY");
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
