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
#include "conversions.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>


struct OutputEntry
{
   OutputEntry(const unsigned int              measurementID,
               const boost::asio::ip::address& sourceIP,
               const boost::asio::ip::address& destinationIP,
               const unsigned long long        timeStamp,
               const unsigned int              roundNumber,
               const std::string&              line) :
      MeasurementID(measurementID),
      SourceIP(sourceIP),
      DestinationIP(destinationIP),
      TimeStamp(timeStamp),
      RoundNumber(roundNumber),
      SeqNumber(0),
      Line(line) {
         /*
         std::cout << MeasurementID  << "\t"
                   <<  SourceIP      << "\t"
                   <<  DestinationIP << "\t"
                   <<  TimeStamp     << "\t"
                   <<  RoundNumber   << "\t"
                   <<  SeqNumber     << "\n";
         */
      };

   const unsigned int             MeasurementID;
   const boost::asio::ip::address SourceIP;
   const boost::asio::ip::address DestinationIP;
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
   InputType   Type;
   std::string String;
};


// ###### < operator for sorting ############################################
// NOTE: find() will assume equality for: !(a < b) && !(b < a)
bool operator<(const OutputEntry& a, const OutputEntry& b)
{
   // ====== Level 1: TimeStamp ====================================
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   else if(a.TimeStamp == b.TimeStamp) {
      // ====== Level 2: MeasurementID ============================================
      if(a.MeasurementID < b.MeasurementID) {
         return true;
      }
      else if(a.MeasurementID == b.MeasurementID) {
         // ====== Level 3: SourceIP =============================================
         if(a.SourceIP < b.SourceIP) {
            return true;
         }
         else if(a.SourceIP == b.SourceIP) {
            // ====== Level 4: DestinationIP =====================================
            if(a.DestinationIP < b.DestinationIP) {
               return true;
            }
            else if(a.DestinationIP == b.DestinationIP) {
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



// ###### Count columns #####################################################
static unsigned int countColumns(const std::string& string, const char separator = ' ')
{
   unsigned int columns = 1;
   for(const char c : string) {
      if(c == separator) {
         columns++;
      }
   }
   return columns;
}


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


// ###### Get format identifier (#? HPTC ...) ###############################
static bool getFormatIdentifier(const std::string& line,
                                InputFormat&       format,
                                unsigned int&      version)
{
   if(line.substr(0, 8) == "#? HPCT ") {
      boost::tokenizer<boost::char_separator<char>> tokens(line, boost::char_separator<char>(" "));
      std::vector<std::string> tokensVector;
      for (const auto& token : tokens) {
         tokensVector.push_back(token);
      }
      if(tokensVector.size() >= 4) {
         version = (InputType)atoi(tokensVector[3].c_str());
         if(tokensVector[2] == "Ping") {
            format.Type   = InputType::IT_Ping;
            format.String = "#P";
            return true;
         }
         else if(tokensVector[2] == "Traceroute") {
            format.Type   = InputType::IT_Traceroute;
            format.String = "#T";
            return true;
         }
         else if(tokensVector[2] == "Jitter") {
            format.Type   = InputType::IT_Jitter;
            format.String = "#J";
            return true;
         }
      }
   }
   return false;
}



// ###### Check format of file ##############################################
static bool checkFormat(boost::iostreams::filtering_ostream* outputStream,
                        std::mutex*                          outputMutex,
                        const std::filesystem::path&         fileName,
                        InputFormat&                         format,
                        unsigned int&                        version,
                        unsigned int&                        columns,
                        const std::string&                   line,
                        const char                           separator,
                        bool*                                foundFormat)
{
   const unsigned int inputColumns = countColumns(line);

   // ====== Check input ====================================================
   if(line.size() < 3) {
      HPCT_LOG(fatal) << "Unrecognised format of input data"
                      << " in input file " << fileName;
      return false;
   }

   // ====== Identify format ================================================
   version = 0;   // To be identified!
   if(format.Type == InputType::IT_Unknown) {

      // ====== Check for specified type and version ========================
      if(line.substr(0, 2) == "#?") {
         InputFormat inputFormat;
         if(!getFormatIdentifier(line, inputFormat, version)) {
            HPCT_LOG(fatal) << "Incompatible format of input data"
                           << " in input file " << fileName;
            return false;
         }
         format.Type    = inputFormat.Type;
         format.String = inputFormat.String;
      }
      else {
         // Guess format instead:
         format.Type   = (InputType)line[1];
         format.String = line.substr(0, 3);
      }

      // ====== Ping ========================================================
      std::string columnNames;
      if(format.Type == InputType::IT_Ping) {
         columnNames =
            "Ping "                  // 00: "#P<p>"
            "MeasurementID "         // 01: Measurement ID
            "SourceIP "              // 02: SourceIP address
            "DestinationIP "         // 03: DestinationIP address
            "Timestamp "             // 04: Timestamp (nanoseconds since the UTC epoch, hexadecimal).
            "BurstSeq "              // 05: Sequence number within a burst (decimal), numbered from 0.
            "TrafficClass "          // 06: Traffic Class setting (hexadecimal)
            "PacketSize "            // 07: Packet size, in bytes (decimal; 0 if unknown)
            "ResponseSize "          // 08: Response size, in bytes (decimal; 0 if unknown)
            "Checksum "              // 09: Checksum (hexadecimal)
            "SourcePort "            // 10: Source port (decimal)
            "DestinationPort "       // 11: Destination port (decimal)
            "Status "                // 12: Status (decimal)
            "TimeSource "            // 13: Source of the timing information (hexadecimal) as: AAQQSSHH
            "Delay.AppSend "         // 14: The measured application send delay (nanoseconds, decimal; -1 if not available).
            "Delay.Queuing "         // 15: The measured kernel software queuing delay (nanoseconds, decimal; -1 if not available).
            "Delay.AppReceive "      // 16: The measured application receive delay (nanoseconds, decimal; -1 if not available).
            "RTT.App "               // 17: The measured application RTT (nanoseconds, decimal).
            "RTT.SW "                // 18: The measured kernel software RTT (nanoseconds, decimal; -1 if not available).
            "RTT.HW";                // 19: The measured kernel hardware RTT (nanoseconds, decimal; -1 if not available).
      }

      // ====== Traceroute  =================================================
      else if(format.Type == InputType::IT_Traceroute) {
         columnNames =
            "Traceroute "            // 00: "#T<p>"
            "MeasurementID "         // 01: Measurement ID
            "SourceIP "              // Source IP address
            "DestinationIP "         // Destination IP address
            "Timestamp "             // 04: Absolute time since the epoch in UTC, in microseconds (hexadecimal)
            "RoundNumber "           // 05: Round number (decimal)
            "TotalHops "             // 06: Total hops (decimal)
            "TrafficClass "          // 07: Traffic Class setting (hexadecimal)
            "PacketSize "            // 08: Packet size, in bytes (decimal)
            "Checksum "              // 09: Checksum (hexadecimal)
            "SourcePort "            // 10: Source port (decimal)
            "DestinationPort "       // 11: Destination port (decimal)
            "StatusFlags "           // 12: Status flags (hexadecimal)
            "PathHash "              // 13: Hash of the path (hexadecimal)

            "TAB "                   // 14: NOTE: must be "\t" from combination above!

            "SendTimestamp "         // 15: Timestamp (nanoseconds since the UTC epoch, hexadecimal) for the request to this hop.
            "HopNumber "             // 16: Number of the hop.
            "ResponseSize "          // 17: Response size, in bytes (decimal)
            "Status "                // 18: Status code (decimal!)
            "TimeSource "            // 19: Source of the timing information (hexadecimal) as: AAQQSSHH
            "Delay.AppSend "         // 20: The measured application send delay (nanoseconds, decimal; -1 if not available).
            "Delay.Queuing "         // 21: The measured kernel software queuing delay (nanoseconds, decimal; -1 if not available).
            "Delay.AppReceive "      // 22: The measured application receive delay (nanoseconds, decimal; -1 if not available).
            "RTT.App "               // 23: The measured application RTT (nanoseconds, decimal).
            "RTT.SW "                // 24: The measured kernel software RTT (nanoseconds, decimal; -1 if not available).
            "RTT.HW "                // 25: The measured kernel hardware RTT (nanoseconds, decimal; -1 if not available).
            "HopIP";                 // 26: Hop IP address.
      }

      // ====== Jitter ======================================================
      else if(format.Type == InputType::IT_Jitter) {
         columnNames =
            "Jitter "                 // 00: "#J<p>"
            "MeasurementID "          // 01: Measurement ID
            "SourceIP "               // 02: Source IP address
            "DestinationIP "          // 03: Destination IP address
            "Timestamp "              // 04: Timestamp (nanoseconds since the UTC epoch, hexadecimal).
            "BurstSeq "               // 05: Sequence number within a burst (decimal), numbered from 0.
            "TrafficClass "           // 06: Traffic Class setting (hexadecimal)
            "PacketSize "             // 07: Packet size, in bytes (decimal)
            "Checksum "               // 08: Checksum (hexadecimal)
            "SourcePort "             // 09: Source port (decimal)
            "DestinationPort "        // 10: Destination port (decimal)
            "Status "                 // 11: Status (decimal)
            "JitterType "             // 12: Jitter type (decimal, 0 for calculation as in RFC 3550, Appendix A.8).
            "TimeSource "             // 13: Source of the timing information (hexadecimal) as: AAQQSSHH

            "Packets.AppSend "        // 14: Number of packets for application send delay jitter/mean RTT computation
            "MeanDelay.AppSend "      // 15: Mean application send delay
            "Jitter.AppSend "         // 16: Jitter of application send delay

            "Packets.Queuing "        // 17: Number of packets for queuing delay jitter/mean RTT computation
            "MeanDelay.Queuing "      // 18: Mean queuing delay
            "Jitter.Queuing "         // 19: Jitter of queuing delay

            "Packets.AppReceive "     // 20: Number of packets for application receive delay jitter/mean RTT computation
            "MeanDelay.AppReceive "   // 21: Mean application receive delay
            "Jitter.AppReceive "      // 22: Jitter of application receive delay

            "Packets.App "            // 23: Number of packets for application RTT jitter/mean RTT computation
            "MeanRTT.App "            // 24: Mean application RTT
            "Jitter.App "             // 25: Jitter of application RTT

            "Packets.SW "             // 26: Number of packets for kernel software RTT jitter/mean RTT computation
            "MeanRTT.SW "             // 27: Mean kernel software RTT
            "Jitter.SW "              // 28: Jitter of kernel software RTT

            "Packets.HW "             // 29: Number of packets for kernel hardware RTT jitter/mean RTT computation
            "MeanRTT.HW "             // 30: Mean kernel hardware RTT
            "Jitter.HW";              // 31: Jitter of kernel hardware RTT
      }

      // ====== Error =======================================================
      else {
         HPCT_LOG(fatal) << "Unrecognised type of input data"
                         << " in input file " << fileName;
         return false;
      }
      columns = applySeparator(columnNames, separator);

      const std::lock_guard<std::mutex> lock(*outputMutex);
      if(*foundFormat == false) {
         *outputStream << columnNames << "\n";
         *foundFormat = true;
      }
   }

   // ====== Compatibility check ============================================
   else if(line.substr(0, 2) == "#?") {
      InputFormat inputFormat;
      if( (!getFormatIdentifier(line, inputFormat, version)) || (inputFormat.Type != format.Type) ) {
         HPCT_LOG(fatal) << "Incompatible format for merging ("
                         << inputFormat.String.substr(0, 2) << " vs. " << format.String.substr(0, 2) << ")"
                         << " in input file " << fileName;
         return false;
      }
   }

   // ====== Compatibility check ============================================
   else if(format.String.substr(0, 2) != line.substr(0, 2)) {
      HPCT_LOG(fatal) << "Incompatible format for merging ("
                        << line.substr(0, 2) << " vs. " << format.String.substr(0, 2) << ")"
                        << " in input file " << fileName;
      return false;
   }

   // ====== Guess version, if not specified ================================
   if(version == 0) {
      // ====== Ping ========================================================
      if(format.Type == InputType::IT_Ping) {
         // ------ Ping, Version 2 ------------------------------------------
         if(line[2] != ' ') {
            if(inputColumns >= 20) {
               version = 2;
            }
         }
         // ------ Ping, Version 1 ------------------------------------------
         else {
            if(inputColumns >= 7) {
               version = 1;
            }
         }
      }

      // ====== Traceroute ==================================================
      else if(format.Type == InputType::IT_Traceroute) {
         // ------ Traceroute, Version 2 ------------------------------------
         if(line[2] != ' ') {
            if(inputColumns >= 12) {
               version = 2;
            }
         }
         // ------ Traceroute, Version 1 ------------------------------------
         else {
            if(inputColumns >= 11) {
               version = 1;
            }
         }
      }

      // ====== Jitter ======================================================
      else if(format.Type == InputType::IT_Jitter) {
         version = 2;
      }
   }

   // ====== Error ==========================================================
   if(version == 0) {
      HPCT_LOG(fatal) << "Unrecognised version of input data"
                      << " in input file " << fileName;
      return false;
   }
   return true;
}


// ###### Dump results file #################################################
static bool dumpResultsFile(std::atomic<unsigned int>*                             errorCounter,
                            std::set<OutputEntry*, pointer_lessthan<OutputEntry>>* outputSet,
                            boost::iostreams::filtering_ostream*                   outputStream,
                            std::mutex*                                            outputMutex,
                            const std::filesystem::path&                           fileName,
                            InputFormat&                                           format,
                            unsigned int&                                          columns,
                            const char                                             separator,
                            bool*                                                  foundFormat,
                            const bool                                             checkOnly = false)
{
   // ====== Open input file ================================================
#ifdef POSIX_FADV_SEQUENTIAL
   // With fadvise() to optimise caching:
   int handle = open(fileName.c_str(), 0, O_RDONLY);
   if(handle < 0) {
      HPCT_LOG(error) << "Failed to read input file " << fileName;
      (*errorCounter)++;
      return false;
   }
   if(posix_fadvise(handle, 0, 0, POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED|POSIX_FADV_NOREUSE) < 0) {
      HPCT_LOG(warning) << "posix_fadvise() failed:" << strerror(errno);
   }

   boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> fstreambuffer(
      handle,
      boost::iostreams::file_descriptor_flags::close_handle);
   std::istream inputFile(&fstreambuffer);
#else
#warning Without fadvise()
      // Without fadvise():
   std::ifstream                       inputFile;
   inputFile.open(fileName, std::ios_base::in | std::ios_base::binary);
   if(!inputFile.is_open()) {
      HPCT_LOG(error) << "Failed to read input file " << fileName;
      (*errorCounter)++;
      return false;
   }
#endif

      // ====== Prepare input stream ========================================
   boost::iostreams::filtering_istream inputStream;
   std::string                         extension(fileName.extension());
   boost::algorithm::to_lower(extension);
   if(extension == ".xz") {
      const boost::iostreams::lzma_params params(
         boost::iostreams::lzma::default_compression,
         std::thread::hardware_concurrency());
      inputStream.push(boost::iostreams::lzma_decompressor(params));
   }
   else if(extension == ".bz2") {
      inputStream.push(boost::iostreams::bzip2_decompressor());
   }
   else if(extension == ".gz") {
      inputStream.push(boost::iostreams::gzip_decompressor());
   }
   inputStream.push(inputFile);

   // ====== Process lines of the input file ================================
   unsigned int       version    = 0;
   std::string        line;
   unsigned long long lineNumber = 0;
   OutputEntry*       newEntry   = nullptr;
   unsigned long long oldTimeStamp;   // Just used for version 1 conversion!
   while(std::getline(inputStream, line, '\n')) {
      lineNumber++;

      // ====== #<line> =====================================================
      if(line.size() < 2) {
         continue;
      }
      else if(line[0] == '#') {
         if(version == 0) {
            if(!checkFormat(outputStream, outputMutex, fileName, format, version, columns, line, separator, foundFormat)) {
               (*errorCounter)++;
               return false;
            }
            if(checkOnly) {
               return true;
            }
         }
         if(line[1] == '?') {
            continue;   // #? ...
         }

         try {
            // ------ Conversion from old versions --------------------------
            if(version < 2) {
               if(format.Type == InputType::IT_Ping) {
                  line = convertOldPingLine(line);
               }
               else if(format.Type == InputType::IT_Traceroute) {
                  line = convertOldTracerouteLine(line, oldTimeStamp);
               }
            }

            // ------ Obtain pointers to first N entries --------------------
            const unsigned int maxColumns = 6;
            const char*        linestr    = line.c_str();
            const char*        value[maxColumns];
            size_t             length[maxColumns];

            unsigned int i = 0;
            unsigned int c = 0;
            unsigned int l = 0;
            value[c] = linestr;
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
            c++;

            // ------ Create output entry --------------------------------
            if(c < maxColumns) {
               HPCT_LOG(fatal) << "Unexpected syntax"
                              << " in input file " << fileName << ", line " << lineNumber;
               (*errorCounter)++;
               return false;
            }
            size_t measurementIDIndex;
            const unsigned int measurementID = std::stoul(value[1], &measurementIDIndex, 10);
            if(measurementIDIndex != length[1]) {
               throw std::range_error("Bad measurement ID");
            }
            const boost::asio::ip::address sourceIP      = boost::asio::ip::make_address(std::string(value[2], length[2]));
            const boost::asio::ip::address destinationIP = boost::asio::ip::make_address(std::string(value[3], length[3]));
            size_t                         timeStampIndex;
            const unsigned long long       timeStamp   = std::stoull(value[4], &timeStampIndex, 16);
            if(timeStampIndex != length[4]) {
               throw std::range_error("Bad time stamp");
            }
            size_t       roundNumberIndex;
            unsigned int roundNumber;
            if(format.Type == InputType::IT_Traceroute) {
               roundNumber = std::stoul(value[5], &roundNumberIndex, 10);
               if(roundNumberIndex != length[5]) {
                  throw std::range_error("Bad round number");
               }
            }
            else {
               roundNumber = 0;
            }

            if(newEntry != nullptr) {
               delete newEntry;
            }
            newEntry = new OutputEntry(measurementID, sourceIP, destinationIP, timeStamp,
                                       roundNumber, line);

            // ====== Write entry, if not Traceroute ========================
            if(format.Type != InputType::IT_Traceroute) {
               const unsigned int seenColumns = applySeparator(newEntry->Line, separator);
               if(seenColumns != columns) {
                  HPCT_LOG(fatal) << "Got " << seenColumns << " instead of expected "
                                 << columns << " columns"
                                 << " in input file " << fileName << ", line " << lineNumber;
                  (*errorCounter)++;
                  return false;
               }

               const std::lock_guard<std::mutex> lock(*outputMutex);
               if(outputSet) {
                  auto success = outputSet->insert(newEntry);
                  if(!success.second) {
                     HPCT_LOG(fatal) << "Duplicate entry"
                                    << " in input file " << fileName << ", line " << lineNumber;
                     (*errorCounter)++;
                     return false;
                  }
               }
               else {
                  *outputStream << newEntry->Line << "\n";
                  delete newEntry;
               }
               newEntry = nullptr;
            }
         }
         catch(const std::exception& e) {
            HPCT_LOG(fatal) << "Unexpected input"
                           << " in input file " << fileName << ", line " << lineNumber
                           << ": " << e.what();
            (*errorCounter)++;
            return false;
         }
      }

      // ====== TAB<line> ===================================================
      else if(line[0] == '\t') {

         if(format.Type == InputType::IT_Traceroute) {
            // ------ Conversion from old versions -----------------------------
            try {
               if(version < 2) {
                  line = convertOldTracerouteLine(line, oldTimeStamp);
               }
            }
            catch(const std::exception& e) {
               HPCT_LOG(fatal) << "Unexpected input"
                              << " in input file " << fileName << ", line " << lineNumber
                              << ": " << e.what();
               (*errorCounter)++;
               return false;
            }

            if(newEntry == nullptr) {
               HPCT_LOG(fatal) << "TAB line without corresponding header line"
                              << " in input file " << fileName << ", line " << lineNumber;
               (*errorCounter)++;
               return false;
            }

            // NOTE: newEntry is the header line, used as reference entry!
            newEntry->SeqNumber++;

            OutputEntry* newSubEntry = new OutputEntry(*newEntry);
            newSubEntry->Line += " ~ " + ((line[1] != ' ') ? line.substr(1) : line.substr(2));

            const unsigned int seenColumns = applySeparator(newSubEntry->Line, separator);
            if(seenColumns != columns) {
               HPCT_LOG(fatal) << "Got " << seenColumns << " instead of expected "
                                 << columns << " columns"
                                 << " in input file " << fileName << ", line " << lineNumber;
               (*errorCounter)++;
               return false;
            }

            const std::lock_guard<std::mutex> lock(*outputMutex);
            if(outputSet) {
               auto success = outputSet->insert(newSubEntry);
               if(!success.second) {
                  HPCT_LOG(fatal) << "Duplicate tab entry"
                                 << " in input file " << fileName << ", line " << lineNumber;
                  (*errorCounter)++;
                  return false;
               }
            }
            else {
               *outputStream << newSubEntry->Line << "\n";
               delete newSubEntry;
            }
         }

      }

      // ------ Syntax error ------------------------------------------------
      else {
         HPCT_LOG(fatal) << "Unexpected syntax"
                         << " in input file " << fileName << ", line " << lineNumber;
         (*errorCounter)++;
         return false;
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
   unsigned int                       logLevel;
   bool                               logColor;
   std::filesystem::path              logFile;
   std::filesystem::path              outputFileName;
   std::vector<std::filesystem::path> inputFileNameList;
   bool                               inputFileNamesFromStdin;
   std::vector<std::filesystem::path> inputFileNamesFromFileList;
   bool                               inputResultsFromStdin;
   char                               separator;
   bool                               sorted;
   unsigned int                       maxThreads;

   // ====== Initialize =====================================================
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

      ( "maxthreads,T",
           boost::program_options::value<unsigned int>(&maxThreads)->default_value(std::thread::hardware_concurrency()),
           "Maximum number of threads" )

      ( "input-results-from-stdin,R",
           boost::program_options::value<bool>(&inputResultsFromStdin)->implicit_value(true)->default_value(false),
           "Read results from standard input" )

      ( "input-file-names-from-stdin,N",
           boost::program_options::value<bool>(&inputFileNamesFromStdin)->implicit_value(true)->default_value(false),
           "Read input file names from standard input" )
      ( "input-file-names-from-file,F",
           boost::program_options::value<std::vector<std::filesystem::path>>(&inputFileNamesFromFileList),
           "Read input file names from file" )

      ( "output,o",
           boost::program_options::value<std::filesystem::path>(&outputFileName)->default_value(std::filesystem::path()),
           "Output file" )
      ( "separator,s",
           boost::program_options::value<char>(&separator)->default_value(' '),
           "Separator character" )
      ( "sorted,A",
           boost::program_options::value<bool>(&sorted)->implicit_value(true)->default_value(true),
           "Sorted results" )
      ( "unsorted,U",
           boost::program_options::value<bool>(&sorted)->implicit_value(false),
           "Unsorted results" )
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
                                          boost::program_options::command_line_style::style_t::unix_style
                                       ).
                                       options(allOptions).positional(positionalParameters).
                                       run(), vm);
      boost::program_options::notify(vm);
   }
   catch(std::exception& e) {
      std::cerr << "Bad parameter: " << e.what() << "!\n";
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " input_file ... OPTIONS" << "\n"
                 << commandLineOptions;
       return 1;
   }
   if( (separator != ' ')  &&
       (separator != '\t') &&
       (separator != ',')  &&
       (separator != ':')  &&
       (separator != ';')  &&
       (separator != '|') ) {
      std::cerr << "Invalid separator \"" << separator << "\"!\n";
      exit(1);
   }
   if(maxThreads < 1) {
      maxThreads = std::thread::hardware_concurrency();
   }

   if(inputResultsFromStdin) {
      inputFileNameList.clear();
      inputFileNameList.push_back("/dev/stdin");
   }
   else {
      if(inputFileNamesFromStdin) {
         inputFileNamesFromFileList.push_back("/dev/stdin");
      }
      for(const std::filesystem::path& inputFileNamesFileName : inputFileNamesFromFileList) {
         std::string inputFileName;
         std::ifstream is(inputFileNamesFileName);
         if(!is.good()) {
            std::cerr << "ERROR: Unable to read input file names from " << inputFileNamesFileName << "!\n";
            exit(1);
         }
         do {
            if(inputFileNamesFromStdin) {
               std::cout << "Input file: ";
               std::cout.flush();
            }
            is >> inputFileName;
            if(!inputFileName.empty()) {
               if(inputFileNamesFromStdin) {
                  std::cout << inputFileName << "\n";
               }
               inputFileNameList.push_back(inputFileName);
            }
         } while(!is.eof());
      }
   }
   if(inputFileNameList.size() == 0) {
      std::cerr << "No input files.\n";
      return 0;
   }

   // ====== Initialize =====================================================
   initialiseLogger(logLevel, logColor,
                    (!logFile.empty()) ? logFile.string().c_str() : nullptr);

   // ====== Open output file ===============================================
   std::string                         extension(outputFileName.extension());
   std::ofstream                       outputFile;
   boost::iostreams::filtering_ostream outputStream;
   const std::filesystem::path         tmpOutputFileName = outputFileName.string() + ".tmp";
   if(!outputFileName.empty()) {
      outputFile.open(tmpOutputFileName, std::ios_base::out | std::ios_base::binary);
      if(!outputFile.is_open()) {
         HPCT_LOG(fatal) << "Failed to create output file " << tmpOutputFileName;
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

   // ====== Read the input files ===========================================
   const std::set<std::filesystem::path>                 inputFileNameSet(inputFileNameList.begin(), inputFileNameList.end());
   std::set<OutputEntry*, pointer_lessthan<OutputEntry>> outputSet;
   std::mutex                                            outputMutex;
   InputFormat                                           format { InputType::IT_Unknown };
   unsigned int                                          columns = 0;

   // ------ Identify format of the first file ------------------------------
   std::atomic<unsigned int> errorCounter = 0;
   const std::filesystem::path firstInputFileName = *(inputFileNameSet.begin());
   HPCT_LOG(info) << "Identifying format from " << firstInputFileName << " ...";
   bool foundFormat = false;
   if(!dumpResultsFile(&errorCounter,
                       (sorted == true) ? &outputSet : nullptr, &outputStream, &outputMutex,
                       firstInputFileName, format, columns, separator, &foundFormat,
                       inputResultsFromStdin ? false : true)) {
      return 1;
   }
   HPCT_LOG(info) << "Format: Type=" << (char)format.Type;

   // ------  Use thread pool to read all files -----------------------------
   boost::asio::thread_pool threadPool(maxThreads);
   HPCT_LOG(info) << "Reading " << inputFileNameSet.size() << " files using up to "
                  << maxThreads << " threads ...";
   const std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();
   for(const std::filesystem::path& inputFileName : inputFileNameSet) {
      boost::asio::post(threadPool, std::bind(dumpResultsFile,
                        &errorCounter,
                        (sorted == true) ? &outputSet : nullptr, &outputStream, &outputMutex,
                        inputFileName, format, columns, separator, &foundFormat,
                        false));
   }
   threadPool.join();
   if(errorCounter > 0) {
      exit(1);
   }
   const std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();
   if(sorted == true) {
      HPCT_LOG(info) << "Read " << outputSet.size() << " results rows in "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms";
   }
   else {
      HPCT_LOG(info) << "Reading input and writing output took "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms";
   }

   // ====== Print the results ==============================================
   if(sorted == true) {
      HPCT_LOG(info) << "Writing " << outputSet.size() << " results rows ...";
      const std::chrono::system_clock::time_point t3 = std::chrono::system_clock::now();
      for(std::set<OutputEntry*, pointer_lessthan<OutputEntry>>::const_iterator iterator = outputSet.begin();
         iterator != outputSet.end(); iterator++) {
         outputStream << (*iterator)->Line << "\n";
         delete *iterator;
      }
      outputStream.reset();

      const std::chrono::system_clock::time_point t4 = std::chrono::system_clock::now();
      HPCT_LOG(info) << "Wrote " << outputSet.size() << " results rows in "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() << " ms";
   }

   if(!outputFileName.empty()) {
      try {
         std::filesystem::rename(tmpOutputFileName, outputFileName);
      }
      catch(const std::exception& e) {
         HPCT_LOG(fatal) << "Unable to rename " << tmpOutputFileName << " to " << outputFileName
                         << ": " << e.what();
         exit(1);
      }
   }

   return 0;
}
