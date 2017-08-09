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
// Copyright (C) 2015-2017 by Thomas Dreibholz
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

#ifndef TRACESERVICEHEADER_H
#define TRACESERVICEHEADER_H

#include <stdint.h>

#include <vector>
#include <istream>


// ==========================================================================
// Format:
// 00 4 MagicNumber
// 04 1 SendTTL
// 05 1 Round
// 06 2 Checksum Tweak
// 08 8 Send Time Stamp
// ==========================================================================


class TraceServiceHeader
{
   public:
   TraceServiceHeader() {
      std::fill(data, data + sizeof(data), 0);
   }

   inline unsigned int magicNumber() const {
      return ( ((unsigned int)data[0] << 24) |
               ((unsigned int)data[1] << 16) |
               ((unsigned int)data[2] << 8)  |
               (unsigned int)data[3] );
   }
   inline void magicNumber(const unsigned int number) {
      data[0] = static_cast<unsigned char>( (number >> 24) & 0xff );
      data[1] = static_cast<unsigned char>( (number >> 16) & 0xff );
      data[2] = static_cast<unsigned char>( (number >> 8)  & 0xff );
      data[3] = static_cast<unsigned char>( number & 0xff );
   }

   inline unsigned char sendTTL() const       { return data[4];    }
   inline void sendTTL(unsigned char sendTTL) { data[4] = sendTTL; }

   inline unsigned char round() const         { return data[5];    }
   inline void round(unsigned char round)     { data[5] = round;   }

   inline uint16_t checksumTweak() const {
      return ( ((uint16_t)data[6] << 8)  |
               (uint16_t)data[7] );
   }
   inline void checksumTweak(const uint16_t value) {
      data[6] = static_cast<unsigned char>( (value >> 8)  & 0xff );
      data[7] = static_cast<unsigned char>( value & 0xff );
   }

   inline unsigned long long sendTimeStamp() const {
      return ( ((unsigned long long)data[8] << 56)  |
               ((unsigned long long)data[9] << 48)  |
               ((unsigned long long)data[10] << 40) |
               ((unsigned long long)data[11] << 32) |
               ((unsigned long long)data[12] << 24) |
               ((unsigned long long)data[13] << 16) |
               ((unsigned long long)data[14] << 8)  |
               (unsigned long long)data[15] );
   }
   inline void sendTimeStamp(const unsigned long long timeStamp) {
      data[8]  = static_cast<unsigned char>( (timeStamp >> 56) & 0xff );
      data[9]  = static_cast<unsigned char>( (timeStamp >> 48) & 0xff );
      data[10] = static_cast<unsigned char>( (timeStamp >> 40) & 0xff );
      data[11] = static_cast<unsigned char>( (timeStamp >> 32) & 0xff );
      data[12] = static_cast<unsigned char>( (timeStamp >> 24) & 0xff );
      data[13] = static_cast<unsigned char>( (timeStamp >> 16) & 0xff );
      data[14] = static_cast<unsigned char>( (timeStamp >> 8) & 0xff );
      data[15] = static_cast<unsigned char>( timeStamp & 0xff );
   }

   inline friend std::istream& operator>>(std::istream& is, TraceServiceHeader& header) {
      return(is.read(reinterpret_cast<char*>(header.data), sizeof(header.data)));
   }

   inline friend std::ostream& operator<<(std::ostream& os, const TraceServiceHeader& header) {
      return(os.write(reinterpret_cast<const char*>(header.data), sizeof(header.data)));
   }

   inline std::vector<unsigned char> contents() const {
      std::vector<unsigned char> contents((unsigned char*)&data, (unsigned char*)&data[sizeof(data)]);
      return(contents);
   }

   private:
   unsigned char data[16];
};

#endif
