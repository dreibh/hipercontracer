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

#ifndef UDPHEADER_H
#define UDPHEADER_H

#include <istream>
#include <algorithm>
#include <boost/asio/ip/address_v4.hpp>


// ==========================================================================
// From RFC 791:
//
//    0      7 8     15 16    23 24    31
//    +--------+--------+--------+--------+
//    |     Source      |   Destination   |
//    |      Port       |      Port       |
//    +--------+--------+--------+--------+
//    |                 |                 |
//    |     Length      |    Checksum     |
//    +--------+--------+--------+--------+
//    |
//    |          data octets ...
//    +---------------- ...
//
// ==========================================================================

class UDPHeader
{
   public:
   UDPHeader() {
      std::fill(Data, Data + sizeof(Data), 0);
   }

   inline uint16_t sourcePort()      const                     { return decode(0, 1);           }
   inline uint16_t destinationPort() const                     { return decode(2, 3);           }
   inline uint16_t length()          const                     { return decode(4, 5);           }
   inline uint16_t checksum()        const                     { return decode(6, 7);           }

   inline void sourcePort(const uint16_t sourcePort)           { encode(0, 1, sourcePort);      }
   inline void destinationPort(const uint16_t destinationPort) { encode(2, 3, destinationPort); }
   inline void length(const uint16_t length)                   { encode(4, 5, length);          }
   inline void checksum(const uint16_t checksum)               { encode(6, 7, checksum);        }

   friend std::istream& operator>>(std::istream& is, UDPHeader& header) {
      is.read(reinterpret_cast<char*>(header.Data), 8);
      std::streamsize totalLength = header.length() - 8;
      if(totalLength < 8) {
         is.setstate(std::ios::failbit);
      }
      else {
         is.read(reinterpret_cast<char*>(header.Data) + 8, totalLength - 8);
      }
      return is;
   }

   inline friend std::ostream& operator<<(std::ostream& os, const UDPHeader& header) {
      return os.write(reinterpret_cast<const char*>(header.Data), sizeof(header.Data));
   }

   inline std::vector<uint8_t> contents() const {
      return std::vector<uint8_t>((uint8_t*)&Data, (uint8_t*)&Data[8]);
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
