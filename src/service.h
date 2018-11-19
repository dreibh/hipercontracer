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
// Copyright (C) 2015-2019 by Thomas Dreibholz
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

#ifndef SERVICE_H
#define SERVICE_H

#include <boost/system/error_code.hpp>

class Service
{
   public:
   virtual ~Service() {};
   virtual bool start() = 0;
   virtual void requestStop() = 0;
   virtual void join() = 0;
   virtual bool prepareSocket() = 0;
   virtual bool prepareRun(const bool newRound = false) = 0;
   virtual void scheduleTimeoutEvent() = 0;
   virtual void scheduleIntervalEvent() = 0;
   virtual void expectNextReply() = 0;
   virtual void noMoreOutstandingRequests() = 0;
   virtual bool notReachedWithCurrentTTL() = 0;
   virtual void processResults() = 0;
   virtual void sendRequests() = 0;
   virtual void handleTimeoutEvent(const boost::system::error_code& errorCode) = 0;
   virtual void handleIntervalEvent(const boost::system::error_code& errorCode) = 0;
   virtual void handleMessage(std::size_t length) = 0;
};

#endif
