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

#include "database-configuration.h"
#include "databaseclient-base.h"
#include "logger.h"
#include "tools.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/program_options.hpp>


typedef std::chrono::high_resolution_clock ResultClock;
typedef ResultClock::time_point            ResultTimePoint;
typedef ResultClock::duration              ResultDuration;


// ###### Convert IPv4-mapped IPv6 address to IPv4 address ##################
inline static boost::asio::ip::address unmapIPv4(const boost::asio::ip::address& address) {
   if(address.is_v6()) {
      const boost::asio::ip::address_v6 v6 = address.to_v6();
      if(v6.is_v4_mapped()) {
         return boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, v6);
      }
   }
   return address;
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================
   unsigned int          logLevel;
   bool                  logColor;
   std::filesystem::path logFile;
   std::filesystem::path databaseConfigurationFile;
   std::filesystem::path outputFileName;
   std::string           queryType;
   ResultTimePoint       fromTimeStamp;
   ResultTimePoint       toTimeStamp;

//    ::time_point
//    template <typename TimePoint> bool stringToTimePoint(
//                                       const std::string& string,
//                                       TimePoint&         timePoint,
//                                       const char*        format = "%Y-%m-%d %H:%M:%S")
//
// fromTimeStamp     = None
// toTimeStamp       = None
// if len(sys.argv) > 3:
//    fromTimeStamp = datetime.datetime.strptime(sys.argv[3], "%Y-%m-%d %H:%M:%S")
//    if len(sys.argv) > 4:
//       toTimeStamp = datetime.datetime.strptime(sys.argv[4], "%Y-%m-%d %H:%M:%S")

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

      ( "config,C",
           boost::program_options::value<std::filesystem::path>(&databaseConfigurationFile),
           "Database configuration file" )

      ( "output,o",
           boost::program_options::value<std::filesystem::path>(&outputFileName)->default_value(std::filesystem::path()),
           "Output file" )

      ( "query-type,Q",
           boost::program_options::value<std::string>(&queryType)->default_value("Ping"),
           "Query type (Ping, Traceroute, Jitter)" )
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
   boost::algorithm::to_lower(queryType);


   // ====== Initialise importer ============================================
   initialiseLogger(logLevel, logColor,
                    (logFile != std::filesystem::path()) ? logFile.string().c_str() : nullptr);

   // ====== Read database configuration ====================================
   DatabaseConfiguration databaseConfiguration;
   if(!databaseConfiguration.readConfiguration(databaseConfigurationFile)) {
      exit(1);
   }
   HPCT_LOG(info) << "Startup:\n" << databaseConfiguration;

   // ====== Initialise database client =====================================
   DatabaseClientBase* databaseClient = databaseConfiguration.createClient();
   assert(databaseClient != nullptr);
   if(!databaseClient->open()) {
      exit(1);
   }

   // ====== Open output file ===============================================
   std::string                         extension(outputFileName.extension());
   std::ofstream                       outputFile;
   boost::iostreams::filtering_ostream outputStream;
   if(outputFileName != std::filesystem::path()) {
      outputFile.open(outputFileName, std::ios_base::out | std::ios_base::binary);
      if(!outputFile.is_open()) {
         HPCT_LOG(fatal) << "Failed to create output file " << outputFileName;
         exit(1);
      }
      boost::algorithm::to_lower(extension);
      if(extension == ".xz") {
         const boost::iostreams::lzma_params params(
            boost::iostreams::lzma::default_compression,
            std::thread::hardware_concurrency());
         outputStream.push(boost::iostreams::lzma_compressor(params));
      }
      else if(extension == ".bz2") {
         outputStream.push(boost::iostreams::bzip2_compressor());
      }
      else if(extension == ".gz") {
         outputStream.push(boost::iostreams::gzip_compressor());
      }
      outputStream.push(outputFile);
   }
   else {
      outputStream.push(std::cout);
   }


   // ====== Prepare query ==================================================
   const DatabaseBackendType backend = databaseClient->getBackend();
   Statement& statement              = databaseClient->getStatement(queryType, false, true);


   // ====== Ping ===========================================================
   if(queryType == "ping") {
      if(backend & DatabaseBackendType::SQL_Generic) {
         statement
            << "SELECT SendTimestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,BurstSeq,PacketSize,ResponseSize,Checksum,Status,TimeSource,Delay_AppSend,Delay_Queuing, Delay_AppReceive,RTT_App,RTT_SW,RTT_HW"
               " FROM Ping"
               " ORDER BY SendTimestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass";
         databaseClient->executeQuery(statement);
         while(databaseClient->fetchNextTuple()) {
            const unsigned long long       sendTimeStamp   = databaseClient->getBigInt(1);
            const unsigned long long       measurementID   = databaseClient->getBigInt(2);
            const boost::asio::ip::address sourceIP        = unmapIPv4(boost::asio::ip::make_address(databaseClient->getString(3)));
            const boost::asio::ip::address destinationIP   = unmapIPv4(boost::asio::ip::make_address(databaseClient->getString(4)));
            const char                     protocol        = databaseClient->getInteger(5);
            const uint8_t                  trafficClass    = databaseClient->getInteger(6);
            const unsigned int             burstSeq        = databaseClient->getInteger(7);
            const unsigned int             packetSize      = databaseClient->getInteger(8);
            const unsigned int             responseSize    = databaseClient->getInteger(9);
            const uint16_t                 checksum        = databaseClient->getInteger(10);
            const unsigned int             status          = databaseClient->getInteger(11);
            const unsigned int             timeSource      = databaseClient->getInteger(12);
            const long long                delayAppSend    = databaseClient->getBigInt(13);
            const long long                delayQueuing    = databaseClient->getBigInt(14);
            const long long                delayAppReceive = databaseClient->getBigInt(15);
            const long long                rttApplication  = databaseClient->getBigInt(16);
            const long long                rttSoftware     = databaseClient->getBigInt(17);
            const long long                rttHardware     = databaseClient->getBigInt(18);

            outputStream <<
               str(boost::format("#P%c %d %s %s %x %d %x %d %d %x %d %08x %d %d %d %d %d %d\n")
                  % protocol

                  % measurementID
                  % sourceIP.to_string()
                  % destinationIP.to_string()
                  % sendTimeStamp
                  % burstSeq

                  % (unsigned int)trafficClass
                  % packetSize
                  % responseSize
                  % checksum
                  % status

                  % timeSource
                  % delayAppSend
                  % delayQueuing
                  % delayAppReceive
                  % rttApplication
                  % rttSoftware
                  % rttHardware
               );
         }
      }
      else if(backend & DatabaseBackendType::NoSQL_Generic) {

      }
      else {
         HPCT_LOG(fatal) << "Unknown backend";
         abort();
      }
   }


   // ====== Traceroute =====================================================
   else if(queryType == "traceroute") {
      if(backend & DatabaseBackendType::SQL_Generic) {
         statement
            << "SELECT Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,HopNumber,TotalHops,PacketSize,ResponseSize,Checksum,Status,PathHash,SendTimestamp,HopIP,TimeSource,Delay_AppSend,Delay_Queuing,Delay_AppReceive,RTT_App,RTT_SW,RTT_HW"
               " FROM Traceroute"
               " ORDER BY Timestamp,MeasurementID,SourceIP,DestinationIP,Protocol,TrafficClass,RoundNumber,HopNumber";
         databaseClient->executeQuery(statement);
         while(databaseClient->fetchNextTuple()) {
            const unsigned long long       timeStamp       = databaseClient->getBigInt(1);
            const unsigned long long       measurementID   = databaseClient->getBigInt(2);
            const boost::asio::ip::address sourceIP        = unmapIPv4(boost::asio::ip::make_address(databaseClient->getString(3)));
            const boost::asio::ip::address destinationIP   = unmapIPv4(boost::asio::ip::make_address(databaseClient->getString(4)));
            const char                     protocol        = databaseClient->getInteger(5);
            const uint8_t                  trafficClass    = databaseClient->getInteger(6);
            const unsigned int             roundNumber     = databaseClient->getInteger(7);
            const unsigned int             hopNumber       = databaseClient->getInteger(8);
            const unsigned int             totalHops       = databaseClient->getInteger(9);
            const unsigned int             packetSize      = databaseClient->getInteger(10);
            const unsigned int             responseSize    = databaseClient->getInteger(11);
            const uint16_t                 checksum        = databaseClient->getInteger(12);
            const unsigned int             status          = databaseClient->getInteger(13);
            const long long                pathHash        = databaseClient->getBigInt(14);
            const unsigned long long       sendTimeStamp   = databaseClient->getBigInt(15);
            const boost::asio::ip::address hopIP           = unmapIPv4(boost::asio::ip::make_address(databaseClient->getString(16)));
            const unsigned int             timeSource      = databaseClient->getInteger(17);
            const long long                delayAppSend    = databaseClient->getBigInt(18);
            const long long                delayQueuing    = databaseClient->getBigInt(19);
            const long long                delayAppReceive = databaseClient->getBigInt(20);
            const long long                rttApplication  = databaseClient->getBigInt(21);
            const long long                rttSoftware     = databaseClient->getBigInt(22);
            const long long                rttHardware     = databaseClient->getBigInt(23);

            if(hopNumber == 1) {
               const unsigned int statusFlags = status - (status & 0xff);
               outputStream <<
                  str(boost::format("#T%c %d %s %s %x %d %d %x %d %x %x %x\n")
                     % protocol

                     % measurementID
                     % sourceIP.to_string()
                     % destinationIP.to_string()
                     % timeStamp
                     % roundNumber

                     % totalHops

                     % (unsigned int)trafficClass
                     % packetSize
                     % checksum
                     % statusFlags

                     % pathHash
                  );
            }
            else {
               outputStream <<
                  str(boost::format("\t%x %d %d %d %08x %d %d %d %d %d %d %s\n")
                     % sendTimeStamp
                     % hopNumber
                     % responseSize
                     % (unsigned int)(status & 0xff)

                     % timeSource
                     % delayAppSend
                     % delayQueuing
                     % delayAppReceive
                     % rttApplication
                     % rttSoftware
                     % rttHardware

                     % hopIP.to_string()
                  );
            }
         }
      }
      else if(backend & DatabaseBackendType::NoSQL_Generic) {

      }
      else {
         HPCT_LOG(fatal) << "Unknown backend";
         abort();
      }
   }


   // ====== Jitter =========================================================
   else if(queryType == "jitter") {
       abort();   // FIXME! TBD
   }


   // ====== Traceroute =====================================================
   else {
      HPCT_LOG(fatal) << "Invalid query type " << queryType;
      exit(1);
   }


   // ====== Clean up =======================================================
   delete databaseClient;
   databaseClient = nullptr;
   return 0;
}
