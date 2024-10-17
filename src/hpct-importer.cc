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

#include "logger.h"
#include "tools.h"

// #include "reader-jitter.h"
#include "reader-ping.h"
#include "reader-traceroute.h"
#include "universal-importer.h"

#include <filesystem>
#include <iostream>

#include <boost/program_options.hpp>



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   unsigned int          logLevel;
   bool                  logColor;
   std::filesystem::path logFile;
   std::filesystem::path databaseConfigFile;
   std::filesystem::path importerConfigFile;
   std::string           importModeName;
   unsigned int          importMaxDepth;
   std::filesystem::path importFilePath;
   std::filesystem::path badFilePath;
   std::filesystem::path goodFilePath;
   std::string           importFilePathFilter;
   bool                  quitWhenIdle;
   unsigned int          pingWorkers;
   unsigned int          tracerouteWorkers;
   // unsigned int          jitterWorkers;
   unsigned int          pingTransactionSize;
   unsigned int          tracerouteTransactionSize;
   // unsigned int          jitterTransactionSize;

   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )

      ( "loglevel,L",
           boost::program_options::value<unsigned int>(&logLevel)->default_value(boost::log::trivial::severity_level::info),
           "Set logging level" )
      ( "logfile,O",
           boost::program_options::value<std::filesystem::path>(&logFile)->default_value(std::filesystem::path()),
           "Log file" )
      ( "logcolor,Z",
           boost::program_options::value<bool>(&logColor)->default_value(true),
           "Use ANSI color escape sequences for log output" )
      ( "verbose,v",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::trace),
           "Verbose logging level" )
      ( "quiet,q",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::warning),
           "Quiet logging level" )

      ( "importer-config,C",        boost::program_options::value<std::filesystem::path>(&importerConfigFile),      "Importer configuration file")
      ( "database-config,D",        boost::program_options::value<std::filesystem::path>(&databaseConfigFile),      "Database configuration file")

      ("import-mode,X",             boost::program_options::value<std::string>(&importModeName),                    "Override import mode")
      ("import-max-depth,M",        boost::program_options::value<unsigned int>(&importMaxDepth)->default_value(0), "Override import max depth")
      ("import-file-path,I",        boost::program_options::value<std::filesystem::path>(&importFilePath),          "Override path for input files")
      ("bad-file-path,B",           boost::program_options::value<std::filesystem::path>(&badFilePath),             "Override path for bad files")
      ("good-file-path,G",          boost::program_options::value<std::filesystem::path>(&goodFilePath),            "Override path for good files")
      ("import-file-path-filter,F", boost::program_options::value<std::string>(&importFilePathFilter),              "Override import path filter (regular expression)")
      ("quit-when-idle,Q",
          boost::program_options::value<bool>(&quitWhenIdle)->implicit_value(true)->default_value(false),
          "Quit importer when idle")

      ( "ping-workers",
           boost::program_options::value<unsigned int>(&pingWorkers)->default_value(1),
           "Number of Ping import worker threads" )
      ( "ping-files",
           boost::program_options::value<unsigned int>(&pingTransactionSize)->default_value(1),
           "Number of Ping files per transaction" )
      ( "traceroute-workers",
           boost::program_options::value<unsigned int>(&tracerouteWorkers)->default_value(1),
           "Number of Traceroute import worker threads" )
      ( "traceroute-files",
           boost::program_options::value<unsigned int>(&tracerouteTransactionSize)->default_value(1),
           "Number of Traceroute files per transaction" )
#if 0
      ( "jitter-workers",
           boost::program_options::value<unsigned int>(&jitterWorkers)->default_value(1),
           "Number of Jitter import worker threads" )
      ( "jitter-files",
           boost::program_options::value<unsigned int>(&jitterTransactionSize)->default_value(1),
           "Number of Jitter files per transaction" )
