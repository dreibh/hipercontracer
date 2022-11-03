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
      ("import_max_depth",  boost::program_options::value<unsigned int>(&ImportMaxDepth)->default_value(5),  "import max depth)")
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
   std::ifstream                         configurationInputStream(configurationFile);
   boost::program_options::variables_map vm = boost::program_options::variables_map();

   boost::program_options::store(boost::program_options::parse_config_file(configurationInputStream , OptionsDescription), vm);
   boost::program_options::notify(vm);

   // ====== Check options ==================================================
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

   // ====== Check directories ==============================================
   std::error_code ec;
   if(!is_directory(ImportFilePath, ec)) {
      HPCT_LOG(error) << "Import file path " << ImportFilePath << " does not exist: " << ec;
      return false;
   }
   if(!is_directory(GoodFilePath, ec)) {
      HPCT_LOG(error) << "Good file path " << GoodFilePath << " does not exist: " << ec;
      return false;
   }
   if(!is_directory(BadFilePath, ec)) {
      HPCT_LOG(error) << "Bad file path " << BadFilePath << " does not exist: " << ec;
      return false;
   }

   if(is_subdir_of(GoodFilePath, ImportFilePath)) {
      HPCT_LOG(error) << "Good file path " << GoodFilePath
                      << " must not be within import file path " << ImportFilePath;
      return false;
   }
   if(is_subdir_of(BadFilePath, ImportFilePath)) {
      HPCT_LOG(error) << "Bad file path " << BadFilePath
                      << " must not be within import file path " << ImportFilePath;
      return false;
   }

   return true;
}


// ###### Print reader status ###############################################
void DatabaseConfiguration::printConfiguration(std::ostream& os) const
{
   os << "Import configuration:"                 << std::endl
      << "Import File Path = " << ImportFilePath << " (max depth: " << ImportMaxDepth << ")" << std::endl
      << "Good File Path   = " << GoodFilePath   << std::endl
      << "Bad File Path    = " << BadFilePath    << std::endl

      << "Database configuration:"               << std::endl
      << "Backend          = " << BackendName    << std::endl
      << "Reconnect Delay  = " << ReconnectDelay << " s" << std::endl
      << "Server           = " << Server         << std::endl
      << "Port             = " << Port           << std::endl
      << "User             = " << User           << std::endl
      << "Password         = " << ((Password.size() > 0) ? "****************" : "(none)") << std::endl
      << "CA File          = " << CAFile         << std::endl
      << "Database         = " << Database       << std::endl;
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
