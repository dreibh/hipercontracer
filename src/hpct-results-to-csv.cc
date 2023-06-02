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
#include <map>
#include <set>

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/program_options.hpp>
#include <regex>


struct OutputEntry
{
   OutputEntry(const int                 measurementID,
               boost::asio::ip::address& source,
               boost::asio::ip::address& destination,
               unsigned long long        timeStamp,
               unsigned int              roundNumber,
               const std::string&        line) :
      MeasurementID(measurementID),
      Source(source),
      Destination(destination),
      TimeStamp(timeStamp),
      RoundNumber(roundNumber),
      SeqNumber(0),
      Line(line) { };

   const int                      MeasurementID;
   const boost::asio::ip::address Source;
   const boost::asio::ip::address Destination;
   const unsigned long long       TimeStamp;
   const unsigned int             RoundNumber;

   unsigned int                   SeqNumber;
   std::string                    Line;
};


enum InputType
{
   IT_Unknown    = 0,
   IT_Traceroute = 'T',
   IT_Ping       = 'P',
   IT_Jitter     = 'J'
};

enum InputProtocol
{
   IP_Unknown    = 0,
   IP_ICMP       = 'i',
   IP_UDP        = 'u',
   IP_TCP        = 't'
};

struct InputFormat
{
   InputType     Type;
   InputProtocol Protocol;
   unsigned int  Version;
   std::string   String;
};


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const OutputEntry& a, const OutputEntry& b)
{
   // ====== Level 1: MeasurementID ============================================
   if(a.MeasurementID < b.MeasurementID) {
      return true;
   }
   else if(a.MeasurementID == b.MeasurementID) {
      // ====== Level 2: Source =============================================
      if(a.Source < b.Source) {
         return true;
      }
      else if(a.Source == b.Source) {
         // ====== Level 3: Destination =====================================
         if(a.Destination < b.Destination) {
            return true;
         }
         else if(a.Destination == b.Destination) {
            // ====== Level 4: TimeStamp ====================================
            if(a.TimeStamp < b.TimeStamp) {
               return true;
            }
            else if(a.TimeStamp == b.TimeStamp) {
               // ====== Level 5: RoundNumber ===============================
               if(a.RoundNumber < b.RoundNumber) {
                  return true;
               }
               else if(a.RoundNumber == b.RoundNumber) {
                  // ====== Level 6: SeqNumber ==============================
                  if(a.SeqNumber < b.SeqNumber) {
                     return true;
                  }
               }
            }
         }
      }
   }
   return false;
}


template <typename T>
struct pointer_lessthan
{
   bool operator()(T* left, T* right) const {
      return (left && right) ? std::less<T>{}(*left, *right) : std::less<T*>{}(left, right);
   }
};


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
   return 1 + changes;   // Return number of columns
}


