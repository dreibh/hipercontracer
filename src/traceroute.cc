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
// Copyright (C) 2015 by Thomas Dreibholz
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

#include "traceroute.h"

#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"

#include <boost/format.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>


// ###### Constructor #######################################################
ResultEntry::ResultEntry(const unsigned short           seqNumber,
                         const unsigned int             hop,
                         boost::asio::ip::address       address,
                         const HopStatus                status,
                         const boost::posix_time::ptime sendTime)
   : SeqNumber(seqNumber),
     Hop(hop),
     Address(address),
     Status(status),
     SendTime(sendTime)
{
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry)
{
   os << boost::format("#%5d")            % resultEntry.SeqNumber
      << "\t" << boost::format("%2d")     % resultEntry.Hop
      << "\t" << boost::format("%9.3fms") % (resultEntry.rtt().total_microseconds() / 1000.0)
      << "\t" << boost::format("%3d")     % resultEntry.Status
      << "\t" << resultEntry.Address;
   return(os);
}


// ###### Constructor #######################################################
Traceroute::Traceroute(const boost::asio::ip::address&          sourceAddress,
                       const std::set<boost::asio::ip::address> destinationAddressArray,
                       const unsigned long long                 interval,
                       const unsigned int                       expiration,
                       const unsigned int                       initialMaxTTL,
                       const unsigned int                       finalMaxTTL,
                       const unsigned int                       incrementMaxTTL)
   : Interval(interval),
     Expiration(expiration),
     InitialMaxTTL(initialMaxTTL),
     FinalMaxTTL(finalMaxTTL),
     IncrementMaxTTL(incrementMaxTTL),
     IOService(),
     SourceAddress(sourceAddress),
     ICMPSocket(IOService, (isIPv6() == true) ? boost::asio::ip::icmp::v6() : boost::asio::ip::icmp::v4()),
     TimeoutTimer(IOService)
{
   // ====== Some initialisations ===========================================
   Identifier          = 0;
   SeqNumber           = (unsigned short)(std::rand() & 0xffff);
   MagicNumber         = ((std::rand() & 0xffff) << 16) | (std::rand() & 0xffff);
   OutstandingRequests = 0;
   LastHop             = 0xffffffff;
   ExpectingReply      = false;
   StopRequested       = false;
   MinTTL              = 1;
   MaxTTL              = InitialMaxTTL;

   // ====== Prepare destination endpoints ==================================
   for(std::set<boost::asio::ip::address>::const_iterator destinationIterator = destinationAddressArray.begin();
       destinationIterator != destinationAddressArray.end(); destinationIterator++) {
      const boost::asio::ip::address& destinationAddress = *destinationIterator;
      if(destinationAddress.is_v6() == sourceAddress.is_v6()) {
         DestinationAddressArray.insert(destinationAddress);
      }
   }
   DestinationAddressIterator = DestinationAddressArray.end();
}


// ###### Destructor ########################################################
Traceroute::~Traceroute()
{
}


// ###### Start thread ######################################################
bool Traceroute::start()
{
   StopRequested = false;
   Thread        = boost::thread(&Traceroute::run, this);
   return(prepareSocket());
}


// ###### Request stop of thread ############################################
void Traceroute::requestStop() {
   StopRequested = true;
}


// ###### Join thread #######################################################
void Traceroute::join()
{
   Thread.join();
   StopRequested = false;
}


// ###### Prepare ICMP socket ###############################################
bool Traceroute::prepareSocket()
{
   // ====== Bind ICMP socket to given source address =======================
   boost::system::error_code errorCode;
   ICMPSocket.bind(boost::asio::ip::icmp::endpoint(SourceAddress, 0), errorCode);
   if(errorCode !=  boost::system::errc::success) {
      std::cerr << "ERROR: Unable to bind ICMP socket to source address "
                << SourceAddress << "!" << std::endl;
      return(false);
   }

   // ====== Set filter (not required, but much more efficient) =============
   if(isIPv6()) {
      struct icmp6_filter filter;
      ICMP6_FILTER_SETBLOCKALL(&filter);
      ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_DST_UNREACH, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &filter);
      ICMP6_FILTER_SETPASS(ICMP6_TIME_EXCEEDED, &filter);
      if(setsockopt(ICMPSocket.native(), IPPROTO_ICMPV6, ICMP6_FILTER,
                    &filter, sizeof(struct icmp6_filter)) < 0) {
         std::cerr << "WARNING: Unable to set ICMP6_FILTER!" << std::endl;
      }
   }
   return(true);
}


