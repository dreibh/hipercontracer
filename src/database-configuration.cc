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

#include "database-configuration.h"
#include "logger.h"
#include "tools.h"

#include <fstream>

#include <boost/algorithm/string.hpp>


//  ###### Database Backend Registry ########################################

#ifdef ENABLE_BACKEND_DEBUG
#include "databaseclient-debug.h"
REGISTER_BACKEND(DatabaseBackendType::SQL_Debug, "DebugSQL", DebugClient)
REGISTER_BACKEND_ALIAS(DatabaseBackendType::NoSQL_Debug, "DebugNoSQL", DebugClient, 2)
#endif

#ifdef ENABLE_BACKEND_MARIADB
#include "databaseclient-mariadb.h"
REGISTER_BACKEND(DatabaseBackendType::SQL_MariaDB, "MariaDB", MariaDBClient)
REGISTER_BACKEND_ALIAS(DatabaseBackendType::SQL_MariaDB, "MySQL", MariaDBClient, 2)
#endif

#ifdef ENABLE_BACKEND_POSTGRESQL
#include "databaseclient-postgresql.h"
REGISTER_BACKEND(DatabaseBackendType::SQL_PostgreSQL, "PostgreSQL", PostgreSQLClient)
#endif

#ifdef ENABLE_BACKEND_MONGODB
#include "databaseclient-mongodb.h"
REGISTER_BACKEND(DatabaseBackendType::NoSQL_MongoDB, "MongoDB", MongoDBClient)
#endif

//  #########################################################################


std::list<DatabaseConfiguration::RegisteredBackend*>* DatabaseConfiguration::BackendList = nullptr;


// ###### Constructor #######################################################
DatabaseConfiguration::DatabaseConfiguration()
   : OptionsDescription("Options")
{
   OptionsDescription.add_options()
      ("dbserver",          boost::program_options::value<std::string>(&Server),                             "database server")
      ("dbport",            boost::program_options::value<uint16_t>(&Port)->default_value(0),                "database port")
      ("dbuser",            boost::program_options::value<std::string>(&User),                               "database username")
      ("dbpassword",        boost::program_options::value<std::string>(&Password),                           "database password")
      ("dbcafile",          boost::program_options::value<std::string>(&CAFile),                             "database CA file")
      ("dbcrlfile",         boost::program_options::value<std::string>(&CRLFile),                            "database CRL file")
      ("dbcertfile",        boost::program_options::value<std::string>(&CertFile),                           "database certificate file")
      ("dbkeyfile",         boost::program_options::value<std::string>(&KeyFile),                            "database key file")
      ("dbcertkeyfile",     boost::program_options::value<std::string>(&CertKeyFile),                        "database certificate+key file")
      ("database",          boost::program_options::value<std::string>(&Database),                           "database name")
      ("dbbackend",         boost::program_options::value<std::string>(&BackendName),                        "database backend")
      ("dbreconnectdelay",  boost::program_options::value<unsigned int>(&ReconnectDelay)->default_value(60), "database reconnect delay (in s)")
      ("dbconnectionflags", boost::program_options::value<std::string>(&FlagNames),                          "database connection flags")
   ;
   BackendName = "Invalid";
   Backend     = DatabaseBackendType::Invalid;
   Flags       = ConnectionFlags::None;
}


// ###### Destructor ########################################################
DatabaseConfiguration::~DatabaseConfiguration()
{
}


// ###### Read database configuration #######################################
bool DatabaseConfiguration::readConfiguration(const std::filesystem::path& configurationFile)
{
   std::ifstream configurationInputStream(configurationFile);

   if(!configurationInputStream.good()) {
      HPCT_LOG(error) << "Unable to read database configuration from " << configurationFile;
      return false;
   }

   try {
      boost::program_options::variables_map vm = boost::program_options::variables_map();
      boost::program_options::store(boost::program_options::parse_config_file(configurationInputStream , OptionsDescription), vm);
      boost::program_options::notify(vm);
   } catch(const std::exception& e) {
      HPCT_LOG(error) << "Parsing configuration file " << configurationFile
                      << " failed: " << e.what();
      return false;
   }

   // ====== Check options ==================================================
   if(!setBackend(BackendName))           return false;
   if(!setConnectionFlags(FlagNames))     return false;

   // Legacy parameter settings:
   if(boost::iequals(CAFile, "NONE") || boost::iequals(CAFile, "IGNORE")) {
      CAFile = std::string();
   }
   if(boost::iequals(CRLFile, "NONE") || boost::iequals(CRLFile, "IGNORE")) {
      CRLFile = std::string();
   }
   if(boost::iequals(CertFile, "NONE") || boost::iequals(CertFile, "IGNORE")) {
      CertFile = std::string();
   }
   if(boost::iequals(KeyFile, "NONE") || boost::iequals(KeyFile, "IGNORE")) {
      KeyFile = std::string();
   }
   if(boost::iequals(CertKeyFile, "NONE") || boost::iequals(CertKeyFile, "IGNORE")) {
      CertKeyFile = std::string();
   }

   return true;
}


