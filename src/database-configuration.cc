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

#include "database-configuration.h"
#include "logger.h"
#include "tools.h"

#include "databaseclient-debug.h"
#include "databaseclient-mariadb.h"

#include <fstream>


// ###### Constructor #######################################################
DatabaseConfiguration::DatabaseConfiguration()
   : OptionsDescription("Options")
{
   OptionsDescription.add_options()
      ("dbserver",          boost::program_options::value<std::string>(&Server),                             "database server")
      ("dbport",            boost::program_options::value<uint16_t>(&Port),                                  "database port")
      ("dbuser",            boost::program_options::value<std::string>(&User),                               "database username")
      ("dbpassword",        boost::program_options::value<std::string>(&Password),                           "database password")
      ("dbcafile",          boost::program_options::value<std::string>(&CAFile),                             "database CA file")
      ("database",          boost::program_options::value<std::string>(&Database),                           "database name")
      ("dbbackend",         boost::program_options::value<std::string>(&BackendName),                        "database backend")
      ("dbreconnectdelay",  boost::program_options::value<unsigned int>(&ReconnectDelay)->default_value(60), "database reconnect delay (in s)")
      ("import_mode",       boost::program_options::value<std::string>(&ImportModeName),                     "import mode")
      ("import_max_depth",  boost::program_options::value<unsigned int>(&ImportMaxDepth)->default_value(6),  "import max depth)")
      ("import_file_path",  boost::program_options::value<std::filesystem::path>(&ImportFilePath),           "path for input data")
      ("bad_file_path",     boost::program_options::value<std::filesystem::path>(&BadFilePath),              "path for bad files")
      ("good_file_path",    boost::program_options::value<std::filesystem::path>(&GoodFilePath),             "path for good files")
   ;
   BackendName    = "Invalid";
   Backend        = DatabaseBackendType::Invalid;
   ImportModeName = "KeepImportedFiles";
   ImportMode     = ImportModeType::KeepImportedFiles;
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

   boost::program_options::variables_map vm = boost::program_options::variables_map();
   boost::program_options::store(boost::program_options::parse_config_file(configurationInputStream , OptionsDescription), vm);
   boost::program_options::notify(vm);

   // ====== Check options ==================================================
   if(!setBackend(BackendName))           return false;
   if(!setImportMode(ImportModeName))     return false;
   if(!setImportMaxDepth(ImportMaxDepth)) return false;
   if(!setImportFilePath(ImportFilePath)) return false;
   if(!setGoodFilePath(GoodFilePath))     return false;
   if(!setBadFilePath(BadFilePath))       return false;

   return true;
}


// ###### Set backend #######################################################
bool DatabaseConfiguration::setBackend(const std::string& backendName)
{
   BackendName = backendName;
   if( (BackendName == "MySQL") || (BackendName == "MariaDB") ) {
      Backend = DatabaseBackendType::SQL_MariaDB;
   }
   else if(BackendName == "PostgreSQL") {
      Backend = DatabaseBackendType::SQL_PostgreSQL;
   }
   else if(BackendName == "MongoDB") {
      Backend = DatabaseBackendType::NoSQL_MongoDB;
   }
   else if(BackendName == "DebugSQL") {
      Backend = DatabaseBackendType::SQL_Debug;
   }
   else if(BackendName == "DebugNoSQL") {
      Backend = DatabaseBackendType::NoSQL_Debug;
   }
   else {
      HPCT_LOG(error) << "Invalid backend name " << Backend;
      return false;
   }
   return true;
}


// ###### Set import mode ###################################################
bool DatabaseConfiguration::setImportMode(const std::string& importModeName)
{
   ImportModeName = importModeName;
   if(ImportModeName == "KeepImportedFiles") {
      ImportMode = ImportModeType::KeepImportedFiles;
   }
   else if(ImportModeName == "MoveImportedFiles") {
      ImportMode = ImportModeType::MoveImportedFiles;
   }
   else if(ImportModeName == "DeleteImportedFiles") {
      ImportMode = ImportModeType::DeleteImportedFiles;
   }
   else {
      HPCT_LOG(error) << "Invalid import mode name " << ImportModeName;
      return false;
   }
   return true;
}


