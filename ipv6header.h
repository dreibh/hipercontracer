// NorNet Trace Service
// Copyright (C) 2015 by Thomas Dreibholz
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

   inline unsigned char  version()       const { return((data[0] >> 4) & 0x0f);             }
   inline unsigned char  trafficClass()  const { return((data[1] & 0x0f) | (data[2] >> 4)); }
   inline unsigned int   flowLabel()     const { return( (((unsigned int)data[2] & 0x0f) << 16) | ((unsigned int)data[3] << 8) | (unsigned int)data[4] ); }
   inline unsigned short payloadLength() const { return(decode(4, 5)); }
   inline unsigned char  nextHeader()    const { return data[6]; }
   inline unsigned int   timeToLive()    const { return data[7]; }

   inline boost::asio::ip::address_v4 sourceAddress() const {
      const boost::asio::ip::address_v4::bytes_type bytes = { { data[12], data[13], data[14], data[15] } };
      return(boost::asio::ip::address_v4(bytes));
    }

   inline boost::asio::ip::address_v4 destinationAddress() const {
      const boost::asio::ip::address_v4::bytes_type bytes = { { data[16], data[17], data[18], data[19] } };
      return(boost::asio::ip::address_v4(bytes));
   }

   friend std::istream& operator>>(std::istream& is, IPv6Header& header) {
      is.read(reinterpret_cast<char*>(header.data), 40);
      if (header.version() != 6) {
         printf("VERSION=%d\n",header.version());
         is.setstate(std::ios::failbit);
      }
      else   puts("is v6version!");
      return(is);
   }

   private:
   unsigned short decode(int a, int b) const { return((data[a] << 8) + data[b]); }
   unsigned char data[40];
};

#endif
