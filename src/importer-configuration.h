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

#ifndef IMPORTER_CONFIGURATION_H
#define IMPORTER_CONFIGURATION_H

#include <filesystem>
#include <list>
#include <string>
#include <vector>

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

   inline ImportModeType               getImportMode()                const { return ImportMode;                }
   inline unsigned int                 getImportMaxDepth()            const { return ImportMaxDepth;            }
   inline const std::string&           getImportPathFilter()          const { return ImportPathFilter;          }
   inline const std::filesystem::path& getImportFilePath()            const { return ImportFilePath;            }
   inline const std::filesystem::path& getGoodFilePath()              const { return GoodFilePath;              }
   inline const std::filesystem::path& getBadFilePath()               const { return BadFilePath;               }
   inline unsigned int                 getMoveDirectoryDepth()        const { return MoveDirectoryDepth;        }
   inline unsigned int                 getMoveTimestampDepth()        const { return MoveTimestampDepth;        }
   inline unsigned int                 getGarbageCollectionInterval() const { return GarbageCollectionInterval; }
   inline unsigned int                 getGarbageCollectionMaxAge()   const { return GarbageCollectionMaxAge;   }
   inline unsigned int                 getStatusInterval()            const { return StatusInterval;            }
   const std::string& getTableName(const std::string& readerName,
                                   const std::string& defaultTableName) const;

   bool setImportMode(const std::string& importModeName);
   bool setImportMaxDepth(const unsigned int importMaxDepth);
   bool setImportPathFilter(const std::string& importPathFilter);
   bool setImportFilePath(const std::filesystem::path& importFilePath);
   bool setGoodFilePath(const std::filesystem::path& goodFilePath);
   bool setBadFilePath(const std::filesystem::path& badFilePath);
   bool setMoveDirectoryDepth(const unsigned int moveDirectoryDepth);
   bool setMoveTimestampDepth(const unsigned int moveTimestampDepth);

   bool readConfiguration(const std::filesystem::path& configurationFile);

   friend std::ostream& operator<<(std::ostream& os, const ImporterConfiguration& configuration);

   private:
   boost::program_options::options_description OptionsDescription;

   std::string                                 ImportModeName;
   ImportModeType                              ImportMode;
   unsigned int                                ImportMaxDepth;
   std::string                                 ImportPathFilter;
   unsigned int                                MoveDirectoryDepth;
   unsigned int                                MoveTimestampDepth;
   std::filesystem::path                       ImportFilePath;
   std::filesystem::path                       BadFilePath;
   std::filesystem::path                       GoodFilePath;
   std::vector<std::string>                    Tables;
   std::map<std::string, std::string>          TableMap;
   unsigned int                                StatusInterval;
   unsigned int                                GarbageCollectionInterval;
   unsigned int                                GarbageCollectionMaxAge;
};

#endif
