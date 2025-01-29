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

#ifndef RESULTENTRY_H
#define RESULTENTRY_H

#include "destinationinfo.h"

#include <chrono>


enum HopStatus {
   // ====== Status byte ==================================
   Unknown                   = 0,

   // ====== ICMP responses (from routers) ================
   // NOTE: Status values from 1 to 199 have a given
   //       router IP in their HopIP result!

   // ------ TTL/Hop Count --------------------------------
   TimeExceeded              = 1,     // ICMP response

   // ------ Reported as "unreachable" --------------------
   // NOTE: Status values from 100 to 199 denote unreachability
   UnreachableScope          = 100,   // ICMP response
   UnreachableNetwork        = 101,   // ICMP response
   UnreachableHost           = 102,   // ICMP response
   UnreachableProtocol       = 103,   // ICMP response
   UnreachablePort           = 104,   // ICMP response
   UnreachableProhibited     = 105,   // ICMP response
   UnreachableUnknown        = 110,   // ICMP response

   // ====== No response  =================================
   // NOTE: Status values from 200 to 254 have the destination
   //       IP in their HopIP field. However, there is no response!
   Timeout                   = 200,

   NotSentGenericError       = 210,   // sendto() call failed (generic error)
   NotSentPermissionDenied   = 211,   // sendto() error: EACCES
   NotSentNetworkUnreachable = 212,   // sendto() error: ENETUNREACH
   NotSentHostUnreachable    = 213,   // sendto() error: EHOSTUNREACH
   NotAvailableAddress       = 214,   // sendto() error: EADDRNOTAVAIL
   NotValidMsgSize           = 215,   // sendto() error: EMSGSIZE
   NotEnoughBufferSpace      = 216,   // sendto() error: ENOBUFS

   // ====== Destination's response (from destination) ====
   // NOTE: Successful response, destination is in HopIP field.
   Success                   = 255,   // Success!

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


// ###### Is send error? ####################################################
inline bool statusIsSendError(const HopStatus hopStatus)
{
   // Values 200 to 254 => the packet cannot be sent by the sender
   return( (hopStatus > Timeout) &&
           (hopStatus < Success) );
}


const char* getStatusColor(const HopStatus hopStatus);
const char* getStatusName(const HopStatus hopStatus);


enum ProtocolType
{
   PT_ICMP = 'i',
   PT_UDP  = 'u',
   PT_TCP  = 't'
};

enum TimeSourceType
{
   TST_Unknown         = 0x0,

   // The following time stamp types are based on the system time:
   TST_SysClock        = 0x1,   // System clock
   TST_TIMESTAMP       = 0x2,   // SO_TIMESTAMPING option, microseconds granularity
   TST_TIMESTAMPNS     = 0x3,   // SO_TIMESTAMPINGNS option (or SO_TIMESTAMPING+SO_TS_CLOCK), nanoseconds granularity
   TST_SIOCGSTAMP      = 0x4,   // SIOCGSTAMP ioctl, microseconds granularity
   TST_SIOCGSTAMPNS    = 0x5,   // SIOCGSTAMPNS ioctl, nanoseconds granularity
   TST_TIMESTAMPING_SW = 0x6,   // SO_TIMESTAMPING option, software

   // The following time stamp type is raw, no assumption should be made on
   // relation to system time!
   TST_TIMESTAMPING_HW = 0xa    // SO_TIMESTAMPING option, hardware
};

enum TXTimeStampType
{
   TXTST_Application    = 0,   // User-space application
   TXTST_TransmissionSW = 1,   // Software Transmission (e.g. SOF_TIMESTAMPING_TX_SOFTWARE)
   TXTST_TransmissionHW = 2,   // Hardware Transmission (e.g. SOF_TIMESTAMPING_TX_HARDWARE)
   TXTST_SchedulerSW    = 3,   // Kernel scheduler (e.g. SOF_TIMESTAMPING_TX_SCHED)
   TXTST_MAX = TXTST_SchedulerSW
};

enum RXTimeStampType
{
   // Same numbering as TXTimeStampType, for corresponding RTT computation!
   RXTST_Application = TXTimeStampType::TXTST_Application,      // User-space application
   RXTST_ReceptionSW = TXTimeStampType::TXTST_TransmissionSW,   // Software Reception (SOF_TIMESTAMPING_TX_SOFTWARE)
   RXTST_ReceptionHW = TXTimeStampType::TXTST_TransmissionHW,   // Hardware Reception (SOF_TIMESTAMPING_RX_HARDWARE)
   RXTST_MAX = RXTST_ReceptionHW
};


typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemDuration;

#include <iostream>

typedef std::chrono::high_resolution_clock   ResultClock;
typedef ResultClock::time_point              ResultTimePoint;
typedef ResultClock::duration                ResultDuration;


class ResultEntry {
   public:
   ResultEntry();
   ~ResultEntry();

   void initialise(const uint32_t                  timeStampSeqID,
                   const unsigned short            roundNumber,
                   const unsigned short            seqNumber,
                   const unsigned int              hopNumber,
                   const unsigned int              packetSize,
                   const uint16_t                  checksum,
                   const uint16_t                  sourcePort,
                   const uint16_t                  destinationPort,
                   const ResultTimePoint&          sendTime,
                   const boost::asio::ip::address& source,
                   const DestinationInfo&          destination,
                   const HopStatus                 status);
   void expire(const unsigned int expiration);
   void failedToSend(const boost::system::error_code& errorCode);

