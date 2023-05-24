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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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

#ifndef TCPHEADER_H
#define TCPHEADER_H

#include <istream>
#include <ostream>

#include "internet16.h"


// ==========================================================================
// From RFC 793:
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |          Source Port          |       Destination Port        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                        Sequence Number                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                    Acknowledgment Number                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |  Data |           |U|A|P|R|S|F|                               |
//    | Offset| Reserved  |R|C|S|S|Y|I|            Window             |
//    |       |           |G|K|H|T|N|N|                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |           Checksum            |         Urgent Pointer        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                    Options                    |    Padding    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                             data                              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

//
// ==========================================================================


enum TCPFlags
{
   TF_FIN = (1 << 0),
   TF_SYN = (1 << 1),
   TF_RST = (1 << 2),
   TF_PSH = (1 << 3),
   TF_ACK = (1 << 4),
   TF_URG = (1 << 5),
   TF_ECE = (1 << 6),
   TF_CWR = (1 << 7)
};

class TCPHeader
{
   public:
   TCPHeader() { }

   inline uint16_t sourcePort()      const { return ntohs(*((uint16_t*)&Data[0]));  }
   inline uint16_t destinationPort() const { return ntohs(*((uint16_t*)&Data[2]));  }
   inline uint16_t seqNumber()       const { return ntohl(*((uint32_t*)&Data[4]));  }
   inline uint16_t ackNumber()       const { return ntohl(*((uint32_t*)&Data[8]));  }
   inline uint8_t dataOffset()       const { return (Data[12] & 0xf0) >> 2;         }   /* converted to bytes (*4)! */
   inline TCPFlags flags()           const { return (TCPFlags)Data[13];             }
   inline uint16_t window()          const { return ntohs(*((uint16_t*)&Data[14])); }
   inline uint16_t checksum()        const { return ntohs(*((uint16_t*)&Data[16])); }
   inline uint16_t urgentPointer()   const { return ntohs(*((uint16_t*)&Data[18])); }

   inline void sourcePort(const uint16_t sourcePort)           { *((uint16_t*)&Data[0])  = htons(sourcePort);      }
   inline void destinationPort(const uint16_t destinationPort) { *((uint16_t*)&Data[2])  = htons(destinationPort); }
   inline void seqNumber(const uint32_t seqNumber)             { *((uint32_t*)&Data[4])  = htonl(seqNumber);       }
   inline void ackNumber(const uint32_t ackNumber)             { *((uint32_t*)&Data[8])  = htonl(ackNumber);       }
   inline void dataOffset(const uint8_t dataOffset)            { Data[12] = ((dataOffset >> 2) & 0x0f) << 4;       }   /* in bytes! */
   inline void flags(const TCPFlags flags)                     { Data[13] = (uint8_t)flags;                        }
   inline void window(const uint16_t window)                   { *((uint16_t*)&Data[14]) = htons(window);          }
   inline void checksum(const uint16_t checksum)               { *((uint16_t*)&Data[16]) = htons(checksum);        }
   inline void urgentPointer(const uint16_t urgentPointer)     { *((uint16_t*)&Data[18]) = htons(urgentPointer);   }

   inline void computeInternet16(uint32_t& sum) const {
      ::computeInternet16(sum, (uint8_t*)&Data, dataOffset());
   }

   inline const uint8_t* data() const {
      return (const uint8_t*)&Data;
   }
   inline size_t size() const {
      return dataOffset();
   }

   friend std::istream& operator>>(std::istream& is, TCPHeader& header) {
      is.read(reinterpret_cast<char*>(header.Data), 20);
      std::streamsize totalLength = header.dataOffset();
      if(totalLength < 20) {
         is.setstate(std::ios::failbit);
      }
      return is;
   }

   inline friend std::ostream& operator<<(std::ostream& os, const TCPHeader& header) {
      return os.write(reinterpret_cast<const char*>(header.Data), header.dataOffset());
   }

   private:
   uint8_t Data[60];
};

#endif
