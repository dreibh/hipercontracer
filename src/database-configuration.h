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

#ifndef DATABASE_CONFIGURATION_H
#define DATABASE_CONFIGURATION_H

#include <filesystem>
#include <string>

#include <boost/program_options.hpp>


enum DatabaseBackendType {
   Invalid        = 0,

   SQL_Generic    = (1 << 0),
   NoSQL_Generic  = (1 << 1),

   SQL_Debug      = SQL_Generic | (1 << 16),
   SQL_MariaDB    = SQL_Generic | (1 << 17),
   SQL_PostgreSQL = SQL_Generic | (1 << 18),
   SQL_Cassandra  = SQL_Generic | (1 << 19),

   NoSQL_Debug    = NoSQL_Generic | (1 << 24),
   NoSQL_MongoDB  = NoSQL_Generic | (1 << 25)
};


enum ImportModeType {
   KeepImportedFiles   = 0,   // Keep the files where they are
   MoveImportedFiles   = 1,   // Move into "good file" directory
   DeleteImportedFiles = 2    // Delete
};


class DatabaseClientBase;

class DatabaseConfiguration
{
   public:
   DatabaseConfiguration();
   ~DatabaseConfiguration();

   inline DatabaseBackendType getBackend()        const { return Backend;        }
   inline const std::string&  getServer()         const { return Server;         }
   inline const uint16_t      getPort()           const { return Port;           }
   inline const std::string&  getUser()           const { return User;           }
   inline const std::string&  getPassword()       const { return Password;       }
   inline const std::string&  getCAFile()         const { return CAFile;         }
   inline const std::string&  getDatabase()       const { return Database;       }
   inline const unsigned int  getReconnectDelay() const { return ReconnectDelay; }
   inline ImportModeType      getImportMode()     const { return ImportMode;     }
   inline const unsigned int  getImportMaxDepth() const { return ImportMaxDepth; }
   inline const std::filesystem::path& getImportFilePath() const { return ImportFilePath; }
   inline const std::filesystem::path& getBadFilePath()    const { return BadFilePath;    }
   inline const std::filesystem::path& getGoodFilePath()   const { return GoodFilePath;   }

   inline void setBackend(const DatabaseBackendType backend)  { Backend = backend;       }
   inline void setImportMode(const ImportModeType importMode) { ImportMode = importMode; }

   bool readConfiguration(const std::filesystem::path& configurationFile);
   void printConfiguration(std::ostream& os) const;
   DatabaseClientBase* createClient();

   private:
   boost::program_options::options_description OptionsDescription;
   std::string           BackendName;
   DatabaseBackendType   Backend;
   unsigned int          ReconnectDelay;
   std::string           Server;
   uint16_t              Port;
   std::string           User;
   std::string           Password;
   std::string           CAFile;
   std::string           Database;
   std::string           ImportModeName;
   ImportModeType        ImportMode;
   unsigned int          ImportMaxDepth;
   std::filesystem::path ImportFilePath;
   std::filesystem::path BadFilePath;
   std::filesystem::path GoodFilePath;
};

#endif
