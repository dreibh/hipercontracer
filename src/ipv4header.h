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

#ifndef IPV4HEADER_H
#define IPV4HEADER_H

#include <istream>
#include <algorithm>
#include <boost/asio/ip/address_v4.hpp>


// ==========================================================================
// From RFC 791:
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |Version|  IHL  |Type of Service|          Total Length         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |         Identification        |Flags|      Fragment Offset    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |  Time to Live |    Protocol   |         Header Checksum       |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                       Source Address                          |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                    Destination Address                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                    Options                    |    Padding    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// ==========================================================================

class IPv4Header
{
   public:
   IPv4Header() {
      std::fill(data, data + sizeof(data), 0);
   }

   inline uint8_t version()         const { return((data[0] >> 4) & 0x0f); }
   inline uint16_t headerLength()   const { return((data[0] & 0x0f) * 4);  }
   inline uint8_t typeOfService()   const { return(data[1]);      }
   inline uint16_t totalLength()    const { return(decode(2, 3)); }
   inline uint16_t identification() const { return(decode(4, 5)); }
   inline bool dontFragment()             const { return((data[6] & 0x40) != 0); }
   inline bool moreFragments()            const { return((data[6] & 0x20) != 0); }
   inline uint16_t fragmentOffset() const { return(decode(6, 7) & 0x1fff); }
   inline unsigned int timeToLive()       const { return(data[8]); }
   inline uint8_t protocol()        const { return(data[9]); }
   inline uint16_t headerChecksum() const { return(decode(10, 11)); }

   inline boost::asio::ip::address_v4 sourceAddress() const {
      const boost::asio::ip::address_v4::bytes_type bytes = { { data[12], data[13], data[14], data[15] } };
      return(boost::asio::ip::address_v4(bytes));
    }

   inline boost::asio::ip::address_v4 destinationAddress() const {
      const boost::asio::ip::address_v4::bytes_type bytes = { { data[16], data[17], data[18], data[19] } };
      return(boost::asio::ip::address_v4(bytes));
   }

   friend std::istream& operator>>(std::istream& is, IPv4Header& header) {
      is.read(reinterpret_cast<char*>(header.data), 20);
      if (header.version() != 4) {
         is.setstate(std::ios::failbit);
      }
      std::streamsize options_length = header.headerLength() - 20;
      if (options_length < 0 || options_length > 40) {
         is.setstate(std::ios::failbit);
      }
      else {
         is.read(reinterpret_cast<char*>(header.data) + 20, options_length);
      }
      return(is);
   }

   private:
   uint16_t decode(const uint8_t a, const uint8_t b) const { return((data[a] << 8) + data[b]); }
   uint8_t  data[20 + 40];
};

#endif
