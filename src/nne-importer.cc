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

#include "logger.h"
#include "tools.h"

#include "reader-nne-ping.h"
#include "reader-nne-speedtest.h"
#include "reader-nne-metadata.h"

#include "universal-importer.h"


#include <filesystem>
#include <iostream>

#include <boost/program_options.hpp>



// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   unsigned int          logLevel;
   std::filesystem::path databaseConfigurationFile;
   std::string           importModeName;
   unsigned int          importMaxDepth;
   std::filesystem::path importFilePath;
   std::filesystem::path badFilePath;
   std::filesystem::path goodFilePath;
   std::string           importFilePathFilter;
   bool                  quitWhenIdle;
   unsigned int          pingWorkers;
   unsigned int          speedTestWorkers;
   unsigned int          metadataWorkers;
   unsigned int          pingTransactionSize;
   unsigned int          speedTestTransactionSize;
   unsigned int          metadataTransactionSize;

   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )

      ( "loglevel,L",
           boost::program_options::value<unsigned int>(&logLevel)->default_value(boost::log::trivial::severity_level::info),
           "Set logging level" )
      ( "verbose,v",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::trace),
           "Verbose logging level" )
      ( "quiet,q",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::warning),
           "Quiet logging level" )

      ( "config,C",
           boost::program_options::value<std::filesystem::path>(&databaseConfigurationFile),
           "Database configuration file" )
      ("import-mode,X",              boost::program_options::value<std::string>(&importModeName),                    "Override import mode")
      ("import-max-depth,D",         boost::program_options::value<unsigned int>(&importMaxDepth)->default_value(0), "Override import max depth")
      ("import-file-path,I",         boost::program_options::value<std::filesystem::path>(&importFilePath),          "Override path for input files")
      ("bad-file-path,B",            boost::program_options::value<std::filesystem::path>(&badFilePath),             "Override path for bad files")
      ("good-file-path,G",           boost::program_options::value<std::filesystem::path>(&goodFilePath),            "Override path for good files")
      ("import-file-path-filter,F",  boost::program_options::value<std::string>(&importFilePathFilter),              "Import path filter (regular expression)")
      ("quit-when-idle,Q",
          boost::program_options::value<bool>(&quitWhenIdle)->implicit_value(true)->default_value(false),
          "Quit importer when idle")

      ( "ping-workers,P",
           boost::program_options::value<unsigned int>(&pingWorkers)->default_value(4),
           "Number of Ping import worker threads" )
      ( "ping-files,p",
           boost::program_options::value<unsigned int>(&pingTransactionSize)->default_value(512),
           "Number of Ping files per transaction" )
      ( "speedtest-workers,S",
           boost::program_options::value<unsigned int>(&speedTestWorkers)->default_value(1),
           "Number of SpeedTest import worker threads" )
      ( "speedtest-files,s",
           boost::program_options::value<unsigned int>(&speedTestTransactionSize)->default_value(1),
           "Number of SpeedTest files per transaction" )
      ( "metadata-workers,M",
           boost::program_options::value<unsigned int>(&metadataWorkers)->default_value(4),
           "Number of Metadata import worker threads" )
      ( "metadata-files,m",
           boost::program_options::value<unsigned int>(&metadataTransactionSize)->default_value(4096),
           "Number of Metadata files per transaction" )
   ;


   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      const boost::program_options::parsed_options parsedOptions =
         boost::program_options::command_line_parser(argc, argv).
            style(boost::program_options::command_line_style::style_t::default_style| boost::program_options::command_line_style::style_t::allow_long_disguise).
            options(commandLineOptions).
            // allow_unregistered().
            run();
      boost::program_options::store(parsedOptions, vm);

      // std::vector<std::string> p = boost::program_options::collect_unrecognized(parsedOptions.options, boost::program_options::include_positional);
      // if(p.size() > 1) {
      //    std::cerr << "ERROR: Only one database configuration file may be provided!\n";
      //    exit(1);
      // }
      // for(auto it = p.begin(); it != p.end(); it++) {
      //     databaseConfigurationFile = *it;
      //     break;
      // }
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

   if(pingWorkers + speedTestWorkers + metadataWorkers < 1) {
      std::cerr << "ERROR: At least one worker is needed!" << "\n";
      return 1;
   }


   // ====== Read database configuration ====================================
   DatabaseConfiguration databaseConfiguration;
   if(!databaseConfiguration.readConfiguration(databaseConfigurationFile)) {
      exit(1);
   }
   if(importModeName.size() > 0) {
      if(!databaseConfiguration.setImportMode(importModeName)) return 1;
   }
   if(importMaxDepth) {
      if(!databaseConfiguration.setImportMaxDepth(importMaxDepth)) return 1;
   }
   if(importFilePath.string().size() > 0) {
      if(!databaseConfiguration.setImportFilePath(importFilePath)) return 1;
   }
   if(goodFilePath.string().size() > 0) {
      if(!databaseConfiguration.setGoodFilePath(goodFilePath)) return 1;
   }
   if(goodFilePath.string().size() > 0) {
      if(!databaseConfiguration.setGoodFilePath(goodFilePath)) return 1;
   }
   if(badFilePath.string().size() > 0) {
      if(!databaseConfiguration.setBadFilePath(badFilePath)) return 1;
   }
   HPCT_LOG(info) << "Startup:\n" << databaseConfiguration;


   // ====== Initialise importer ============================================
   initialiseLogger(logLevel);

   boost::asio::io_service ioService;
   UniversalImporter importer(ioService, databaseConfiguration);


   // ====== Initialise database clients and readers ========================
   // ------ NorNet Edge Ping -----------------------------
   DatabaseClientBase*   pingDatabaseClients[pingWorkers];
   NorNetEdgePingReader* nnePingReader = nullptr;
   if(pingWorkers > 0) {
      for(unsigned int i = 0; i < pingWorkers; i++) {
         pingDatabaseClients[i] = databaseConfiguration.createClient();
         assert(pingDatabaseClients[i] != nullptr);
         if(!pingDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      nnePingReader = new NorNetEdgePingReader(databaseConfiguration,
                                               pingWorkers, pingTransactionSize);
      assert(nnePingReader != nullptr);
      importer.addReader(*nnePingReader,
                         (DatabaseClientBase**)&pingDatabaseClients, pingWorkers);
   }

   // ------ NorNet Edge SpeedTest ------------------------
   DatabaseClientBase*        speedTestDatabaseClients[speedTestWorkers];
   NorNetEdgeSpeedTestReader* nneSpeedTestReader = nullptr;
   if(speedTestWorkers > 0) {
      for(unsigned int i = 0; i < speedTestWorkers; i++) {
         speedTestDatabaseClients[i] = databaseConfiguration.createClient();
         assert(speedTestDatabaseClients[i] != nullptr);
         if(!speedTestDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      nneSpeedTestReader = new NorNetEdgeSpeedTestReader(databaseConfiguration,
                                                         speedTestWorkers, speedTestTransactionSize);
      assert(nneSpeedTestReader != nullptr);
      importer.addReader(*nneSpeedTestReader,
                         (DatabaseClientBase**)&speedTestDatabaseClients, speedTestWorkers);
   }

   // ------ NorNet Edge Metadata -------------------------
   DatabaseClientBase*       metadataDatabaseClients[metadataWorkers];
   NorNetEdgeMetadataReader* nneMetadataReader = nullptr;
   if(metadataWorkers > 0) {
      for(unsigned int i = 0; i < metadataWorkers; i++) {
         metadataDatabaseClients[i] = databaseConfiguration.createClient();
         assert(metadataDatabaseClients[i] != nullptr);
         if(!metadataDatabaseClients[i]->open()) {
            exit(1);
         }
      }
      nneMetadataReader = new NorNetEdgeMetadataReader(databaseConfiguration,
                                                       metadataWorkers, metadataTransactionSize);
      assert(nneMetadataReader != nullptr);
      importer.addReader(*nneMetadataReader,
                         (DatabaseClientBase**)&metadataDatabaseClients, metadataWorkers);
   }


   // ====== Main loop ======================================================
   if(importer.start(importFilePathFilter, quitWhenIdle) == false) {
      exit(1);
   }
   ioService.run();
   importer.stop();


   // ====== Clean up =======================================================
   if(metadataWorkers > 0) {
      delete nneMetadataReader;
      nneMetadataReader = nullptr;
      for(unsigned int i = 0; i < metadataWorkers; i++) {
         delete metadataDatabaseClients[i];
         metadataDatabaseClients[i] = nullptr;
      }
   }
   if(speedTestWorkers > 0) {
      delete nneSpeedTestReader;
      nneSpeedTestReader = nullptr;
      for(unsigned int i = 0; i < speedTestWorkers; i++) {
         delete speedTestDatabaseClients[i];
         speedTestDatabaseClients[i] = nullptr;
      }
   }
   if(pingWorkers > 0) {
      delete nnePingReader;
      nnePingReader = nullptr;
      for(unsigned int i = 0; i < pingWorkers; i++) {
         delete pingDatabaseClients[i];
         pingDatabaseClients[i] = nullptr;
      }
   }
   return 0;
}
