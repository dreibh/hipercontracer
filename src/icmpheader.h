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

#ifndef ICMPHEADER_H
#define ICMPHEADER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>

#include <istream>
#include <ostream>

#include "internet16.h"


// ==========================================================================
// From RFC 4443:
//
//        0                   1                   2                   3
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |     Type      |     Code      |          Checksum             |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |           Identifier          |        Sequence Number        |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |     Data ...
//       +-+-+-+-+-
//
// ==========================================================================

class ICMPHeader
{
   public:
   ICMPHeader() {
      std::fill(Data, Data + sizeof(Data), 0);
   }
   ICMPHeader(const char* inputData, const size_t length) {
      memcpy(Data, inputData, std::min(length, (size_t)8));
   }

   inline uint8_t  type()       const { return Data[0];      }
   inline uint8_t  code()       const { return Data[1];      }
   inline uint16_t checksum()   const { return decode(2, 3); }
   inline uint16_t identifier() const { return decode(4, 5); }
   inline uint16_t seqNumber()  const { return decode(6, 7); }

   inline void type(uint8_t type)          { Data[0] = type;         }
   inline void code(uint8_t code)          { Data[1] = code;         }
   inline void checksum(uint16_t checksum) { encode(2, 3, checksum); }
   inline void identifier(uint16_t id)     { encode(4, 5, id);       }
   inline void seqNumber(uint16_t seqNum)  { encode(6, 7, seqNum);   }

   inline void computeInternet16(uint32_t& sum) const {
      ::computeInternet16(sum, (uint8_t*)&Data, sizeof(Data));
   }

   inline const uint8_t* data() const {
      return (const uint8_t*)&Data;
   }
   inline size_t size() const {
      return sizeof(Data);
   }

   inline friend std::istream& operator>>(std::istream& is, ICMPHeader& header) {
      return is.read(reinterpret_cast<char*>(header.Data), 8);
   }

   inline friend std::ostream& operator<<(std::ostream& os, const ICMPHeader& header) {
      return os.write(reinterpret_cast<const char*>(header.Data), 8);
   }

   private:
   inline uint16_t decode(const unsigned int a, const unsigned int b) const {
      return ((uint16_t)Data[a] << 8) + Data[b];
   }

   inline void encode(const unsigned int a, const unsigned int b, const uint16_t n) {
      Data[a] = static_cast<uint8_t>(n >> 8);
      Data[b] = static_cast<uint8_t>(n & 0xff);
   }

   uint8_t Data[8];
};

#endif