#endif
   ;

   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
                                       style(
                                          boost::program_options::command_line_style::style_t::default_style|
                                          boost::program_options::command_line_style::style_t::allow_long_disguise
                                       ).
                                       options(commandLineOptions).
                                       run(), vm);
      boost::program_options::notify(vm);
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: Bad parameter: " << e.what() << "\n";
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << "\n"
                 << commandLineOptions;
       return 1;
   }

   if(importerConfigFile.empty()) {
      std::cerr << "ERROR: No importer configuration file provided!\n";
      return 1;
   }
   if(databaseConfigFile.empty()) {
      std::cerr << "ERROR: No database configuration file provided!\n";
      return 1;
   }
   if(pingWorkers + tracerouteWorkers < 1) {
      std::cerr << "ERROR: At least one worker is needed!\n";
      return 1;
   }

   // ====== Read Importer configuration ====================================
   ImporterConfiguration importerConfiguration;
   if(!importerConfiguration.readConfiguration(importerConfigFile)) {
      exit(1);
   }
   if(importModeName.size() > 0) {
      if(!importerConfiguration.setImportMode(importModeName)) return 1;
   }
   if(importMaxDepth) {
      if(!importerConfiguration.setImportMaxDepth(importMaxDepth)) return 1;
   }
   if(importFilePath.string().size() > 0) {
      if(!importerConfiguration.setImportFilePath(importFilePath)) return 1;
   }
   if(goodFilePath.string().size() > 0) {
      if(!importerConfiguration.setGoodFilePath(goodFilePath)) return 1;
   }
   if(goodFilePath.string().size() > 0) {
      if(!importerConfiguration.setGoodFilePath(goodFilePath)) return 1;
   }
   if(badFilePath.string().size() > 0) {
      if(!importerConfiguration.setBadFilePath(badFilePath)) return 1;
   }
   if(importFilePathFilter.size() > 0) {
      if(!importerConfiguration.setImportPathFilter(importFilePathFilter)) return 1;
   }
   if( (importerConfiguration.getImportMode() == ImportModeType::KeepImportedFiles) &&
       (!quitWhenIdle) ) {
      std::cerr << "ERROR: Import mode \"KeepImportedFiles\" is only useful with --quit-when-idle option!\n";
      exit(1);
   }

   // ====== Read database configuration ====================================
   DatabaseConfiguration databaseConfiguration;
   if(!databaseConfiguration.readConfiguration(databaseConfigFile)) {
      exit(1);
   }

   // ====== Initialise importer ============================================
   HPCT_LOG(info) << "Startup:\n" << importerConfiguration << databaseConfiguration;
   initialiseLogger(logLevel, logColor,
                    (logFile != std::filesystem::path()) ? logFile.string().c_str() : nullptr);

   boost::asio::io_service ioService;
   UniversalImporter importer(ioService, importerConfiguration, databaseConfiguration);


   // ====== Initialise database clients and readers ========================
   // ------ HiPerConTracer Ping --------------------------
   DatabaseClientBase* pingDatabaseClients[pingWorkers];
   PingReader*         pingReader = nullptr;
   if(pingWorkers > 0) {
      for(unsigned int i = 0; i < pingWorkers; i++) {
         pingDatabaseClients[i] = databaseConfiguration.createClient();
         assert(pingDatabaseClients[i] != nullptr);
         if(!pingDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      pingReader = new PingReader(importerConfiguration,
                                  pingWorkers, pingTransactionSize,
                                  importerConfiguration.getTableName(PingReader::Identification,
                                                                     PingReader::Identification));
      assert(pingReader != nullptr);
      importer.addReader(*pingReader,
                         (DatabaseClientBase**)&pingDatabaseClients, pingWorkers);
   }

   // ------ HiPerConTracer Traceroute --------------------
   DatabaseClientBase* tracerouteDatabaseClients[tracerouteWorkers];
   TracerouteReader*   tracerouteReader = nullptr;
   if(tracerouteWorkers > 0) {
      for(unsigned int i = 0; i < tracerouteWorkers; i++) {
         tracerouteDatabaseClients[i] = databaseConfiguration.createClient();
         assert(tracerouteDatabaseClients[i] != nullptr);
         if(!tracerouteDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      tracerouteReader = new TracerouteReader(importerConfiguration,
                                              tracerouteWorkers, tracerouteTransactionSize,
                                              importerConfiguration.getTableName(TracerouteReader::Identification,
                                                                                 TracerouteReader::Identification));
      assert(tracerouteReader != nullptr);
      importer.addReader(*tracerouteReader,
                         (DatabaseClientBase**)&tracerouteDatabaseClients, tracerouteWorkers);
   }

#if 0
   // ------ HiPerConTracer Jitter ------------------------
   DatabaseClientBase* jitterDatabaseClients[jitterWorkers];
   JitterReader*   jitterReader = nullptr;
   if(jitterWorkers > 0) {
      for(unsigned int i = 0; i < jitterWorkers; i++) {
         jitterDatabaseClients[i] = databaseConfiguration.createClient();
         assert(jitterDatabaseClients[i] != nullptr);
         if(!jitterDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      jitterReader = new JitterReader(importerConfiguration,
                                      jitterWorkers, jitterTransactionSize,
                                      importerConfiguration.getTableName(JitterReader::Identification,
                                                                         JitterReader::Identification));
      assert(jitterReader != nullptr);
      importer.addReader(*jitterReader,
                         (DatabaseClientBase**)&jitterDatabaseClients, jitterWorkers);
   }
#endif


   // ====== Main loop ======================================================
   if(importer.start(quitWhenIdle) == false) {
      exit(1);
   }
   if(quitWhenIdle) {
     importer.waitForFinish();
   }
   ioService.run();
   importer.stop();


   // ====== Clean up =======================================================
#if 0
   if(jitterWorkers > 0) {
      delete jitterReader;
      jitterReader = nullptr;
      for(unsigned int i = 0; i < jitterWorkers; i++) {
         delete jitterDatabaseClients[i];
         jitterDatabaseClients[i] = nullptr;
      }
   }
#endif
   if(tracerouteWorkers > 0) {
      delete tracerouteReader;
      tracerouteReader = nullptr;
      for(unsigned int i = 0; i < tracerouteWorkers; i++) {
         delete tracerouteDatabaseClients[i];
         tracerouteDatabaseClients[i] = nullptr;
      }
   }
   if(pingWorkers > 0) {
      delete pingReader;
      pingReader = nullptr;
      for(unsigned int i = 0; i < pingWorkers; i++) {
         delete pingDatabaseClients[i];
         pingDatabaseClients[i] = nullptr;
      }
   }
   return 0;
}
