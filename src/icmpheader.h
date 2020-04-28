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

#ifndef ICMPHEADER_H
#define ICMPHEADER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>

#include <istream>
#include <algorithm>


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
   enum {
      IPv4EchoRequest           = ICMP_ECHO,
      IPv4EchoReply             = ICMP_ECHOREPLY,
      IPv4TimeExceeded          = ICMP_TIMXCEED,
      IPv4Unreachable           = ICMP_UNREACH,

      IPv6EchoRequest           = ICMP6_ECHO_REQUEST,
      IPv6EchoReply             = ICMP6_ECHO_REPLY,
      IPv6TimeExceeded          = ICMP6_TIME_EXCEEDED,
      IPv6Unreachable           = ICMP6_DST_UNREACH,

      IPv6NeighborSolicitation  = ND_NEIGHBOR_SOLICIT,
      IPv6NeighborAdvertisement = ND_NEIGHBOR_ADVERT,
      IPv6RouterSolicitation    = ND_ROUTER_SOLICIT,
      IPv6RouterAdvertisement   = ND_ROUTER_ADVERT
   };

   ICMPHeader() {
      std::fill(Data, Data + sizeof(Data), 0);
   }
   ICMPHeader(const char* inputData, const size_t length) {
      memcpy(Data, inputData, std::min(length, (size_t)8));
   }

   inline unsigned char type()        const { return Data[0];      }
   inline unsigned char code()        const { return Data[1];      }
   inline unsigned short checksum()   const { return decode(2, 3); }
   inline unsigned short identifier() const { return decode(4, 5); }
   inline unsigned short seqNumber()  const { return decode(6, 7); }

   inline void type(unsigned char type)          { Data[0] = type;         }
   inline void code(unsigned char code)          { Data[1] = code;         }
   inline void checksum(unsigned short checksum) { encode(2, 3, checksum); }
   inline void identifier(unsigned short id)     { encode(4, 5, id);       }
   inline void seqNumber(unsigned short seqNum)  { encode(6, 7, seqNum);   }

   inline friend std::istream& operator>>(std::istream& is, ICMPHeader& header) {
      return(is.read(reinterpret_cast<char*>(header.Data), 8));
   }

   inline friend std::ostream& operator<<(std::ostream& os, const ICMPHeader& header) {
      return(os.write(reinterpret_cast<const char*>(header.Data), 8));
   }

   private:
   inline unsigned short decode(int a, int b) const {
      return((Data[a] << 8) + Data[b]);
   }

   inline void encode(int a, int b, unsigned short n) {
      Data[a] = static_cast<unsigned char>(n >> 8);
      Data[b] = static_cast<unsigned char>(n & 0xFF);
   }

   unsigned char Data[8];
};


// Internet-16 checksum according to RFC 1071
template <typename Iterator> void computeInternet16(ICMPHeader& header, Iterator bodyBegin, Iterator bodyEnd)
{
   uint32_t sum =
      (header.type() << 8) +
      header.code() +
      header.identifier() +
      header.seqNumber();

   Iterator body_iter = bodyBegin;
   while (body_iter != bodyEnd) {
      sum += (static_cast<unsigned char>(*body_iter++) << 8);
      if (body_iter != bodyEnd) {
         sum += static_cast<unsigned char>(*body_iter++);
      }
   }

   sum = (sum >> 16) + (sum & 0xFFFF);
   sum += (sum >> 16);
   header.checksum(static_cast<uint16_t>(~sum));
}

#endif
