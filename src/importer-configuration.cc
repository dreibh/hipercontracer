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

#include "importer-configuration.h"
#include "logger.h"
#include "tools.h"

#include <fstream>
#include <regex>


// ###### Constructor #######################################################
ImporterConfiguration::ImporterConfiguration()
   : OptionsDescription("Options")
{
   OptionsDescription.add_options()
      ("import_mode",          boost::program_options::value<std::string>(&ImportModeName),                                 "import mode")
      ("import_max_depth",     boost::program_options::value<unsigned int>(&ImportMaxDepth)->default_value(6),              "import max depth")
      ("import_path_filter",   boost::program_options::value<std::string>(&ImportPathFilter)->default_value(std::string()), "import path filter")
      ("move_directory_depth", boost::program_options::value<unsigned int>(&MoveDirectoryDepth)->default_value(1),          "move directory depth")
      ("move_timestamp_depth", boost::program_options::value<unsigned int>(&MoveTimestampDepth)->default_value(3),          "move timestamp depth")
      ("import_file_path",     boost::program_options::value<std::filesystem::path>(&ImportFilePath),                       "path for input data")
      ("bad_file_path",        boost::program_options::value<std::filesystem::path>(&BadFilePath),                          "path for bad files")
      ("good_file_path",       boost::program_options::value<std::filesystem::path>(&GoodFilePath),                         "path for good files")
      ("status_interval",      boost::program_options::value<unsigned int>(&StatusInterval)->default_value(60),             "status interval (s)")
      ("gc_interval",          boost::program_options::value<unsigned int>(&GarbageCollectionInterval)->default_value(275), "garbage collection interval (s)")
      ("gc_max_age",           boost::program_options::value<unsigned int>(&GarbageCollectionMaxAge)->default_value(3600),  "garbage collection max age (s)")
      ("table",                boost::program_options::value<std::vector<std::string>>(&Tables),                            "mapping of reader:table");

   ImportModeName = "KeepImportedFiles";
   ImportMode     = ImportModeType::KeepImportedFiles;
}


// ###### Destructor ########################################################
ImporterConfiguration::~ImporterConfiguration()
{
}


// ###### Read importer configuration #######################################
bool ImporterConfiguration::readConfiguration(const std::filesystem::path& configurationFile)
{
   const static std::regex tableRegEx("^([[a-zA-Z][a-zA-Z0-9]*):([a-zA-Z][a-zA-Z0-9_]*)$");
   std::ifstream           configurationInputStream(configurationFile);

   if(!configurationInputStream.good()) {
      HPCT_LOG(error) << "Unable to read importer configuration from " << configurationFile;
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

   TableMap.clear();
   for(const std::string& table : Tables) {
      std::smatch match;
      if(!std::regex_match(table, match, tableRegEx)) {
         HPCT_LOG(error) << "Parsing configuration file " << configurationFile
                        << " had invalid table maapping "  << table;
         return false;
      }
      TableMap.insert_or_assign(match[1], match[2]);
   }

   // ====== Check options ==================================================
   if(!setImportMode(ImportModeName))         return false;
   if(!setImportMaxDepth(ImportMaxDepth))     return false;
   if(!setImportPathFilter(ImportPathFilter)) return false;
   if(!setImportFilePath(ImportFilePath))     return false;
   if(!setGoodFilePath(GoodFilePath))         return false;
   if(!setBadFilePath(BadFilePath))           return false;
   StatusInterval            = std::max(5U,  StatusInterval);
   GarbageCollectionInterval = std::max(10U, GarbageCollectionInterval);
   GarbageCollectionMaxAge   = std::max(60U, GarbageCollectionMaxAge);

   return true;
}


// ###### Get table name ####################################################
const std::string& ImporterConfiguration::getTableName(const std::string& readerName,
                                                       const std::string& defaultTableName) const
{
   std::map<std::string, std::string>::const_iterator found = TableMap.find(readerName);
   if(found == TableMap.end()) {
      return defaultTableName;
   }
   return found->second;
}


// ###### Set import mode ###################################################
bool ImporterConfiguration::setImportMode(const std::string& importModeName)
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
bool ImporterConfiguration::setImportMaxDepth(const unsigned int importMaxDepth)
{
   ImportMaxDepth = importMaxDepth;
   if(ImportMaxDepth < 1) {
      HPCT_LOG(error) << "Import max depth must be at least 1!";
      return false;
   }
   return true;
}


// ###### Set import path filter ############################################
bool ImporterConfiguration::setImportPathFilter(const std::string& importPathFilter)
{
   ImportPathFilter = importPathFilter;
   try {
      const std::regex importPathFilterRegExp(ImportPathFilter);
   }
   catch(const std::regex_error& e) {
      HPCT_LOG(error) << "Invalid regular expression for import path filter \""
                      << ImportPathFilter << "\": " << e.what();
      return false;
   }
   return true;
}


// ###### Set import file path ##############################################
bool ImporterConfiguration::setImportFilePath(const std::filesystem::path& importFilePath)
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
bool ImporterConfiguration::setGoodFilePath(const std::filesystem::path& goodFilePath)
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
bool ImporterConfiguration::setBadFilePath(const std::filesystem::path& badFilePath)
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


// ###### Set move directory depth ##########################################
bool ImporterConfiguration::setMoveDirectoryDepth(const unsigned int moveDirectoryDepth)
{
   MoveDirectoryDepth = moveDirectoryDepth;
   return true;
}


// ###### Set move timestamp depth ##########################################
bool ImporterConfiguration::setMoveTimestampDepth(const unsigned int moveTimestampDepth)
{
   MoveTimestampDepth = moveTimestampDepth;
   return true;
}


// ###### << operator #######################################################
std::ostream& operator<<(std::ostream& os, const ImporterConfiguration& configuration)
{
   os << "Importer configuration:" << "\n"
      << "  Import Mode          = ";
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
         abort();
       break;
   }
   os << "\n"
      << "  Import Filter         = " << configuration.ImportPathFilter          << "\n"
      << "  Import File Path      = " << configuration.ImportFilePath            << " (max depth: " << configuration.ImportMaxDepth << ")" << "\n"
      << "  Good File Path        = " << configuration.GoodFilePath              << "\n"
      << "  Bad File Path         = " << configuration.BadFilePath               << "\n"
      << "  Move Directory Depth  = " << configuration.MoveDirectoryDepth        << "\n"
      << "  Move Timestamp Depth  = " << configuration.MoveTimestampDepth        << "\n"
      << "  Status Interval       = " << configuration.StatusInterval            << " s\n"
      << "  Directory GC Interval = " << configuration.GarbageCollectionInterval << " s\n"
      << "  Directory GC Max Age  = " << configuration.GarbageCollectionMaxAge   << " s\n"
      << "  Custom Table Mapping  = {";
   for(std::map<std::string, std::string>::const_iterator iterator = configuration.TableMap.begin();
      iterator != configuration.TableMap.end(); iterator++) {
      os << " " << iterator->first << ":" << iterator->second;
   }
   os << " }\n";
   return os;
}
