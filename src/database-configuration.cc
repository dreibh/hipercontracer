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

#include <fstream>

#include <boost/algorithm/string.hpp>


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
      ("import_mode",       boost::program_options::value<std::string>(&ImportModeName),                     "import mode")
      ("import_max_depth",  boost::program_options::value<unsigned int>(&ImportMaxDepth)->default_value(6),  "import max depth)")
      ("import_file_path",  boost::program_options::value<std::filesystem::path>(&ImportFilePath),           "path for input data")
      ("bad_file_path",     boost::program_options::value<std::filesystem::path>(&BadFilePath),              "path for bad files")
      ("good_file_path",    boost::program_options::value<std::filesystem::path>(&GoodFilePath),             "path for good files")
   ;
   BackendName     = "Invalid";
   Backend         = DatabaseBackendType::Invalid;
   Flags           = ConnectionFlags::None;
   ImportModeName  = "KeepImportedFiles";
   ImportMode      = ImportModeType::KeepImportedFiles;
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
   if(!setImportMode(ImportModeName))     return false;
   if(!setImportMaxDepth(ImportMaxDepth)) return false;
   if(!setImportFilePath(ImportFilePath)) return false;
   if(!setGoodFilePath(GoodFilePath))     return false;
   if(!setBadFilePath(BadFilePath))       return false;

   // Legacy parameter handling:
   if(boost::iequals(CAFile, "NONE") || boost::iequals(CAFile, "IGNORE")) {
      CAFile = std::string();
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
      HPCT_LOG(error) << "Invalid backend name " << Backend
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
      << "CRL File         = " << configuration.CRLFile        << "\n"
      << "Certificate File = " << configuration.CertFile       << "\n"
      << "Key File         = " << configuration.KeyFile        << "\n"
      << "Cert.+Key File   = " << configuration.CertKeyFile    << "\n"
      << "Database         = " << configuration.Database       << "\n"
      << "Flags            =";
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