// ###### Check format of file ##############################################
void checkFormat(boost::iostreams::filtering_ostream& outputStream,
                 const std::filesystem::path&         fileName,
                 InputFormat&                         format,
                 unsigned int&                        columns,
                 const std::string&                   line,
                 const char                           separator)
{
   if(format.Type == InputType::IT_Unknown) {
      std::string columnNames;
      format.Type    = (InputType)line[1];
      format.Version = 0;
      format.String  = line.substr(0, 3);

      // ====== Ping ========================================================
      if(format.Type == InputType::IT_Ping) {
         // ------ Ping, Version 2 ------------------------------------------
         if(line[2] != ' ') {
            format.Protocol = (InputProtocol)line[2];
            format.Version  = 2;
            columnNames =
               "Ping "               // "#P<p>"
               "MeasurementID "      // Measurement ID
               "Source "             // Source address
               "Destination "        // Destination address
               "Timestamp "          // Timestamp (nanoseconds since the UTC epoch, hexadecimal).
               "BurstSeq "           // Sequence number within a burst (decimal), numbered from 0.
               "TrafficClass "       // Traffic Class setting (hexadeciaml)
               "PacketSize "         // Packet size, in bytes (decimal)
               "ResponseSize "       // Response size, in bytes (decimal)
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
         // ------ Ping, Version 1 ------------------------------------------
         else {
            format.Protocol = InputProtocol::IP_ICMP;
            format.Version  = 1;
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
      }

      // ====== Traceroute  =================================================
      else if(format.Type == InputType::IT_Traceroute) {
         // ------ Traceroute, Version 2 ------------------------------------
         if(line[2] != ' ') {
            format.Protocol = (InputProtocol)line[2];
            format.Version  = 2;
            columnNames =
               "Traceroute "         // "#T<p>"
               "MeasurementID "      // Measurement ID
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
               "ResponseSize "      // Response size, in bytes (decimal)
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
         // ------ Traceroute, Version 1 ------------------------------------
         else {
            format.Protocol = InputProtocol::IP_ICMP;
            format.Version  = 1;
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
      }

      // ====== Jitter ======================================================
      else if(format.Type == InputType::IT_Jitter) {
         format.Protocol = (InputProtocol)line[2];
         format.Version  = 2;
         abort(); // FIXME! TBD
      }

      // ====== Error =======================================================
      else {
         std::cerr << "ERROR: Unknown format " << format.String
                   << " in input file " << fileName << "!\n";
         exit(1);
      }

      columns = applySeparator(columnNames, separator);
      outputStream << columnNames << "\n";
   }
   else {
      if(format.String != line.substr(0, 3)) {
         std::cerr << "ERROR: Incompatible format for merging ("
                   << line.substr(0, 3) << " vs. " << format.String << ")"
                   << " in input file " << fileName << "!\n";
         exit(1);
      }
   }
}


// ###### Dump results file #################################################
bool dumpResultsFile(std::set<OutputEntry*, pointer_lessthan<OutputEntry>>&  outputSet,
                     boost::iostreams::filtering_ostream& outputStream,
                     const std::filesystem::path&         fileName,
                     InputFormat&                         format,
                     unsigned int&                        columns,
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
   std::string        line;
   unsigned long long lineNumber  = 0;
   OutputEntry*       newEntry    = nullptr;
   OutputEntry*       newSubEntry = nullptr;
   const std::regex re_space("\\s");   // space
   while(std::getline(inputStream, line, '\n')) {
      lineNumber++;

      // ====== #<line> =====================================================
      if(line[0] == '#') {
         checkFormat(outputStream, fileName, format, columns, line, separator);

         // ------ Obtain pointers to first N entres ------------------------
         const unsigned int maxColumns = 6;
         const char*  linestr = line.c_str();
         const char*  value[maxColumns];
         size_t       length[maxColumns];

         unsigned int i = 0;
         unsigned int c = 0;
         unsigned int l = 0;
         value[c]  = linestr;
         while(linestr[i] != 0x00) {
            if(linestr[i] == ' ') {
               length[c] = l;
               c++;
               if(c >= maxColumns) {
                  break;
               }
               value[c] = &linestr[i + 1];
               l = 0; i++;
               continue;
            }
            l++; i++;
         }
         length[c] = l;

         // ------ Create output entry --------------------------------------
         unsigned int             measurementID;
         size_t                   timeStampIndex;
         unsigned int             timeStamp;
         size_t                   roundNumberIndex;
         unsigned int             roundNumber;
         boost::asio::ip::address source;
         boost::asio::ip::address destination;
         if(format.Version == 2) {
            size_t measurementIDIndex;
            measurementID = std::stoul(value[1], &measurementIDIndex, 10);
            if(measurementIDIndex != length[1]) {
               throw std::exception();
            }
            source      = boost::asio::ip::make_address(std::string(value[2], length[2]));
            destination = boost::asio::ip::make_address(std::string(value[3], length[3]));
            timeStamp   = std::stoull(value[4], &timeStampIndex, 16);
            if(timeStampIndex != length[4]) {
               throw std::exception();
            }
            if(format.Type == InputType::IT_Traceroute) {
               roundNumber = std::stoul(value[5], &roundNumberIndex, 10);
               if(roundNumberIndex != length[5]) {
                  throw std::exception();
               }
            }
            else {
               roundNumber = 0;
            }
         }
         else {
            source      = boost::asio::ip::make_address(std::string(value[1], length[1]));
            destination = boost::asio::ip::make_address(std::string(value[2], length[2]));
            timeStamp   = std::stoull(value[3], &timeStampIndex, 16);
            if(timeStampIndex != length[3]) {
               throw std::exception();
            }
            if(format.Type == InputType::IT_Traceroute) {
               roundNumber = std::stoul(value[4], &roundNumberIndex, 10);
               if(roundNumberIndex != length[4]) {
                  throw std::exception();
               }
            }
            else {
               roundNumber = 0;
            }
         }
         if(newEntry != nullptr) {
            delete newEntry;
         }
         newEntry = new OutputEntry(measurementID, source, destination, timeStamp,
                                    roundNumber, line);

         // ====== Write entry, if not Traceroute ===========================
         if(format.Type != InputType::IT_Traceroute) {
            const unsigned int currentColumns = applySeparator(line, separator);
            if(currentColumns != columns) {
               std::cerr << "ERROR: Got " << currentColumns << " instead of expected " << columns
                         << " in input file " << fileName << ", line " << lineNumber << "!\n";
               exit(1);
            }

            auto success = outputSet.insert(newEntry);
            if(!success.second) {
               std::cerr << "ERROR: Duplicate entry"
                        << " in input file " << fileName << ", line " << lineNumber << "!\n";
               exit(1);
            }
         }

         // ====== Remember header, if Traceroute ===========================
         else {
            // This header is combined with TAB lines later!
            newSubEntry = newEntry;
         }
      }

      // ====== TAB<line> ===================================================
      else if(line[0] == '\t') {
         if(newSubEntry == nullptr) {
            std::cerr << "ERROR: TAB line without corresponding header line"
                      << " in input file " << fileName << ", line " << lineNumber << "!\n";
            exit(1);
         }
         newSubEntry = new OutputEntry(*newSubEntry);
         newSubEntry->SeqNumber++;
         newSubEntry->Line += " ~ " + ((line[1] == ' ') ? line.substr(2) : line.substr(1));

         auto success = outputSet.insert(newSubEntry);
         if(!success.second) {
            std::cerr << "ERROR: Duplicate entry"
                      << " in input file " << fileName << ", line " << lineNumber << "!\n";
            exit(1);
         }
      }

      // ------ Syntax error ------------------------------------------------
      else {
         std::cerr << "ERROR: Unexpected syntax"
                     << " in input file " << fileName << ", line " << lineNumber << "!\n";
         exit(1);
      }
   }

   if(newEntry != nullptr) {
      delete newEntry;
   }
   return true;
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   std::vector<std::filesystem::path> inputFileNameList;
   std::filesystem::path              outputFileName;
   InputFormat                        format { InputType::IT_Unknown };
   unsigned int                       columns = 0;
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
   std::set<std::filesystem::path>    inputFileNameSet(inputFileNameList.begin(),
                                                       inputFileNameList.end());
   std::set<OutputEntry*, pointer_lessthan<OutputEntry>> outputSet;
   for(const std::filesystem::path& inputFileName : inputFileNameSet) {
      dumpResultsFile(outputSet, outputStream, inputFileName, format, columns, separator);
   }
   for(std::set<OutputEntry*, pointer_lessthan<OutputEntry>>::const_iterator iterator = outputSet.begin();
       iterator != outputSet.end(); iterator++) {
      outputStream << (*iterator)->Line << "\n";
      delete *iterator;
   }

   return 0;
}