   inline uint32_t     timeStampSeqID()                 const { return TimeStampSeqID;         }
   inline unsigned int roundNumber()                    const { return RoundNumber;            }
   inline unsigned int seqNumber()                      const { return SeqNumber;              }
   inline unsigned int hopNumber()                      const { return HopNumber;              }
   inline unsigned int packetSize()                     const { return PacketSize;             }
   inline unsigned int responseSize()                   const { return ResponseSize;           }
   inline uint16_t     checksum()                       const { return Checksum;               }
   inline uint16_t     sourcePort()                     const { return SourcePort;             }
   inline uint16_t     destinationPort()                const { return DestinationPort;        }

   const boost::asio::ip::address& sourceAddress()      const { return Source;                 }
   const DestinationInfo& destination()                 const { return Destination;            }
   const boost::asio::ip::address& destinationAddress() const { return Destination.address();  }
   const boost::asio::ip::address& hopAddress()         const { return Hop;                    }
   inline HopStatus status()                            const { return Status;                 }
   inline ResultTimePoint sendTime(const TXTimeStampType txTimeStampType)    const { return SendTime[txTimeStampType];    }
   inline ResultTimePoint receiveTime(const RXTimeStampType rxTimeStampType) const { return ReceiveTime[rxTimeStampType]; }

   void updateSourceAddress(const boost::asio::ip::address& sourceAddress);
   inline void setStatus(const HopStatus status)                      { Status       = status;               }
   inline void setResponseSize(const unsigned int responseSize)       { ResponseSize = responseSize;         }
   inline void setHopAddress(const boost::asio::ip::address& address) { Hop          = dropScopeID(address); }

   inline void setSendTime(const TXTimeStampType  txTimeStampType,
                           const TimeSourceType   txTimeSource,
                           const ResultTimePoint& txTime) {
      assert((unsigned int)txTimeStampType <= TXTimeStampType::TXTST_MAX);
      SendTimeSource[txTimeStampType] = txTimeSource;
      SendTime[txTimeStampType]       = txTime;
   }

   inline void setReceiveTime(const RXTimeStampType  rxTimeStampType,
                              const TimeSourceType   rxTimeSource,
                              const ResultTimePoint& rxTime) {
      assert((unsigned int)rxTimeStampType <= RXTimeStampType::RXTST_MAX);
      ReceiveTimeSource[rxTimeStampType] = rxTimeSource;
      ReceiveTime[rxTimeStampType]       = rxTime;
   }

   bool obtainSendReceiveTime(const RXTimeStampType rxTimeStampType,
                              unsigned int&         timeSource,
                              ResultTimePoint&      sendTime,
                              ResultTimePoint&      receiveTime) const;
   bool obtainSchedulingSendTime(unsigned int&    timeSource,
                                 ResultTimePoint& schedulingTime,
                                 ResultTimePoint& sendTime) const;
   bool obtainApplicationSendSchedulingTime(unsigned int&    timeSource,
                                            ResultTimePoint& appSendTime,
                                            ResultTimePoint& schedulingTime) const;
   bool obtainReceptionApplicationReceiveTime(unsigned int&    timeSource,
                                              ResultTimePoint& receiveTime,
                                              ResultTimePoint& appReceiveTime) const;

   ResultDuration getRTT(const RXTimeStampType rxTimeStampType,
                         unsigned int&         timeSource) const;
   ResultDuration getQueuingDelay(unsigned int& timeSource) const;
   ResultDuration getAppSendDelay(unsigned int& timeSource) const;
   ResultDuration getAppReceiveDelay(unsigned int& timeSource) const;

   void obtainResultsValues(unsigned int&   timeSource,
                            ResultDuration& rttApplication,
                            ResultDuration& rttSoftware,
                            ResultDuration& rttHardware,
                            ResultDuration& delayQueuing,
                            ResultDuration& delayAppSend,
                            ResultDuration& delayAppReceive) const;
   ResultDuration obtainMostAccurateRTT(const RXTimeStampType rxTimeStampType,
                                        unsigned int&         timeSource) const;

   inline friend bool operator<(const ResultEntry& resultEntry1, const ResultEntry& resultEntry2) {
      return resultEntry1.SeqNumber < resultEntry2.SeqNumber;
   }
   friend std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry);

   private:
   inline static boost::asio::ip::address dropScopeID(const boost::asio::ip::address& address) {
      if(address.is_v6()) {
         boost::asio::ip::address_v6 v6 = address.to_v6();
         v6.scope_id(0);
         return v6;
      }
      return address;
   }

   uint32_t                 TimeStampSeqID;   /* Used with SOF_TIMESTAMPING_OPT_ID */
   unsigned int             RoundNumber;
   unsigned short           SeqNumber;
   unsigned int             HopNumber;
   unsigned int             PacketSize;
   unsigned int             ResponseSize;
   uint16_t                 Checksum;
   uint16_t                 SourcePort;
   uint16_t                 DestinationPort;

   boost::asio::ip::address Source;
   DestinationInfo          Destination;
   boost::asio::ip::address Hop;
   HopStatus                Status;
   TimeSourceType           SendTimeSource[TXTimeStampType::TXTST_MAX + 1];
   ResultTimePoint          SendTime[TXTimeStampType::TXTST_MAX + 1];
   TimeSourceType           ReceiveTimeSource[RXTimeStampType::RXTST_MAX + 1];
   ResultTimePoint          ReceiveTime[RXTimeStampType::RXTST_MAX + 1];
};

#endif