// ###### Set import max depth ##############################################
bool DatabaseConfiguration::setImportMaxDepth(const unsigned int importMaxDepth)
{
   ImportMaxDepth = importMaxDepth;
   if(ImportMaxDepth < 1) {
      HPCT_LOG(error) << "Import max depth must be at least 1!";
      return false;
   }
   return true;
}


// ###### Set import file path ##############################################
bool DatabaseConfiguration::setImportFilePath(const std::filesystem::path& importFilePath)
{
   try {
      ImportFilePath = std::filesystem::canonical(std::filesystem::absolute(importFilePath));
      if(std::filesystem::is_directory(ImportFilePath)) {
         return true;
      }
   }
   catch(...) { }
   HPCT_LOG(error) << "Invalid or inaccessible import file path " << ImportFilePath;
   return false;
}


// ###### Set good file path ################################################
bool DatabaseConfiguration::setGoodFilePath(const std::filesystem::path& goodFilePath)
{
   try {
      GoodFilePath = std::filesystem::canonical(std::filesystem::absolute(goodFilePath));
      if(std::filesystem::is_directory(GoodFilePath)) {
         return true;
      }
   }
   catch(...) { }
   HPCT_LOG(error) << "Invalid or inaccessible good file path " << GoodFilePath;
   return false;
}


// ###### Set bad file path #################################################
bool DatabaseConfiguration::setBadFilePath(const std::filesystem::path& badFilePath)
{
   try {
      BadFilePath = std::filesystem::canonical(std::filesystem::absolute(badFilePath));
      if(std::filesystem::is_directory(BadFilePath)) {
         return true;
      }
   }
   catch(...) { }
   HPCT_LOG(error) << "Invalid or inaccessible bad file path " << BadFilePath;
   return false;
}


// ###### << operator #######################################################
std::ostream& operator<<(std::ostream& os, const DatabaseConfiguration& configuration)
{
   os << "Import configuration:" << "\n"
      << "Import mode      = ";
   switch(configuration.ImportMode) {
      case KeepImportedFiles:
         os << "KeepImportedFiles";
       break;
      case MoveImportedFiles:
         os << "MoveImportedFiles";
       break;
      case DeleteImportedFiles:
         os << "DeleteImportedFiles";
       break;
      default:
         assert(false);
       break;
   }
   os << "\n"
      << "Import File Path = " << configuration.ImportFilePath << " (max depth: " << configuration.ImportMaxDepth << ")" << "\n"
      << "Good File Path   = " << configuration.GoodFilePath   << "\n"
      << "Bad File Path    = " << configuration.BadFilePath    << "\n"

      << "Database configuration:"               << "\n"
      << "Backend          = " << configuration.BackendName    << "\n"
      << "Reconnect Delay  = " << configuration.ReconnectDelay << " s" << "\n"
      << "Server           = " << configuration.Server         << "\n"
      << "Port             = " << configuration.Port           << "\n"
      << "User             = " << configuration.User           << "\n"
      << "Password         = " << ((configuration.Password.size() > 0) ? "****************" : "(none)") << "\n"
      << "CA File          = " << configuration.CAFile         << "\n"
      << "Database         = " << configuration.Database;
   return os;
}


// ###### Create new database client instance ###############################
DatabaseClientBase* DatabaseConfiguration::createClient()
{
   DatabaseClientBase* databaseClient = nullptr;

   switch(Backend) {
      case SQL_Debug:
      case NoSQL_Debug:
          databaseClient = new DebugClient(*this);
       break;
      case SQL_MariaDB:
          databaseClient = new MariaDBClient(*this);
       break;
      default:
       break;
   }

   return databaseClient;
}
