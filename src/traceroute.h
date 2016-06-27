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
// Copyright (C) 2015-2016 by Thomas Dreibholz
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

#ifndef TRACEROUTE_H
#define TRACEROUTE_H

#include "service.h"
#include "icmpheader.h"
#include "sqlwriter.h"

#include <set>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


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
   ResultEntry(const unsigned short           round,
               const unsigned short           seqNumber,
               const unsigned int             hop,
               boost::asio::ip::address       address,
               const HopStatus                status,
               const boost::posix_time::ptime sendTime);

   inline unsigned int round()                   const { return(Round);       }
   inline unsigned int seqNumber()               const { return(SeqNumber);   }
   inline unsigned int hop()                     const { return(Hop);         }
   const boost::asio::ip::address& address()     const { return(Address);     }
   inline HopStatus status()                     const { return(Status);      }
   inline boost::posix_time::ptime sendTime()    const { return(SendTime);    }
   inline boost::posix_time::ptime receiveTime() const { return(ReceiveTime); }
   inline boost::posix_time::time_duration rtt() const { return(ReceiveTime - SendTime); }

   inline void address(const boost::asio::ip::address address)         { Address = address;         }
   inline void status(const HopStatus status)                          { Status = status;           }
   inline void receiveTime(const boost::posix_time::ptime receiveTime) { ReceiveTime = receiveTime; }

   inline friend bool operator<(const ResultEntry& resultEntry1, const ResultEntry& resultEntry2) {
      return(resultEntry1.SeqNumber < resultEntry2.SeqNumber);
   }
   friend std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry);

   private:
   const unsigned int             Round;
   const unsigned short           SeqNumber;
   const unsigned int             Hop;
   boost::asio::ip::address       Address;
   HopStatus                      Status;
   const boost::posix_time::ptime SendTime;
   boost::posix_time::ptime       ReceiveTime;
};


class Traceroute : virtual public Service
{
   public:
   Traceroute(SQLWriter*                               sqlWriter,
              const bool                               verboseMode,
              const boost::asio::ip::address&          sourceAddress,
              const std::set<boost::asio::ip::address> destinationAddressArray,
              const unsigned long long                 interval        = 30*60000ULL,
              const unsigned int                       expiration      = 3000,
              const unsigned int                       rounds          = 1,
              const unsigned int                       initialMaxTTL   = 5,
              const unsigned int                       finalMaxTTL     = 35,
              const unsigned int                       incrementMaxTTL = 2);
   virtual ~Traceroute();

   virtual bool start();
   virtual void requestStop();
   virtual void join();

   inline bool isIPv6() const {
      return(SourceAddress.is_v6());
   }

   protected:
   virtual bool prepareSocket();
   virtual bool prepareRun(const bool newRound = false);
   virtual void scheduleTimeoutEvent();
   virtual void scheduleIntervalEvent();
   virtual void expectNextReply();
   virtual void noMoreOutstandingRequests();
   virtual bool notReachedWithCurrentTTL();
   virtual void processResults();
   virtual void sendRequests();
   virtual void handleTimeoutEvent(const boost::system::error_code& errorCode);
   virtual void handleIntervalEvent(const boost::system::error_code& errorCode);
   virtual void handleMessage(std::size_t length);

   void cancelTimeoutTimer();
   void cancelIntervalTimer();

   void run();
   void sendICMPRequest(const boost::asio::ip::address& destinationAddress,
                        const unsigned int              ttl,
                        const unsigned int              round,
                        uint32_t&                       targetChecksum);
   void recordResult(const boost::posix_time::ptime& receiveTime,
                     const ICMPHeader&               icmpHeader,
                     const unsigned short            seqNumber);
   unsigned int getInitialMaxTTL(const boost::asio::ip::address& destinationAddress) const;
   static unsigned long long ptimeToMircoTime(const boost::posix_time::ptime t);

   SQLWriter*                            SQLOutput;
   const bool                            VerboseMode;
   const unsigned long long              Interval;
   const unsigned int                    Expiration;
   const unsigned int                    Rounds;
   const unsigned int                    InitialMaxTTL;
   const unsigned int                    FinalMaxTTL;
   const unsigned int                    IncrementMaxTTL;
   boost::asio::io_service               IOService;
   boost::asio::ip::address              SourceAddress;
   std::set<boost::asio::ip::address>    DestinationAddressArray;
   boost::asio::ip::icmp::socket         ICMPSocket;
   boost::asio::deadline_timer           TimeoutTimer;
   boost::asio::deadline_timer           IntervalTimer;
   boost::asio::ip::icmp::endpoint       ReplyEndpoint;    // Store ICMP reply's source

   boost::thread                         Thread;
   volatile bool                         StopRequested;
   unsigned int                          Identifier;
   unsigned short                        SeqNumber;
   unsigned int                          MagicNumber;
   unsigned int                          OutstandingRequests;
   unsigned int                          LastHop;
   std::map<unsigned short, ResultEntry> ResultsMap;
   std::map<boost::asio::ip::address,
            unsigned int>                TTLCache;
   bool                                  ExpectingReply;
   char                                  MessageBuffer[65536 + 40];
   unsigned int                          MinTTL;
   unsigned int                          MaxTTL;
   boost::posix_time::ptime              RunStartTimeStamp;

   std::set<boost::asio::ip::address>::iterator DestinationAddressIterator;

   private:
   static int compareTracerouteResults(const ResultEntry* a, const ResultEntry* b);
};

#endif
