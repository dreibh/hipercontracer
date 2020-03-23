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
// Copyright (C) 2020 Alfred Arouna
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
// Contact: alfred@simula.no

#include "burstping.h"
#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"
#include "logger.h"

#include <iostream>

// ###### Constructor #######################################################
Burstping::Burstping(ResultsWriter*         resultsWriter,
           const unsigned int               iterations,
           const bool                       removeDestinationAfterRun,
           const boost::asio::ip::address&  sourceAddress,
           const std::set<DestinationInfo>& destinationArray,
           const unsigned long long         interval,
           const unsigned int               expiration,
           const unsigned int               ttl,
           const unsigned int               payload,
           const unsigned int               burst)
   :  payload(payload),
      burst(burst),
      Ping(resultsWriter, iterations, removeDestinationAfterRun,
                sourceAddress, destinationArray,
                interval, expiration, ttl),
      BurstpingInstanceName(std::string("Burstping(") + sourceAddress.to_string() + std::string(")"))


{
}

// ###### Destructor ########################################################
Burstping::~Burstping()
{
}


// ###### Start thread ######################################################
const std::string& Burstping::getName() const
{
   std::cout << "Burstping name" << std::endl;
   return BurstpingInstanceName;
}

// ###### Send one ICMP request to given destination ########################
void Burstping::sendBurstICMPRequest(const DestinationInfo& destination,
                                 const unsigned int     ttl,
                                 const unsigned int     round,
                                 uint32_t&              targetChecksum,
                                 const unsigned int     payload)
{

   // ====== Set TTL ========================================
   const boost::asio::ip::unicast::hops option(ttl);
   ICMPSocket.set_option(option);

   // ====== Create an ICMP header for an echo request ======
   SeqNumber++;
   ICMPHeader echoRequest;
   echoRequest.type((isIPv6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
   echoRequest.code(0);
   echoRequest.identifier(Identifier);
   echoRequest.seqNumber(SeqNumber);

   TraceServiceHeader tsHeader;
   tsHeader.magicNumber(MagicNumber);
   tsHeader.sendTTL(ttl);
   tsHeader.round((unsigned char)round);
   tsHeader.checksumTweak(0);
   const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();
   tsHeader.sendTimeStamp(makePacketTimeStamp(sendTime));
   std::vector<unsigned char> tsHeaderContents = tsHeader.contents();

   // ====== Tweak checksum ===============================
   computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
   targetChecksum = echoRequest.checksum();

   // ====== Encode the request packet ======================
   boost::asio::streambuf request_buffer;
   std::ostream os(&request_buffer);
   os << echoRequest << tsHeader;
   int missing_data = payload - request_buffer.size();
   if (missing_data > 0){
      std::vector<unsigned char> contents(missing_data, ~0);
      os.write(reinterpret_cast<char const*>(contents.data()), contents.size());
   }
   HPCT_LOG(info) << "Request size: " << request_buffer.size() << std::endl;

   //const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();

   // ======== Payload ========================
   //

   // ====== Tweak checksum ===============================
   //computeInternet16(echoRequest, contents.begin(), contents.end());
   //targetChecksum = echoRequest.checksum();

   // ====== Encode the request packet ======================
   //boost::asio::streambuf request_buffer;
   //std::ostream os(&request_buffer);
   //os << echoRequest;
   //os.write(reinterpret_cast<char const*>(contents.data()), contents.size());
   //HPCT_LOG(info) << "Request size: " << request_buffer.size() << std::endl;


   // ====== Send the request ===============================
   std::size_t sent;
   try {
      int level;
      int option;
      int trafficClass = destination.trafficClass();
      if(destination.address().is_v6()) {
         level  = IPPROTO_IPV6;
         option = IPV6_TCLASS;
      }
      else {
         level  = IPPROTO_IP;
         option = IP_TOS;
      }

      if(setsockopt(ICMPSocket.native_handle(), level, option,
                    &trafficClass, sizeof(trafficClass)) < 0) {
         HPCT_LOG(warning) << "Unable to set Traffic Class!";
         sent = -1;
      }
      else {
         sent = ICMPSocket.send_to(request_buffer.data(), boost::asio::ip::icmp::endpoint(destination.address(), 0));
      }
   }
   catch (boost::system::system_error const& e) {
      sent = -1;
   }
   if(sent < 1) {
      HPCT_LOG(warning) << getName() << ": Burstping::sendBurstICMPRequest() - ICMP send_to("
                        << SourceAddress << "->" << destination << ") failed!";
   }
   else {
      // ====== Record the request ==========================
      OutstandingRequests++;

      assert((targetChecksum & ~0xffff) == 0);
      ResultEntry resultEntry(round, SeqNumber, ttl, (uint16_t)targetChecksum, sendTime,
                              destination, Unknown);
      std::pair<std::map<unsigned short, ResultEntry>::iterator, bool> result = ResultsMap.insert(std::pair<unsigned short, ResultEntry>(SeqNumber,resultEntry));
      assert(result.second == true);
   }
}

// ###### Send requests to all destinations #################################
void Burstping::sendRequests()
{

   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
   // ====== Send requests, if there are destination addresses ==============
   if(Destinations.begin() != Destinations.end()) {
      // All packets of this request block (for each destination) use the same checksum.
      // The next block of requests may then use another checksum.
      uint32_t targetChecksum = ~0U;
      for(std::set<DestinationInfo>::const_iterator destinationIterator = Destinations.begin();
          destinationIterator != Destinations.end(); destinationIterator++) {
         const DestinationInfo& destination = *destinationIterator;
         for(int i=1; i<=Burstping::burst; i++)
         {
            HPCT_LOG(info) << "Burst No. " << i << " of payload " << Burstping::payload << std::endl;
            sendBurstICMPRequest(destination, FinalMaxTTL, 0, targetChecksum, Burstping::payload);
         }

      }

      scheduleTimeoutEvent();
   }

   // ====== No destination addresses -> wait ===============================
   else {
      scheduleIntervalEvent();
   }
}
