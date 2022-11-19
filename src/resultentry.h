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

#ifndef RESULTENTRY_H
#define RESULTENTRY_H

#include "destinationinfo.h"

#include <chrono>


enum HopStatus {
   // ====== Status byte ==================================
   Unknown                 = 0,

   // ====== ICMP responses (from routers) ================
   // NOTE: Status values from 1 to 199 have a given
   //       router IP in their HopIP result!

   // ------ TTL/Hop Count --------------------------------
   TimeExceeded            = 1,     // ICMP response
   // ------ Reported as "unreachable" --------------------
   // NOTE: Status values from 100 to 199 denote unreachability
   UnreachableScope        = 100,   // ICMP response
   UnreachableNetwork      = 101,   // ICMP response
   UnreachableHost         = 102,   // ICMP response
   UnreachableProtocol     = 103,   // ICMP response
   UnreachablePort         = 104,   // ICMP response
   UnreachableProhibited   = 105,   // ICMP response
   UnreachableUnknown      = 110,   // ICMP response

   // ====== No response  =================================
   // NOTE: Status values from 200 to 254 have the destination
   //       IP in their HopIP field. However, there is no response!
   Timeout                 = 200,

   // ====== Destination's response (from destination) ====
   // NOTE: Successful response, destination is in HopIP field.
   Success                 = 255,   // Success!

   // ------ Response received ----------------------------
   Flag_StarredRoute       = (1 << 8),  // Route with * (router did not respond)
   Flag_DestinationReached = (1 << 9)   // Destination has responded
};


// ###### Is destination not reachable? #####################################
inline bool statusIsUnreachable(const HopStatus hopStatus)
{
   // Values 100 to 199 => the destination cannot be reached any more, since
   // a router on the way reported unreachability.
   return( (hopStatus >= UnreachableScope) &&
           (hopStatus < Timeout) );
}


class ResultEntry {
   public:
   ResultEntry(const unsigned short                         round,
               const unsigned short                         seqNumber,
               const unsigned int                           hop,
               const unsigned int                           packetSize,
               const uint16_t                               checksum,
               const std::chrono::system_clock::time_point& sendTime,
               const DestinationInfo&                       destination,
               const HopStatus                              status);
   ~ResultEntry();

   inline unsigned int round()                                const { return(Round);                  }
   inline unsigned int seqNumber()                            const { return(SeqNumber);              }
   inline unsigned int hop()                                  const { return(Hop);                    }
   inline unsigned int packetSize()                           const { return(PacketSize);             }
   const DestinationInfo& destination()                       const { return(Destination);            }
   const boost::asio::ip::address& destinationAddress()       const { return(Destination.address());  }
   inline HopStatus status()                                  const { return(Status);                 }
   inline uint16_t checksum()                                 const { return(Checksum);               }
   inline std::chrono::system_clock::time_point sendTime()    const { return(SendTime);               }
   inline std::chrono::system_clock::time_point receiveTime() const { return(ReceiveTime);            }
   inline std::chrono::system_clock::duration   rtt()         const { return(ReceiveTime - SendTime); }

   inline void setDestination(const DestinationInfo& destination)                      { Destination = destination;       }
   inline void setDestinationAddress(const boost::asio::ip::address& address)          { Destination.setAddress(address); }
   inline void setStatus(const HopStatus status)                                       { Status      = status;            }
   inline void setReceiveTime(const std::chrono::system_clock::time_point receiveTime) { ReceiveTime = receiveTime;       }

   inline friend bool operator<(const ResultEntry& resultEntry1, const ResultEntry& resultEntry2) {
      return(resultEntry1.SeqNumber < resultEntry2.SeqNumber);
   }
   friend std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry);

   private:
   const unsigned int                          Round;
   const unsigned short                        SeqNumber;
   const unsigned int                          Hop;
   const unsigned int                          PacketSize;
   const uint16_t                              Checksum;
   const std::chrono::system_clock::time_point SendTime;

   DestinationInfo                             Destination;
   HopStatus                                   Status;
   std::chrono::system_clock::time_point       ReceiveTime;
};

#endif
