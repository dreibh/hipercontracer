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
// Copyright (C) 2015-2020 by Thomas Dreibholz
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

#ifndef IPV6HEADER_H
#define IPV6HEADER_H

#include <istream>
#include <algorithm>
#include <boost/asio/ip/address_v6.hpp>


// ==========================================================================
// From RFC 2460:
//
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |Version| Traffic Class |           Flow Label                  |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |         Payload Length        |  Next Header  |   Hop Limit   |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                                                               |
//    +                                                               +
//    |                                                               |
//    +                         Source Address                        +
//    |                                                               |
//    +                                                               +
//    |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                                                               |
//    +                                                               +
//    |                                                               |
//    +                      Destination Address                      +
//    |                                                               |
//    +                                                               +
//    |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// ==========================================================================

class IPv6Header
{
   public:
   IPv6Header() {
      std::fill(data, data + sizeof(data), 0);
   }

   inline uint8_t  version()       const { return((data[0] >> 4) & 0x0f);             }
   inline uint8_t  trafficClass()  const { return((data[1] & 0x0f) | (data[2] >> 4)); }
   inline uint32_t flowLabel()     const { return( (((uint32_t)data[2] & 0x0f) << 16) | ((uint32_t)data[3] << 8) | (uint32_t)data[4] ); }
   inline uint16_t payloadLength() const { return(decode(4, 5)); }
   inline uint8_t  nextHeader()    const { return data[6]; }
   inline uint32_t timeToLive()    const { return data[7]; }

   inline boost::asio::ip::address_v6 sourceAddress() const {
      boost::asio::ip::address_v6::bytes_type v6address;
      for(size_t i = 0;i < 16; i++) {
         v6address[i] = data[8 + i];
      }
      const boost::asio::ip::address_v6 address = boost::asio::ip::address_v6(v6address, 0);
      return(address);
   }

   inline boost::asio::ip::address_v6 destinationAddress() const {
      boost::asio::ip::address_v6::bytes_type v6address;
      for(size_t i = 0;i < 16; i++) {
         v6address[i] = data[24 + i];
      }
      const boost::asio::ip::address_v6 address = boost::asio::ip::address_v6(v6address, 0);
      return(address);
   }

   friend std::istream& operator>>(std::istream& is, IPv6Header& header) {
      is.read(reinterpret_cast<char*>(header.data), 40);
      if (header.version() != 6) {
         is.setstate(std::ios::failbit);
      }
      return(is);
   }

   private:
   uint16_t decode(const unsigned int a, const unsigned int b) const { return(((uint16_t)data[a] << 8) + data[b]); }
   uint8_t  data[40];
};

#endif
