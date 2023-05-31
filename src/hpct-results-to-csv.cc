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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/program_options.hpp>



// ###### Replace space by given separator character ########################
unsigned int applySeparator(std::string& string, const char separator)
{
   unsigned int changes = 0;
   for(char& c : string) {
      if(c == ' ') {
         c = separator;
         changes++;
      }
   }
   return changes;
}


// ###### Dump results file #################################################
bool dumpResultsFile(boost::iostreams::filtering_ostream& outputStream,
                     const std::filesystem::path&         fileName,
                     std::string&                         format,
                     unsigned long long&                  columns,
                     const char                           separator)
{
   // ====== Open input file ================================================
   std::string                         extension(fileName.extension());
   std::ifstream                       inputFile;
   boost::iostreams::filtering_istream inputStream;

   inputFile.open(fileName, std::ios_base::in | std::ios_base::binary);
   if(!inputFile.is_open()) {
      std::cerr << "ERROR: Failed to read input file " << fileName << "!\n";
      exit(1);
   }
   boost::algorithm::to_lower(extension);
   if(extension == ".xz") {
      inputStream.push(boost::iostreams::lzma_decompressor());
   }
   else if(extension == ".bz2") {
      inputStream.push(boost::iostreams::bzip2_decompressor());
   }
   else if(extension == ".gz") {
      inputStream.push(boost::iostreams::gzip_decompressor());
   }
   inputStream.push(inputFile);

   // ====== Process lines of the input file ================================
   std::string line;
   std::string header;
   while(std::getline(inputStream, line, '\n')) {
      // ------ #<line> -----------------------------------------------------
      if(line[0] == '#') {
         if(format.size() == 0) {
            std::string columnNames;
            format = line.substr(0, 3);
            // ------ Ping --------------------------------------------------
            if(format[1] == 'P') {
               // ------ Ping, Version 1 ------------------------------------
               if(format[2] == ' ') {
                  columnNames =
                     "Ping "               // "#P"
                     "Source "             // Source address
                     "Destination "        // Destination address
                     "Timestamp "          // Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
                     "Checksum "           // Checksum (hexadeciaml)
                     "Status "             // Status (decimal)
                     "RTT.App "            // RTT in microseconds (decimal)
                     "TrafficClass "       // Traffic Class setting (hexadeciaml)
                     "PacketSize";         // Packet size, in bytes (decimal)
               }
               // ------ Ping, Version 2 ------------------------------------
               else {
                  columnNames =
                     "Ping "               // "#P<p>"
                     "Source "             // Source address
                     "Destination "        // Destination address
                     "Timestamp "          // Timestamp (nanoseconds since the UTC epoch, hexadecimal).
                     "BurstSeq "           // Sequence number within a burst (decimal), numbered from 0.
                     "TrafficClass "       // Traffic Class setting (hexadeciaml)
                     "PacketSize "         // Packet size, in bytes (decimal)
                     "Checksum "           // Checksum (hexadeciaml)
                     "Status "             // Status (decimal)
                     "TimeSource "         // Source of the timing information (hexadecimal) as: AAQQSSHH
                     "Delay.AppSend "      // The measured application send delay (nanoseconds, decimal; -1 if not available).
                     "Delay.Queuing "      // The measured kernel software queuing delay (nanoseconds, decimal; -1 if not available).
                     "Delay.AppReceive "   // The measured application receive delay (nanoseconds, decimal; -1 if not available).
                     "RTT.App "            // The measured application RTT (nanoseconds, decimal).
                     "RTT.SW "             // The measured kernel software RTT (nanoseconds, decimal; -1 if not available).
                     "RTT.HW";             // The measured kernel hardware RTT (nanoseconds, decimal; -1 if not available).
               }
            }
            // ------ Traceroute --------------------------------------------
            else if(format[1] == 'T') {
               // ------ Traceroute, Version 1 ------------------------------
               if(format[2] == ' ') {
                  columnNames =
                     "Traceroute "        // "#T"
                     "Source "            // Source address
                     "Destination "       // Destination address
                     "Timestamp "         // Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
                     "Round "             // Round number (decimal)
                     "Checksum "          // Checksum (hexadeciaml)
                     "TotalHops "         // Total hops (decimal)
                     "StatusFlags "       // Status flags (hexadecimal)
                     "PathHash "          // Hash of the path (hexadecimal)
                     "TrafficClass "      // Traffic Class setting (hexadeciaml)
                     "PacketSize "        // Packet size, in bytes (decimal)

                     "TAB "               // NOTE: must be "\t" from combination above!
                     "HopNumber "         // Number of the hop.
                     "Status "            // Status code (in hexadecimal here!)
                     "RTT.App "           // RTT in microseconds (decimal)
                     "LinkDestination";   // Hop IP address.
               }
               // ------ Traceroute, Version 2 ------------------------------
               else {
                  columnNames =
                     "Traceroute "         // "#T<p>"
                     "Source "             // Source address
                     "Destination "        // Destination address
                     "Timestamp "          // Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
                     "Round "              // Round number (decimal)
                     "TotalHops "          // Total hops (decimal)
                     "TrafficClass "       // Traffic Class setting (hexadeciaml)
                     "PacketSize "         // Packet size, in bytes (decimal)
                     "Checksum "           // Checksum (hexadeciaml)
                     "StatusFlags "        // Status flags (hexadecimal)
                     "PathHash "           // Hash of the path (hexadecimal)

                     "TAB "                // NOTE: must be "\t" from combination above!
                     "HopNumber "          // Number of the hop.
                     "Status "             // Status code (decimal!)
                     "TimeSource "         // Source of the timing information (hexadecimal) as: AAQQSSHH
                     "Delay.AppSend "      // The measured application send delay (nanoseconds, decimal; -1 if not available).
                     "Delay.Queuing "      // The measured kernel software queuing delay (nanoseconds, decimal; -1 if not available).
                     "Delay.AppReceive "   // The measured application receive delay (nanoseconds, decimal; -1 if not available).
                     "RTT.App "            // The measured application RTT (nanoseconds, decimal).
                     "RTT.SW "             // The measured kernel software RTT (nanoseconds, decimal; -1 if not available).
                     "RTT.HW "             // The measured kernel hardware RTT (nanoseconds, decimal; -1 if not available).
                     "LinkDestination";    // Hop IP address.
               }
            }
            // ------ Jitter ------------------------------------------------
            else if(format[1] == 'J') {
               abort(); // FIXME! TBD
            }
            // ------ Error -------------------------------------------------
            else {
               std::cerr << "ERROR: Unknown format " << format << " in input file " << fileName << "!\n";
               exit(1);
            }

            columns = applySeparator(columnNames, separator);
            outputStream << columnNames << "\n";
         }
         else {
            if(format != line.substr(0, 3)) {
               std::cerr << "ERROR: Different format in input file " << fileName << "!\n"
                         << "Expected: " << format << ", Read: " << line.substr(0, 3) << "\n";
               exit(1);
            }
         }

         if(format[1] != 'T') {
            if(applySeparator(line, separator) != columns) {
               std::cerr << "ERROR: Different number of columns than expected " << columns << "!\n";
            }
            outputStream << line << "\n";
         }
         else {
            header = line;
         }
      }

      // ------ TAB<line> ---------------------------------------------------
      else if(line[0] == '\t') {
         if(header.size() == 0) {
            std::cerr << "ERROR: Missing header for TAB line in input file " << fileName << "!\n";
            exit(1);
         }
         std::stringstream ss;
         ss << header << " TAB "
            << ((line[1] == ' ') ? line.substr(2) : line.substr(1));
         line = ss.str();
         if(applySeparator(line, separator) != columns) {
            std::cerr << "ERROR: Different number of columns than expected " << columns << "!\n";
         }
         outputStream << line << "\n";
      }

      // ------ Syntax error ------------------------------------------------
      else {
         std::cerr << "ERROR: Unexpected syntax in input file " << fileName << "!\n";
         std::cerr << line << "\n";
         exit(1);
      }
   }

   return true;
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   std::vector<std::filesystem::path> inputFileNameList;
   std::filesystem::path              outputFileName;
   std::string                        format;
   unsigned long long                 columns = 0;
   char                               separator;

   // ====== Initialize =====================================================
   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )

      ( "output,o",
           boost::program_options::value<std::filesystem::path>(&outputFileName)->default_value(std::filesystem::path()),
           "Output file" )
      ( "separator,s",
           boost::program_options::value<char>(&separator)->default_value(' '),
           "Separator character" )
   ;
   boost::program_options::options_description hiddenOptions;
   hiddenOptions.add_options()
      ("input,i", boost::program_options::value<std::vector<std::filesystem::path>>(&inputFileNameList))
   ;
   boost::program_options::options_description allOptions;
   allOptions.add(commandLineOptions);
   allOptions.add(hiddenOptions);
   boost::program_options::positional_options_description positionalParameters;
   positionalParameters.add("input", -1);

   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
                                       style(
                                          boost::program_options::command_line_style::style_t::default_style|
                                          boost::program_options::command_line_style::style_t::allow_long_disguise
                                       ).
                                       options(allOptions).positional(positionalParameters).
                                       run(), vm);
      boost::program_options::notify(vm);
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: Bad parameter: " << e.what() << "\n";
      return 1;
   }
   if( (separator != ' ')  &&
       (separator != '\t') &&
       (separator != ',')  &&
       (separator != ':')  &&
       (separator != ';')  &&
       (separator != '|') ) {
      std::cerr << "ERROR: Invalid separator \"" << separator << "\"!\n";
      exit(1);
   }
   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << "\n"
                 << commandLineOptions;
       return 1;
   }


   // ====== Open output file ===============================================
   std::string                         extension(outputFileName.extension());
   std::ofstream                       outputFile;
   boost::iostreams::filtering_ostream outputStream;
   if(outputFileName != std::filesystem::path()) {
      outputFile.open(outputFileName, std::ios_base::out | std::ios_base::binary);
      if(!outputFile.is_open()) {
         std::cerr << "ERROR: Failed to create output file " << outputFileName << "!\n";
         exit(1);
      }
      boost::algorithm::to_lower(extension);
      if(extension == ".xz") {
         outputStream.push(boost::iostreams::lzma_compressor());
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

   // ====== Dump input files ===============================================
   std::set<std::filesystem::path> inputFileNameSet(inputFileNameList.begin(),
                                                    inputFileNameList.end());
   for(const std::filesystem::path& inputFileName : inputFileNameSet) {
      dumpResultsFile(outputStream, inputFileName, format, columns, separator);
   }

   return 0;
}
