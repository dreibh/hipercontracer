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
      std::fill(Data, Data + sizeof(Data), 0);
   }

   inline uint8_t  version()        const { return (Data[0] >> 4) & 0x0f; }
   inline uint16_t headerLength()   const { return (Data[0] & 0x0f) * 4;  }
   inline uint8_t  typeOfService()  const { return Data[1];               }
   inline uint16_t totalLength()    const { return decode(2, 3);          }
   inline uint16_t identification() const { return decode(4, 5);          }
   inline bool     dontFragment()   const { return (Data[6] & 0x40) != 0; }
   inline bool     moreFragments()  const { return (Data[6] & 0x20) != 0; }
   inline uint16_t fragmentOffset() const { return decode(6, 7) & 0x1fff; }
   inline uint8_t  timeToLive()     const { return Data[8];               }
   inline uint8_t  protocol()       const { return Data[9];               }
   inline uint16_t headerChecksum() const { return decode(10, 11);        }

   inline boost::asio::ip::address_v4 sourceAddress() const {
      const boost::asio::ip::address_v4::bytes_type bytes =
         { { Data[12], Data[13], Data[14], Data[15] } };
      return boost::asio::ip::address_v4(bytes);
    }
   inline boost::asio::ip::address_v4 destinationAddress() const {
      const boost::asio::ip::address_v4::bytes_type bytes =
         { { Data[16], Data[17], Data[18], Data[19] } };
      return boost::asio::ip::address_v4(bytes);
   }

   inline void version(const uint8_t version)                { Data[0] = (version << 4) | (Data[0] & 0x0f);               }
   inline void headerLength(const uint8_t headerLength)      { Data[0] = (Data[0] & 0xf0) | ((headerLength >> 2) & 0x0f); }
   inline void typeOfService(const uint8_t typeOfService)    { Data[1] = typeOfService;                                   }
   inline void totalLength(const uint16_t totalLength)       { encode(2, 3, totalLength);                                 }
   inline void identification(const uint16_t identification) { encode(4, 5, identification);                              }
   inline void fragmentOffset(const uint16_t fragmentOffset) { encode(6, 7, fragmentOffset);                              }
   inline void timeToLive(const uint8_t timeToLive)          { Data[8] = timeToLive;                                      }
   inline void protocol(const uint8_t protocol)              { Data[9] = protocol;                                        }
   inline void headerChecksum(const uint16_t headerChecksum) { encode(10, 11, headerChecksum);                            }

   inline void sourceAddress(const boost::asio::ip::address_v4& sourceAddress) {
      memcpy(&Data[12], sourceAddress.to_bytes().data(), 4);
   }
   inline void destinationAddress(const boost::asio::ip::address_v4& destinationAddress) {
      memcpy(&Data[16], destinationAddress.to_bytes().data(), 4);
   }

   friend std::istream& operator>>(std::istream& is, IPv4Header& header) {
      is.read(reinterpret_cast<char*>(header.Data), 20);
      if (header.version() != 4) {
         is.setstate(std::ios::failbit);
      }
      std::streamsize options_length = header.headerLength() - 20;
      if (options_length < 0 || options_length > 40) {
         is.setstate(std::ios::failbit);
      }
      else {
         is.read(reinterpret_cast<char*>(header.Data) + 20, options_length);
      }
      return is;
   }

   inline friend std::ostream& operator<<(std::ostream& os, const IPv4Header& header) {
      return os.write(reinterpret_cast<const char*>(header.Data), header.headerLength());
   }

   inline std::vector<uint8_t> contents() const {
      return std::vector<uint8_t>((uint8_t*)&Data, (uint8_t*)&Data[headerLength()]);
   }

   private:
   friend class IPv4PseudoHeader;

   inline uint16_t decode(const unsigned int a, const unsigned int b) const {
      return ((uint16_t)Data[a] << 8) + Data[b];
   }

   inline void encode(const unsigned int a, const unsigned int b, const uint16_t n) {
      Data[a] = static_cast<uint8_t>(n >> 8);
      Data[b] = static_cast<uint8_t>(n & 0xff);
   }

   uint8_t Data[20 + 40];
};


class IPv4PseudoHeader
{
   public:
   IPv4PseudoHeader(const IPv4Header& ipv4Header, const uint16_t length) {
      memcpy(&Data[0], &ipv4Header.Data[12], 8);      // Source and Destination Address
      Data[8] = 0x00;                                 // Padding
      Data[9] = ipv4Header.Data[9];                   // Protocol
      Data[10] = static_cast<uint8_t>(length >> 8);   // Length (Transport)
      Data[11] = static_cast<uint8_t>(length & 0xff);
   }

   inline std::vector<uint8_t> contents() const {
      return std::vector<uint8_t>((uint8_t*)&Data, (uint8_t*)&Data[12]);
   }

   private:
   uint8_t Data[12];
};

#endif
