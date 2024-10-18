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

#ifndef IPV6HEADER_H
#define IPV6HEADER_H

#include <istream>
#include <ostream>
#include <boost/asio/ip/address_v6.hpp>

#include "internet16.h"


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
      Data[1] = 0x00;   // Avoid warning about uninitialised usage
   }

   inline uint8_t  version()       const { return (Data[0] >> 4) & 0x0f;                             }
   inline uint8_t  trafficClass()  const { return ((Data[0] & 0x0f) << 4) | ((Data[1] >> 4) & 0x0f); }
   inline uint32_t flowLabel()     const {
      return (((uint32_t)Data[1] & 0x0f) << 16) | ((uint32_t)Data[2] << 8) | (uint32_t)Data[3];
   }
   inline uint16_t payloadLength() const { return decode(4, 5); }
   inline uint8_t  nextHeader()    const { return Data[6];      }
   inline uint32_t hopLimit()      const { return Data[7];      }

   inline boost::asio::ip::address_v6 sourceAddress() const {
      boost::asio::ip::address_v6::bytes_type v6address;
      for(size_t i = 0;i < 16; i++) {
         v6address[i] = Data[8 + i];
      }
      const boost::asio::ip::address_v6 address = boost::asio::ip::address_v6(v6address, 0);
      return address;
   }

   inline boost::asio::ip::address_v6 destinationAddress() const {
      boost::asio::ip::address_v6::bytes_type v6address;
      for(size_t i = 0;i < 16; i++) {
         v6address[i] = Data[24 + i];
      }
      const boost::asio::ip::address_v6 address = boost::asio::ip::address_v6(v6address, 0);
      return address;
   }

   inline void version(const uint8_t version)              { Data[0] = ((version & 0x0f) << 4) | (Data[0] & 0x0f); }
   inline void trafficClass(const uint8_t trafficClass)    {
      Data[0] = (Data[0] & 0xf0) | ((trafficClass & 0xf0) >> 4);
      Data[1] = (Data[1] & 0x0f) | ((trafficClass & 0x0f) << 4);
   }
   inline void flowLabel(const uint32_t flowlabel) {
      Data[1] = (Data[1] & 0xf0) | ((flowlabel & 0x000f0000) >> 16);
      Data[2] = (flowlabel & 0x0000ff00) >> 8;
      Data[3] = (flowlabel & 0x000000ff);
   }
   inline void payloadLength(const uint16_t payloadLength) { encode(4, 5, payloadLength); }
   inline void hopLimit(const uint8_t hopLimit)            { Data[7] = hopLimit;          }
   inline void nextHeader(const uint8_t nextHeader)        { Data[6] = nextHeader;        }

   inline void sourceAddress(const boost::asio::ip::address_v6& sourceAddress) {
      memcpy(&Data[8], sourceAddress.to_bytes().data(), 16);
   }
   inline void destinationAddress(const boost::asio::ip::address_v6& destinationAddress) {
      memcpy(&Data[24], destinationAddress.to_bytes().data(), 16);
   }

   inline const uint8_t* data() const {
      return (const uint8_t*)&Data;
   }
   inline size_t size() const {
      return sizeof(Data);
   }

   friend std::istream& operator>>(std::istream& is, IPv6Header& header) {
      is.read(reinterpret_cast<char*>(header.Data), 40);
      if (header.version() != 6) {
         is.setstate(std::ios::failbit);
      }
      return is;
   }

   inline friend std::ostream& operator<<(std::ostream& os, const IPv6Header& header) {
      return os.write(reinterpret_cast<const char*>(&header.Data), sizeof(header.Data));
   }

   private:
   friend class IPv6PseudoHeader;

   inline uint16_t decode(const unsigned int a, const unsigned int b) const {
      return ((uint16_t)Data[a] << 8) + Data[b];
   }

   inline void encode(const unsigned int a, const unsigned int b, const uint16_t n) {
      Data[a] = static_cast<uint8_t>(n >> 8);
      Data[b] = static_cast<uint8_t>(n & 0xff);
   }

   uint8_t Data[40];
};


class IPv6PseudoHeader
{
   public:
   IPv6PseudoHeader() { }
   IPv6PseudoHeader(const IPv6Header& ipv6Header, const uint32_t length) {
      memcpy(&Data[0], &ipv6Header.Data[8], 32);   // Source and Destination Address
      // Length (Transport):
      Data[32] = static_cast<uint8_t>((length & 0xff000000) >> 24);
      Data[33] = static_cast<uint8_t>((length & 0x00ff0000) >> 16);
      Data[34] = static_cast<uint8_t>((length & 0x0000ff00) >> 8);
      Data[35] = static_cast<uint8_t>(length & 0x000000ff);
      // Padding:
      Data[36] = 0x00;
      Data[37] = 0x00;
      Data[38] = 0x00;
      Data[39] = ipv6Header.Data[6];                   // Protocol
   }

   inline void computeInternet16(uint32_t& sum) const {
      ::computeInternet16(sum, (uint8_t*)&Data, sizeof(Data));
   }

   private:
   uint8_t Data[40];
};

#endif
