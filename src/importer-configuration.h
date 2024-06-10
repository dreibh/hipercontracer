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
// Copyright (C) 2015-2024 by Thomas Dreibholz
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

#ifndef IMPORTER_CONFIGURATION_H
#define IMPORTER_CONFIGURATION_H

#include <filesystem>
#include <list>
#include <string>

#include <boost/program_options.hpp>


enum ImportModeType {
   KeepImportedFiles   = 0,   // Keep the files where they are
   MoveImportedFiles   = 1,   // Move into "good file" directory
   DeleteImportedFiles = 2    // Delete
};

class ImporterConfiguration
{
   public:
   ImporterConfiguration();
   ~ImporterConfiguration();

   inline ImportModeType               getImportMode()      const { return ImportMode;     }
   inline unsigned int                 getImportMaxDepth()  const { return ImportMaxDepth; }
   inline const std::filesystem::path& getImportFilePath()  const { return ImportFilePath; }
   inline const std::filesystem::path& getGoodFilePath()    const { return GoodFilePath;   }
   inline const std::filesystem::path& getBadFilePath()     const { return BadFilePath;    }

   bool setImportMode(const std::string& importModeName);
   bool setImportMaxDepth(const unsigned int importMaxDepth);
   bool setImportFilePath(const std::filesystem::path& importFilePath);
   bool setGoodFilePath(const std::filesystem::path& goodFilePath);
   bool setBadFilePath(const std::filesystem::path& badFilePath);

   bool readConfiguration(const std::filesystem::path& configurationFile);

   friend std::ostream& operator<<(std::ostream& os, const ImporterConfiguration& configuration);

   private:
   boost::program_options::options_description OptionsDescription;

   std::string           ImportModeName;
   ImportModeType        ImportMode;
   unsigned int          ImportMaxDepth;
   std::filesystem::path ImportFilePath;
   std::filesystem::path BadFilePath;
   std::filesystem::path GoodFilePath;
};

#endif
