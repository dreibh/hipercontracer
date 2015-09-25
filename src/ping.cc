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

#include "ping.h"


Ping::Ping(const boost::asio::ip::address&          sourceAddress,
           const std::set<boost::asio::ip::address> destinationAddressArray,
           const unsigned long long                 interval,
           const unsigned int                       expiration,
           const unsigned int                       ttl)
   : Traceroute(sourceAddress, destinationAddressArray, interval, expiration, ttl, ttl, ttl)
{
}


Ping::~Ping()
{
}


void Ping::noMoreOutstandingRequests()
{
   // Nothing to do for Ping!
}


void Ping::prepareRun()
{
   // Nothing to do for Ping!
}


bool Ping::notReachedWithCurrentTTL()
{
   // Nothing to do for Ping!
   return(false);
}


void Ping::scheduleTimeout()
{
   const unsigned long long deviation = std::max(10ULL, Interval / 5ULL);   // 20% deviation
   const unsigned long long duration  = Interval + (std::rand() % deviation);
   TimeoutTimer.expires_at(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(duration));
   TimeoutTimer.async_wait(boost::bind(&Ping::handleTimeout, this, _1));
}


void Ping::processResults()
{
   std::vector<ResultEntry*> resultsVector;
   for(auto iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(&iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), [](ResultEntry* a, ResultEntry* b) {
        return(a->address() < b->address());
   });

   const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
   for(auto iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;
      std::cout << *resultEntry << std::endl;

      if( (resultEntry->status() != HopStatus::Unknown) ||
          ((now - resultEntry->sendTime()).total_milliseconds() >= Expiration) ) {
         assert(ResultsMap.erase(resultEntry->seqNumber()) == 1);
         if(resultEntry->status() == HopStatus::Unknown) {
            if(OutstandingRequests > 0) {
               OutstandingRequests--;
            }
         }
      }
   }
}


void Ping::sendRequests()
{
   for(auto destinationIterator = DestinationAddressArray.begin();
       destinationIterator != DestinationAddressArray.end(); destinationIterator++) {
      const boost::asio::ip::address& destinationAddress = *destinationIterator;
      sendICMPRequest(destinationAddress, FinalMaxTTL);
   }

   scheduleTimeout();
}
