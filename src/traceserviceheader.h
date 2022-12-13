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

#ifndef TRACESERVICEHEADER_H
#define TRACESERVICEHEADER_H

#include <stdint.h>

#include <cassert>
#include <chrono>
#include <vector>
#include <istream>

#include "internet16.h"


// ==========================================================================
// Format:
// 00 4 MagicNumber
// 04 1 SendTTL
// 05 1 Round
// 06 2 Checksum Tweak (ICMP only) / Sequence Number (other protocols)
// 08 8 Send Time Stamp
// ==========================================================================

#define MIN_TRACESERVICE_HEADER_SIZE    16
#define MAX_TRACESERVICE_HEADER_SIZE 65536

class TraceServiceHeader
{
   public:
   TraceServiceHeader(const size_t size = MIN_TRACESERVICE_HEADER_SIZE)
      : Size(size)
   {
      assert((Size >= MIN_TRACESERVICE_HEADER_SIZE) && (Size <= MAX_TRACESERVICE_HEADER_SIZE));
      std::fill(Data, Data + std::min(Size, (size_t)64), 0);
      for(unsigned int i = 64;i < Size; i++) {
         Data[i] = i & 0xff;
      }
   }

   inline uint32_t magicNumber() const {
      return ( ((uint32_t)Data[0] << 24) |
               ((uint32_t)Data[1] << 16) |
               ((uint32_t)Data[2] << 8)  |
               (uint32_t)Data[3] );
   }
   inline void magicNumber(const uint32_t number) {
      Data[0] = static_cast<uint8_t>( (number >> 24) & 0xff );
      Data[1] = static_cast<uint8_t>( (number >> 16) & 0xff );
      Data[2] = static_cast<uint8_t>( (number >> 8)  & 0xff );
      Data[3] = static_cast<uint8_t>( number & 0xff );
   }

   inline uint8_t sendTTL() const       { return Data[4];    }
   inline void sendTTL(uint8_t sendTTL) { Data[4] = sendTTL; }

   inline uint8_t round() const         { return Data[5];    }
   inline void round(uint8_t round)     { Data[5] = round;   }

   inline uint16_t checksumTweak() const {
      return ( ((uint16_t)Data[6] << 8) |
               (uint16_t)Data[7] );
   }
   inline void checksumTweak(const uint16_t value) {
      Data[6] = static_cast<uint8_t>( (value >> 8)  & 0xff );
      Data[7] = static_cast<uint8_t>( value & 0xff );
   }

   inline uint16_t seqNumber() const {
      return checksumTweak();
   }
   inline void seqNumber(const uint16_t value) {
      checksumTweak(value);
   }

   inline uint64_t sendTimeStamp() const {
      return ( ((uint64_t)Data[8] << 56)  |
               ((uint64_t)Data[9] << 48)  |
               ((uint64_t)Data[10] << 40) |
               ((uint64_t)Data[11] << 32) |
               ((uint64_t)Data[12] << 24) |
               ((uint64_t)Data[13] << 16) |
               ((uint64_t)Data[14] << 8)  |
               (uint64_t)Data[15] );
   }
   inline void sendTimeStamp(const uint64_t timeStamp) {
      Data[8]  = static_cast<uint8_t>( (timeStamp >> 56) & 0xff );
      Data[9]  = static_cast<uint8_t>( (timeStamp >> 48) & 0xff );
      Data[10] = static_cast<uint8_t>( (timeStamp >> 40) & 0xff );
      Data[11] = static_cast<uint8_t>( (timeStamp >> 32) & 0xff );
      Data[12] = static_cast<uint8_t>( (timeStamp >> 24) & 0xff );
      Data[13] = static_cast<uint8_t>( (timeStamp >> 16) & 0xff );
      Data[14] = static_cast<uint8_t>( (timeStamp >> 8) & 0xff );
      Data[15] = static_cast<uint8_t>( timeStamp & 0xff );
   }
   inline void sendTimeStamp(const std::chrono::system_clock::time_point& timeStamp) {
      // For HiPerConTracer packets: time stamp is microseconds since 1976-09-26.
      static const std::chrono::system_clock::time_point HiPerConTracerEpoch =
         std::chrono::system_clock::from_time_t(212803200);
      sendTimeStamp(std::chrono::duration_cast<std::chrono::microseconds>(
                       timeStamp - HiPerConTracerEpoch).count());
   }

   inline void computeInternet16(uint32_t& sum) const {
      ::computeInternet16(sum, (uint8_t*)&Data, Size);
   }

   inline friend std::istream& operator>>(std::istream& is, TraceServiceHeader& header) {
      return is.read(reinterpret_cast<char*>(header.Data), header.Size);
   }

   inline friend std::ostream& operator<<(std::ostream& os, const TraceServiceHeader& header) {
      return os.write(reinterpret_cast<const char*>(header.Data), header.Size);
   }

   private:
   const size_t Size;
   uint8_t      Data[MAX_TRACESERVICE_HEADER_SIZE];
};

#endif