// ###### Prepare a new run #################################################
void Traceroute::prepareRun()
{
   // ====== Get next destination address ===================================
   if(DestinationAddressIterator != DestinationAddressArray.end()) {
      DestinationAddressIterator++;
   }
   if(DestinationAddressIterator == DestinationAddressArray.end()) {
      // Rewind ...
      DestinationAddressIterator = DestinationAddressArray.begin();
   }

   // ====== Clear results ==================================================
   ResultsMap.clear();
   MinTTL              = 1;
   MaxTTL              = (DestinationAddressIterator != DestinationAddressArray.end()) ? getInitialMaxTTL(*DestinationAddressIterator) : InitialMaxTTL;
   LastHop             = 0xffffffff;
   OutstandingRequests = 0;
   RunStartTimeStamp   = boost::posix_time::microsec_clock::universal_time();
}


// ###### Send requests to all destinations #################################
void Traceroute::sendRequests()
{
   if(DestinationAddressIterator != DestinationAddressArray.end()) {
      const boost::asio::ip::address& destinationAddress = *DestinationAddressIterator;

      std::cout << "Traceroute from " << SourceAddress
                << " to " << destinationAddress << " ... " << std::endl;

      // ====== Send Echo Requests ==========================================
      assert(MinTTL > 0);
      for(int ttl = (int)MaxTTL; ttl >= (int)MinTTL; ttl--) {
         sendICMPRequest(destinationAddress, (unsigned int)ttl);
      }
   }

   scheduleTimeout();
}


// ###### Schedule timer ####################################################
void Traceroute::scheduleTimeout()
{
   const unsigned int deviation = std::max(10U, Expiration / 5);   // 20% deviation
   const unsigned int duration  = Expiration + (std::rand() % deviation);
   TimeoutTimer.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(duration));
   TimeoutTimer.async_wait(boost::bind(&Traceroute::handleTimeout, this, _1));
}


// ###### Expect next ICMP message ##########################################
void Traceroute::expectNextReply()
{
   assert(ExpectingReply == false);
   ICMPSocket.async_receive_from(boost::asio::buffer(MessageBuffer),
                                 ReplyEndpoint,
                                 boost::bind(&Traceroute::handleMessage, this, _2));
   ExpectingReply = true;
}


// ###### All requests have received a response #############################
void Traceroute::noMoreOutstandingRequests()
{
   puts("COMPLETED!");
   TimeoutTimer.cancel();
}


// ###### Get value for initial MaxTTL ######################################
unsigned int Traceroute::getInitialMaxTTL(const boost::asio::ip::address& destinationAddress) const
{
   const std::map<boost::asio::ip::address, unsigned int>::const_iterator found = TTLCache.find(destinationAddress);
   if(found != TTLCache.end()) {
      return(std::min(found->second, FinalMaxTTL));
   }
   return(InitialMaxTTL);
}


// ###### Send one ICMP request to given destination ########################
void Traceroute::sendICMPRequest(const boost::asio::ip::address& destinationAddress,
                                 const unsigned int              ttl)
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
   const boost::posix_time::ptime sendTime = boost::posix_time::microsec_clock::universal_time();
   TraceServiceHeader tsHeader;
   tsHeader.magicNumber(MagicNumber);
   tsHeader.sendTTL(ttl);
   tsHeader.sendTimeStamp(ptimeToMircoTime(sendTime));
   const std::vector<unsigned char> tsHeaderContents = tsHeader.contents();
   computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());

   // ====== Encode the request packet ======================
   boost::asio::streambuf request_buffer;
   std::ostream os(&request_buffer);
   os << echoRequest << tsHeader;

   // ====== Send the request ===============================
   const size_t sent = ICMPSocket.send_to(request_buffer.data(), boost::asio::ip::icmp::endpoint(destinationAddress, 0));
   if(sent < 1) {
      std::cerr << "WARNING: ICMP send_to() failed!" << std::endl;
   }
   else {
      // ====== Record the request ==========================
      OutstandingRequests++;

      ResultEntry resultEntry(SeqNumber, ttl, destinationAddress, Unknown, sendTime);
      std::pair<std::map<unsigned short, ResultEntry>::iterator, bool> result = ResultsMap.insert(std::pair<unsigned short, ResultEntry>(SeqNumber,resultEntry));
      assert(result.second == true);
   }
}


