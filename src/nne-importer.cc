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
           boost::program_options::value<std::filesystem::path>(&databaseConfigurationFile)->default_value(std::filesystem::path("database.conf")),
           "Database configuration file" )

      ( "ping-workers",
           boost::program_options::value<unsigned int>(&pingWorkers)->default_value(1),
           "Number of Ping import worker threads" )
      ( "ping-files",
           boost::program_options::value<unsigned int>(&pingTransactionSize)->default_value(4),
           "Number of Ping files per transaction" )
      ( "speedtest-workers",
           boost::program_options::value<unsigned int>(&speedTestWorkers)->default_value(1),
           "Number of SpeedTest import worker threads" )
      ( "speedtest-files",
           boost::program_options::value<unsigned int>(&speedTestTransactionSize)->default_value(1),
           "Number of SpeedTest files per transaction" )
      ( "metadata-workers",
           boost::program_options::value<unsigned int>(&metadataWorkers)->default_value(1),
           "Number of Metadata import worker threads" )
      ( "metadata-files",
           boost::program_options::value<unsigned int>(&metadataTransactionSize)->default_value(256),
           "Number of Metadata files per transaction" )
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
      std::cerr << "ERROR: Bad parameter: " << e.what() << std::endl;
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << std::endl
                 << commandLineOptions;
       return 1;
   }

   std::cout << pingWorkers << " " << speedTestWorkers << " " << metadataWorkers << std::endl;


   // ====== Read database configuration ====================================
   DatabaseConfiguration databaseConfiguration;
   if(!databaseConfiguration.readConfiguration(databaseConfigurationFile)) {
      exit(1);
   }

// ????
// databaseConfiguration.setBackend(DatabaseBackendType::SQL_Debug);

   databaseConfiguration.printConfiguration(std::cout);


   // ====== Initialise importer ============================================
   initialiseLogger(logLevel);

   boost::asio::io_service ioService;
   UniversalImporter importer(ioService,
                              databaseConfiguration.getImportFilePath(),
                              databaseConfiguration.getGoodFilePath(),
                              databaseConfiguration.getBadFilePath(),
                              databaseConfiguration.getImportMode(),
                              5);


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
      nnePingReader = new NorNetEdgePingReader(pingWorkers, pingTransactionSize);
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
      nneSpeedTestReader = new NorNetEdgeSpeedTestReader(speedTestWorkers, speedTestTransactionSize);
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
      nneMetadataReader = new NorNetEdgeMetadataReader(metadataWorkers, metadataTransactionSize);
      assert(nneMetadataReader != nullptr);
      importer.addReader(*nneMetadataReader,
                         (DatabaseClientBase**)&metadataDatabaseClients, metadataWorkers);
   }


   // ====== Main loop ======================================================
   if(importer.start() == false) {
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