// ###### Set backend #######################################################
bool DatabaseConfiguration::setBackend(const std::string& backendName)
{
   BackendName = backendName;
   Backend     = DatabaseBackendType::Invalid;
   for(RegisteredBackend* registeredBackend : *BackendList) {
      if(registeredBackend->Name == BackendName) {
         Backend = registeredBackend->Type;
      }
   }
   if(Backend == DatabaseBackendType::Invalid) {
      HPCT_LOG(error) << "Invalid backend name " << backendName
                      << ". Available backends: ";
      for(DatabaseConfiguration::RegisteredBackend* registeredBackend : *DatabaseConfiguration::BackendList) {
         HPCT_LOG(error) << registeredBackend->Name << " ";
      }
      return false;
   }
   return true;
}


// ###### Set import mode ###################################################
bool DatabaseConfiguration::setConnectionFlags(const std::string& connectionFlagNames)
{
   std::istringstream flags(connectionFlagNames);
   std::string        flag;
   Flags = ConnectionFlags::None;
   while(std::getline(flags, flag, ' ')) {
      if(flag == "DisableTLS") {
         Flags |= ConnectionFlags::DisableTLS;
      }
      else if(flag == "AllowInvalidCertificate") {
         Flags |= ConnectionFlags::AllowInvalidCertificate;
      }
      else if(flag == "AllowInvalidHostname") {
         Flags |= ConnectionFlags::AllowInvalidHostname;
      }
      else if(!boost::iequals(flag, "NONE")) {
         HPCT_LOG(error) << "Invalid connection flag " << flag;
         return false;
      }
   }
   return true;
}


// ###### << operator #######################################################
std::ostream& operator<<(std::ostream& os, const DatabaseConfiguration& configuration)
{
   os << "Database configuration:\n"
      << "  Backend               = " << configuration.BackendName    << "\n"
      << "  Reconnect Delay       = " << configuration.ReconnectDelay << " s" << "\n"
      << "  Server                = " << configuration.Server         << "\n"
      << "  Port                  = " << configuration.Port           << "\n"
      << "  User                  = " << configuration.User           << "\n"
      << "  Password              = " << ((configuration.Password.size() > 0) ? "****************" : "(none)") << "\n"
      << "  CA File               = " << configuration.CAFile         << "\n"
      << "  CRL File              = " << configuration.CRLFile        << "\n"
      << "  Certificate File      = " << configuration.CertFile       << "\n"
      << "  Key File              = " << configuration.KeyFile        << "\n"
      << "  Certificate+Key File  = " << configuration.CertKeyFile    << "\n"
      << "  Database              = " << configuration.Database       << "\n"
      << "  Flags                 =";
   if(configuration.Flags & ConnectionFlags::DisableTLS) {
      os << " DisableTLS";
   }
   if(configuration.Flags & ConnectionFlags::AllowInvalidCertificate) {
      os << " AllowInvalidCertificate";
   }
   if(configuration.Flags & ConnectionFlags::AllowInvalidHostname) {
      os << " AllowInvalidHostname";
   }
   return os;
}


// ###### Register backend ##################################################
bool DatabaseConfiguration::registerBackend(const DatabaseBackendType type,
                                            const std::string&        name,
                                            DatabaseClientBase*       (*createClientFunction)(const DatabaseConfiguration& configuration))
{
   if(BackendList == nullptr) {
      BackendList = new std::list<RegisteredBackend*>;
      assert(BackendList != nullptr);
   }
   RegisteredBackend* registeredBackend = new RegisteredBackend;
   registeredBackend->Type                 = type;
   registeredBackend->Name                 = name;
   registeredBackend->CreateClientFunction = createClientFunction;
   BackendList->push_back(registeredBackend);
   return true;
}


// ###### Create new database client instance ###############################
DatabaseClientBase* DatabaseConfiguration::createClient()
{
   for(RegisteredBackend* registeredBackend : *BackendList) {
      if(registeredBackend->Type == Backend) {
         return registeredBackend->CreateClientFunction(*this);
      }
   }
   return nullptr;
}
