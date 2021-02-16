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
// Copyright (C) 2015-2021 by Thomas Dreibholz
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

   inline uint32_t magicNumber() const {
      return ( ((uint32_t)data[0] << 24) |
               ((uint32_t)data[1] << 16) |
               ((uint32_t)data[2] << 8)  |
               (uint32_t)data[3] );
   }
   inline void magicNumber(const uint32_t number) {
      data[0] = static_cast<uint8_t>( (number >> 24) & 0xff );
      data[1] = static_cast<uint8_t>( (number >> 16) & 0xff );
      data[2] = static_cast<uint8_t>( (number >> 8)  & 0xff );
      data[3] = static_cast<uint8_t>( number & 0xff );
   }

   inline uint8_t sendTTL() const       { return data[4];    }
   inline void sendTTL(uint8_t sendTTL) { data[4] = sendTTL; }

   inline uint8_t round() const         { return data[5];    }
   inline void round(uint8_t round)     { data[5] = round;   }

   inline uint16_t checksumTweak() const {
      return ( ((uint16_t)data[6] << 8)  |
               (uint16_t)data[7] );
   }
   inline void checksumTweak(const uint16_t value) {
      data[6] = static_cast<uint8_t>( (value >> 8)  & 0xff );
      data[7] = static_cast<uint8_t>( value & 0xff );
   }

   inline uint64_t sendTimeStamp() const {
      return ( ((uint64_t)data[8] << 56)  |
               ((uint64_t)data[9] << 48)  |
               ((uint64_t)data[10] << 40) |
               ((uint64_t)data[11] << 32) |
               ((uint64_t)data[12] << 24) |
               ((uint64_t)data[13] << 16) |
               ((uint64_t)data[14] << 8)  |
               (uint64_t)data[15] );
   }
   inline void sendTimeStamp(const uint64_t timeStamp) {
      data[8]  = static_cast<uint8_t>( (timeStamp >> 56) & 0xff );
      data[9]  = static_cast<uint8_t>( (timeStamp >> 48) & 0xff );
      data[10] = static_cast<uint8_t>( (timeStamp >> 40) & 0xff );
      data[11] = static_cast<uint8_t>( (timeStamp >> 32) & 0xff );
      data[12] = static_cast<uint8_t>( (timeStamp >> 24) & 0xff );
      data[13] = static_cast<uint8_t>( (timeStamp >> 16) & 0xff );
      data[14] = static_cast<uint8_t>( (timeStamp >> 8) & 0xff );
      data[15] = static_cast<uint8_t>( timeStamp & 0xff );
   }

   inline friend std::istream& operator>>(std::istream& is, TraceServiceHeader& header) {
      return(is.read(reinterpret_cast<char*>(header.data), sizeof(header.data)));
   }

   inline friend std::ostream& operator<<(std::ostream& os, const TraceServiceHeader& header) {
      return(os.write(reinterpret_cast<const char*>(header.data), sizeof(header.data)));
   }

   inline std::vector<uint8_t> contents() const {
      std::vector<uint8_t> contents((uint8_t*)&data, (uint8_t*)&data[sizeof(data)]);
      return(contents);
   }

   private:
   uint8_t data[16];
};

#endif
