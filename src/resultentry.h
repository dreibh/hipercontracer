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


enum ProtocolType
{
   PT_ICMP = 'I',
   PT_UDP  = 'U',
   PT_TCP  = 'T'
};


enum TimeSourceType
{
   TST_Unknown         = 0,
   TST_SysClock        = 1,
   TST_TIMESTAMP       = 2,
   TST_TIMESTAMPNS     = 3,
   TST_SIOCGSTAMP      = 4,
   TST_SIOCGSTAMPNS    = 5,
   TST_TIMESTAMPING_SW = 8,
   TST_TIMESTAMPING_HW = 9
};

enum TXTimeStampType
{
   TXTST_Application    = 0,   /* User-space application                               */
   TXTST_TransmissionSW = 1,   /* Software Transmission (SOF_TIMESTAMPING_TX_SOFTWARE) */
   TXTST_TransmissionHW = 2,   /* Hardware Transmission (SOF_TIMESTAMPING_TX_HARDWARE) */
   TXTST_SchedulerSW    = 3,   /* Kernel scheduler (SOF_TIMESTAMPING_TX_SCHED)         */
   TXTST_MAX = TXTST_SchedulerSW
};

enum RXTimeStampType
{
   /* Same numbering as TXTimeStampType, for corresponding RTT computation! */
   RXTST_Application = TXTimeStampType::TXTST_Application,      /* User-space application                            */
   RXTST_ReceptionSW = TXTimeStampType::TXTST_TransmissionSW,   /* Software Reception (SOF_TIMESTAMPING_TX_SOFTWARE) */
   RXTST_ReceptionHW = TXTimeStampType::TXTST_TransmissionHW,   /* Hardware Reception (SOF_TIMESTAMPING_RX_HARDWARE) */
   RXTST_MAX = RXTST_ReceptionHW
};


class ResultEntry {
   public:
   ResultEntry(const uint32_t                                        timeStampSeqID,
               const unsigned short                                  round,
               const unsigned short                                  seqNumber,
               const unsigned int                                    hop,
               const unsigned int                                    packetSize,
               const uint16_t                                        checksum,
               const std::chrono::high_resolution_clock::time_point& sendTime,
               const boost::asio::ip::address&                       source,
               const DestinationInfo&                                destination,
               const HopStatus                                       status);
   ~ResultEntry();

   inline uint32_t     timeStampSeqID()                 const { return(TimeStampSeqID);         }
   inline unsigned int round()                          const { return(Round);                  }
   inline unsigned int seqNumber()                      const { return(SeqNumber);              }
   inline unsigned int hop()                            const { return(Hop);                    }
   inline unsigned int packetSize()                     const { return(PacketSize);             }
   const DestinationInfo& destination()                 const { return(Destination);            }
   const boost::asio::ip::address& sourceAddress()      const { return(Source);                 }
   const boost::asio::ip::address& destinationAddress() const { return(Destination.address());  }
   inline HopStatus status()                            const { return(Status);                 }
   inline uint16_t checksum()                           const { return(Checksum);               }

   inline void setDestination(const DestinationInfo& destination)             { Destination = destination;       }
   inline void setDestinationAddress(const boost::asio::ip::address& address) { Destination.setAddress(address); }
   inline void setStatus(const HopStatus status)                              { Status      = status;            }

   inline std::chrono::high_resolution_clock::time_point sendTime(const TXTimeStampType txTimeStampType)    const { return(SendTime[txTimeStampType]);    }
   inline std::chrono::high_resolution_clock::time_point receiveTime(const RXTimeStampType rxTimeStampType) const { return(ReceiveTime[rxTimeStampType]); }

   inline void setSendTime(const TXTimeStampType                                 txTimeStampType,
                           const TimeSourceType                                  txTimeSource,
                           const std::chrono::high_resolution_clock::time_point& txTime) {
      assert((unsigned int)txTimeStampType <= TXTimeStampType::TXTST_MAX);
      SendTimeSource[txTimeStampType] = txTimeSource;
      SendTime[txTimeStampType]       = txTime;
   }

   inline void setReceiveTime(const RXTimeStampType                                 rxTimeStampType,
                              const TimeSourceType                                  rxTimeSource,
                              const std::chrono::high_resolution_clock::time_point& rxTime) {
      assert((unsigned int)rxTimeStampType <= RXTimeStampType::RXTST_MAX);
      ReceiveTimeSource[rxTimeStampType] = rxTimeSource;
      ReceiveTime[rxTimeStampType]       = rxTime;
   }

   inline std::chrono::high_resolution_clock::duration rtt(const RXTimeStampType rxTimeStampType) const {
      assert((unsigned int)rxTimeStampType <= RXTimeStampType::RXTST_MAX);
      // NOTE: Indexing for both arrays (RX, TX) is the same!
      if( (ReceiveTime[rxTimeStampType] == std::chrono::high_resolution_clock::time_point())  ||
          (SendTime[rxTimeStampType]    == std::chrono::high_resolution_clock::time_point()) ) {
         // At least one value is missing -> return "invalid" duration.
         return std::chrono::high_resolution_clock::duration::min();
      }
      return(ReceiveTime[rxTimeStampType] - SendTime[rxTimeStampType]);
   }

   inline std::chrono::high_resolution_clock::duration queuingDelay() const {
      if( (SendTime[TXTST_TransmissionSW] == std::chrono::high_resolution_clock::time_point())  ||
          (SendTime[TXTST_SchedulerSW]    == std::chrono::high_resolution_clock::time_point()) ) {
         // At least one value is missing -> return "invalid" duration.
         return std::chrono::high_resolution_clock::duration::min();
      }
      return(SendTime[TXTST_TransmissionSW] - SendTime[TXTST_SchedulerSW]);
   }

   inline friend bool operator<(const ResultEntry& resultEntry1, const ResultEntry& resultEntry2) {
      return(resultEntry1.SeqNumber < resultEntry2.SeqNumber);
   }
   friend std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry);

   private:
   const uint32_t                                 TimeStampSeqID;   /* Used with SOF_TIMESTAMPING_OPT_ID */
   const unsigned int                             Round;
   const unsigned short                           SeqNumber;
   const unsigned int                             Hop;
   const unsigned int                             PacketSize;
   const uint16_t                                 Checksum;

   boost::asio::ip::address                       Source;
   DestinationInfo                                Destination;
   HopStatus                                      Status;
   TimeSourceType                                 ReceiveTimeSource[RXTimeStampType::RXTST_MAX + 1];
   std::chrono::high_resolution_clock::time_point ReceiveTime[RXTimeStampType::RXTST_MAX + 1];
   TimeSourceType                                 SendTimeSource[TXTimeStampType::TXTST_MAX + 1];
   std::chrono::high_resolution_clock::time_point SendTime[TXTimeStampType::TXTST_MAX + 1];
};

#endif