// ###### Run the measurement ###############################################
void Traceroute::run()
{
   Identifier = ::getpid();   // Identifier is the process ID
   // NOTE: Assuming 16-bit PID, and one PID per thread!

   prepareRun();
   sendRequests();
   expectNextReply();

   IOService.run();
}


// ###### The destination has not been reached with the current TTL #########
bool Traceroute::notReachedWithCurrentTTL()
{
   if(MaxTTL < FinalMaxTTL) {
      MinTTL = MaxTTL + 1;
      MaxTTL = std::min(MaxTTL + IncrementMaxTTL, FinalMaxTTL);
      printf("TRYING: %d -- %d\n",MinTTL, MaxTTL);
      return(true);
   }
   return(false);
}


// ###### Comparison function for results output ############################
int Traceroute::compareTracerouteResults(const ResultEntry* a, const ResultEntry* b)
{
  return(a->hop() < b->hop());
}


// ###### Process results ###################################################
void Traceroute::processResults()
{
   std::vector<ResultEntry*> resultsVector;
   for(std::map<unsigned short, ResultEntry>::iterator iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(&iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), &compareTracerouteResults);

   for(std::vector<ResultEntry*>::iterator iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;
      std::cout << *resultEntry << std::endl;
      if(resultEntry->status() == Success) {
         break;
      }
   }
}


// ###### Handle timer event ################################################
void Traceroute::handleTimeout(const boost::system::error_code& errorCode)
{
   // ====== Stop requested? ================================================
   if(StopRequested) {
      IOService.stop();
   }

   // ====== Has destination been reached with current TTL? =================
   TTLCache[*DestinationAddressIterator] = LastHop;
   if(LastHop == 0xffffffff) {
      printf("not found with %d\n", MaxTTL);
      if(notReachedWithCurrentTTL()) {
         // Try another round ...
         sendRequests();
         return;
      }
   }

   // ====== Create results output ==========================================
   processResults();

   // ====== Prepare new run ================================================
   prepareRun();
   sendRequests();
}


