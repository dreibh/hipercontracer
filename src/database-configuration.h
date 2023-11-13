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

#ifndef DATABASE_CONFIGURATION_H
#define DATABASE_CONFIGURATION_H

#include <filesystem>
#include <list>
#include <string>

#include <boost/program_options.hpp>


class DatabaseClientBase;


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


enum ConnectionFlags {
   None                    = 0,
   DisableTLS              = (1 << 0),
   AllowInvalidCertificate = (1 << 1),
   AllowInvalidHostname    = (1 << 2)
};

class DatabaseConfiguration
{
   public:
   DatabaseConfiguration();
   ~DatabaseConfiguration();

   inline DatabaseBackendType          getBackend()         const { return (DatabaseBackendType)Backend; }
   inline const std::string&           getServer()          const { return Server;                       }
   inline uint16_t                     getPort()            const { return Port;                         }
   inline const std::string&           getUser()            const { return User;                         }
   inline const std::string&           getPassword()        const { return Password;                     }
   inline ConnectionFlags              getConnectionFlags() const { return (ConnectionFlags)Flags;       }
   inline const std::string&           getCAFile()          const { return CAFile;                       }
   inline const std::string&           getCRLFile()         const { return CRLFile;                      }
   inline const std::string&           getCertFile()        const { return CertFile;                     }
   inline const std::string&           getKeyFile()         const { return KeyFile;                      }
   inline const std::string&           getCertKeyFile()     const { return CertKeyFile;                  }
   inline const std::string&           getDatabase()        const { return Database;                     }
   inline unsigned int                 getReconnectDelay()  const { return ReconnectDelay;               }

   inline ImportModeType               getImportMode()      const { return ImportMode;                   }
   inline unsigned int                 getImportMaxDepth()  const { return ImportMaxDepth;               }
   inline const std::filesystem::path& getImportFilePath()  const { return ImportFilePath;               }
   inline const std::filesystem::path& getGoodFilePath()    const { return GoodFilePath;                 }
   inline const std::filesystem::path& getBadFilePath()     const { return BadFilePath;                  }

   bool setBackend(const std::string& backendName);
   bool setConnectionFlags(const std::string& connectionFlagNames);

   bool setImportMode(const std::string& importModeName);
   bool setImportMaxDepth(const unsigned int importMaxDepth);
   bool setImportFilePath(const std::filesystem::path& importFilePath);
   bool setGoodFilePath(const std::filesystem::path& goodFilePath);
   bool setBadFilePath(const std::filesystem::path& badFilePath);

   bool readConfiguration(const std::filesystem::path& configurationFile,
                          const bool                   isImporter = true);
   DatabaseClientBase* createClient();
   static bool registerBackend(const DatabaseBackendType type,
                               const std::string&        name,
                               DatabaseClientBase*       (*createClientFunction)(const DatabaseConfiguration& configuration));

   friend std::ostream& operator<<(std::ostream& os, const DatabaseConfiguration& configuration);

   private:
   struct RegisteredBackend {
      std::string         Name;
      DatabaseBackendType Type;
      DatabaseClientBase* (*CreateClientFunction)(const DatabaseConfiguration& configuration);
   };

   static std::list<RegisteredBackend*>*       BackendList;
   boost::program_options::options_description OptionsDescription;
   std::string                                 BackendName;
   unsigned int                                Backend;
   std::string                                 FlagNames;
   unsigned int                                Flags;
   unsigned int                                ReconnectDelay;
   std::string                                 Server;
   uint16_t                                    Port;
   std::string                                 User;
   std::string                                 Password;
   std::string                                 CAFile;
   std::string                                 CRLFile;
   std::string                                 CertFile;
   std::string                                 KeyFile;
   std::string                                 CertKeyFile;
   std::string                                 Database;
   std::string                                 ImportModeName;
   ImportModeType                              ImportMode;
   unsigned int                                ImportMaxDepth;
   std::filesystem::path                       ImportFilePath;
   std::filesystem::path                       BadFilePath;
   std::filesystem::path                       GoodFilePath;
};


#define REGISTER_BACKEND(type, name, backend) \
   static DatabaseClientBase* createClient_##backend(const DatabaseConfiguration& configuration) { return new backend(configuration); } \
   static bool Registered = DatabaseConfiguration::registerBackend(type, name, createClient_##backend);
#define REGISTER_BACKEND_ALIAS(type, name, backend, alias) \
   static bool Registered##alias = DatabaseConfiguration::registerBackend(type, name, createClient_##backend);

#endif
