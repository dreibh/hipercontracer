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

#include "conversions.h"
#include "results-exception.h"

#include <boost/format.hpp>


// ###### Convert Ping data line from old version to version 2 ##############
std::string convertOldPingLine(const std::string& line)
{
   const char*        linestr    = line.c_str();
   const unsigned int maxColumns = 11;
   const char*        value[maxColumns];
   size_t             length[maxColumns];
   unsigned int       i          = 0;
   unsigned int       c          = 0;
   unsigned int       l          = 0;

   // ====== Obtain pointers to entries =====================================
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

   // ====== Generate data line in version 2 ================================
   if(c >= 7) {
      size_t                   timeStampIndex;
      const unsigned long long timeStamp = 1000ULL * std::stoull(value[3], &timeStampIndex, 16);
      if(timeStampIndex != length[3]) {
         throw ResultsReaderDataErrorException("Bad time stamp");
      }
      size_t                   rttIndex;
      const unsigned long long rtt = 1000ULL * std::stoull(value[6], &rttIndex, 10);
      if(rttIndex != length[6]) {
         throw ResultsReaderDataErrorException("Bad RTT value");
      }

      const std::string newLine =
         std::string(value[0], length[0]) + "i " +                // "#P<p>"
         "0 " +                                                   // Measurement ID
         std::string(value[1], length[1]) + " " +                 // Source address
         std::string(value[2], length[2]) + " " +                 // Destination address
         (boost::format("%x ") % timeStamp).str() +               // Timestamp
         "0 " +                                                   // Sequence number within a burst (0, not supported in version 1)
         ((c >= 8) ?
             /* TrafficClass was added in HiPerConTracer 1.4.0! */
             (boost::format("%x ") % std::string(value[7], length[7])).str() : std::string("0 ")) +
         ((c >= 9) ?
             /* PacketSize was added in HiPerConTracer 1.6.0! */
             std::string(value[8], length[8]) : std::string("0")) + " " +
         "0 " +                                                   // Response size (0, not supported in version 1)
         std::string(value[4], length[4]) + " " +                 // Checksum
         "0 0 " +                                                 // Source and destination port
         std::string(value[5], length[5]) + " " +                 // Status
         ((c >= 10) ?
             /* TimeSource was added in HiPerConTracer 2.0.0! */
             (boost::format("%x ") % std::string(value[9], length[9])).str() : std::string("00000000 ")) +   // Source of the timing information
         "-1 -1 -1 " +
         std::to_string(rtt) + " -1 -1";

      // std::cout << "<= " << line    << "\n"
      //           << "=> " << newLine << "\n";
      return newLine;
   }
   throw ResultsReaderDataErrorException("Unexpected number of columns");
}


// ###### Convert Traceroute data line from old version to version 2 ########
std::string convertOldTracerouteLine(const std::string&  line,
                                     unsigned long long& timeStamp)
{
   const char*        linestr    = line.c_str();
   const unsigned int maxColumns = 11;
   const char*        value[maxColumns];
   size_t             length[maxColumns];
   unsigned int       i          = 0;
   unsigned int       c          = 0;
   unsigned int       l          = 0;

   // ====== Obtain pointers to entries =====================================
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

   // ====== Generate data line in version 2 ================================
   if(c > 0) {
      if( (value[0][0] == '#') && (c >= 9) ) {
         size_t timeStampIndex;
         timeStamp = 1000ULL * std::stoull(value[3], &timeStampIndex, 16);
         if(timeStampIndex != length[3]) {
            throw ResultsReaderDataErrorException("Bad time stamp");
         }

         const std::string newLine =
            std::string(value[0], length[0]) + "i " +                // "#T<p>"
            "0 " +                                                   // Measurement ID
            std::string(value[1], length[1]) + " " +                 // Source address
            std::string(value[2], length[2]) + " " +                 // Destination address
            (boost::format("%x ") % timeStamp).str() +               // Timestamp
            std::string(value[4], length[4]) + " " +                 // Round number
            std::string(value[6], length[6]) + " " +                 // Total hops
            ((c >= 10) ?
                /* TrafficClass was added in HiPerConTracer 1.4.0! */
                (boost::format("%x ") % std::string(value[9], length[9])).str() : std::string("00000000 ")) +
            ((c >= 11) ?
                /* PacketSize was added in HiPerConTracer 1.6.0! */
                std::string(value[10], length[10]) : std::string("0")) + " " +
            std::string(value[5], length[5]) + " " +                 // Checksum
            "0 0 " +                                                 // Source and destination port
            std::string(value[7], length[7]) + " " +                 // Status flags
            std::string(value[8], length[8]);                        // Path hash

         // std::cout << "<= " << line    << "\n"
         //           << "=> " << newLine << "\n";
         return newLine;
      }

      else if( (value[0][0] == '\t') && (c >= 5) ) {
         size_t             statusIndex;
         const unsigned int status = std::stoul(value[2], &statusIndex, 16);
         if(statusIndex != length[2]) {
            throw ResultsReaderDataErrorException("Bad status value");
         }
         size_t                   rttIndex;
         const unsigned long long rtt = 1000ULL * std::stoull(value[3], &rttIndex, 10);
         if(rttIndex != length[3]) {
            throw ResultsReaderDataErrorException("Bad RTT value");
         }

         const std::string newLine =
            '\t' +
            (boost::format("%x ") % timeStamp).str() +               // Timestamp
            std::string(value[1], length[1]) + " " +                 // Hop number
            "0 " +                                                   // Response size (0, not supported in version 1)
            std::to_string(status) + " " +                           // Status code (decimal!)
            ((c >= 6) ?
                /* TimeSource was added in HiPerConTracer 2.0.0! */
                (boost::format("%x ") % std::string(value[5], length[5])).str() : std::string("00000000 ")) +   // Source of the timing information
            "-1 -1 -1 " +
            std::to_string(rtt) + " -1 -1 " +
            std::string(value[4], length[4]);                        // Hop IP address

         // std::cout << "<= " << line    << "\n"
         //           << "=> " << newLine << "\n";
         return newLine;
      }
   }

   throw ResultsReaderDataErrorException("Unexpected number of columns");
}