// ###### Handle incoming ICMP message ######################################
void Traceroute::handleMessage(std::size_t length)
{
   const boost::posix_time::ptime receiveTime = boost::posix_time::microsec_clock::universal_time();
   boost::interprocess::bufferstream is(MessageBuffer, length);
   ExpectingReply = false;   // Need to call expectNextReply() to get next message!

   ICMPHeader icmpHeader;
   if(isIPv6()) {
      is >> icmpHeader;
      if(is) {
         if(icmpHeader.type() == ICMPHeader::IPv6EchoReply) {
            if(icmpHeader.identifier() == Identifier) {
               TraceServiceHeader tsHeader;
               is >> tsHeader;
               if(is) {
                  if(tsHeader.magicNumber() == MagicNumber) {
                     recordResult(receiveTime, icmpHeader, icmpHeader.seqNumber());
                  }
               }
            }
         }
         else if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
                  (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ) {
            IPv6Header innerIPv6Header;
            ICMPHeader innerICMPHeader;
            TraceServiceHeader tsHeader;
            is >> innerIPv6Header >> innerICMPHeader >> tsHeader;
            if(is) {
               if(tsHeader.magicNumber() == MagicNumber) {
                  recordResult(receiveTime, icmpHeader, innerICMPHeader.seqNumber());
               }
            }
         }
      }
   }
   else {
      IPv4Header ipv4Header;
      is >> ipv4Header;
      if(is) {
         is >> icmpHeader;
         if(is) {
            if(icmpHeader.type() == ICMPHeader::IPv4EchoReply) {
               if(icmpHeader.identifier() == Identifier) {
                  TraceServiceHeader tsHeader;
                  is >> tsHeader;
                  if(is) {
                     if(tsHeader.magicNumber() == MagicNumber) {
                        recordResult(receiveTime, icmpHeader, icmpHeader.seqNumber());
                     }
                  }
               }
            }
            else if(icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) {
               IPv4Header innerIPv4Header;
               ICMPHeader innerICMPHeader;
               is >> innerIPv4Header >> innerICMPHeader;
               if(is) {
                  if( (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ||
                      (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
                     if(innerICMPHeader.identifier() == Identifier) {
                        // Unfortunately, ICMPv4 does not return the full TraceServiceHeader here!
                        recordResult(receiveTime, icmpHeader, innerICMPHeader.seqNumber());
                     }
                  }
               }
            }
         }
      }
   }

   if(OutstandingRequests == 0) {
      noMoreOutstandingRequests();
   }
   expectNextReply();
}


// ###### Record result from response message ###############################
void Traceroute::recordResult(const boost::posix_time::ptime& receiveTime,
                              const ICMPHeader&               icmpHeader,
                              const unsigned short            seqNumber)
{
   // ====== Find corresponding request =====================================
   std::map<unsigned short, ResultEntry>::iterator found = ResultsMap.find(seqNumber);
   if(found == ResultsMap.end()) {
      return;
   }
   ResultEntry& resultEntry = found->second;

   // ====== Get status =====================================================
   if(resultEntry.status() == Unknown) {
      resultEntry.receiveTime(receiveTime);
      resultEntry.address(ReplyEndpoint.address());

      HopStatus status = Unknown;
      if( (icmpHeader.type() == ICMPHeader::IPv6TimeExceeded) ||
          (icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) ) {
         status = TimeExceeded;
      }
      else if( (icmpHeader.type() == ICMPHeader::IPv6Unreachable) ||
               (icmpHeader.type() == ICMPHeader::IPv4Unreachable) ) {
         if(isIPv6()) {
            switch(icmpHeader.code()) {
               case ICMP6_DST_UNREACH_ADMIN:
                  status = UnreachableProhibited;
               break;
               case ICMP6_DST_UNREACH_BEYONDSCOPE:
                  status = UnreachableScope;
               break;
               case ICMP6_DST_UNREACH_NOROUTE:
                  status = UnreachableNetwork;
               break;
               case ICMP6_DST_UNREACH_ADDR:
                  status = UnreachableHost;
               break;
               case ICMP6_DST_UNREACH_NOPORT:
                  status = UnreachablePort;
               break;
               default:
                  status = UnreachableUnknown;
               break;
            }
         }
         else {
            switch(icmpHeader.code()) {
               case ICMP_PKT_FILTERED:
                  status = UnreachableProhibited;
               break;
               case ICMP_NET_UNREACH:
               case ICMP_NET_UNKNOWN:
                  status = UnreachableNetwork;
               break;
               case ICMP_HOST_UNREACH:
               case ICMP_HOST_UNKNOWN:
                  status = UnreachableHost;
               break;
               case ICMP_PORT_UNREACH:
                  status = UnreachablePort;
               break;
               default:
                  status = UnreachableUnknown;
               break;
            }
         }
      }
      else if( (icmpHeader.type() == ICMPHeader::IPv6EchoReply) ||
               (icmpHeader.type() == ICMPHeader::IPv4EchoReply) ) {
         status  = Success;
         LastHop = std::min(LastHop, resultEntry.hop());
      }
      resultEntry.status(status);
      if(OutstandingRequests > 0) {
         OutstandingRequests--;
      }
   }
}


// ###### Convert ptime to microseconds #####################################
unsigned long long Traceroute::ptimeToMircoTime(const boost::posix_time::ptime t)
{
   static const boost::posix_time::ptime myEpoch(boost::gregorian::date(1976,9,29));
   boost::posix_time::time_duration difference = t - myEpoch;
   return(difference.ticks());
}
