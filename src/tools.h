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

#ifndef TOOLS_H
#define TOOLS_H

#include <pwd.h>

#include <chrono>
#include <iomanip>
#include <set>

#include <boost/asio.hpp>


uint64_t usSinceEpoch(const std::chrono::system_clock::time_point& time);

const passwd* getUser(const char* user);
bool reducePrivileges(const passwd* pw);

bool addSourceAddress(std::map<boost::asio::ip::address, std::set<uint8_t>>& array,
                      const std::string&                                     addressString,
                      bool                                                   tryToResolve = true);
bool addDestinationAddress(std::set<boost::asio::ip::address>& array,
                           const std::string&                  addressString,
                           bool                                tryToResolve = true);


// ###### Convert microseconds since the epoch to time point ################
template <typename TimePoint> TimePoint microsecondsToTimePoint(const unsigned long long microTime)
{
   const std::chrono::microseconds      us(microTime);
   const TimePoint timePoint(us);
   return timePoint;
}


// ###### Convert microseconds since the epoch to time point ################
template <typename TimePoint> unsigned long long timePointToMicroseconds(const TimePoint& timePoint)
{
   const std::chrono::microseconds us =
      std::chrono::duration_cast<std::chrono::microseconds>(timePoint.time_since_epoch());
   return us.count();
}


// ###### Convert time point to local time string ###########################
template <typename TimePoint> std::string timePointToLocalTimeString(const TimePoint& timePoint)
{
   const time_t      tt = std::chrono::system_clock::to_time_t(timePoint);
   tm                localTime;
   std::stringstream ss;

   localtime_r(&tt, &localTime);
   ss << std::put_time(&localTime, "%F %T") << std::put_time(&localTime, " %Z");
   return ss.str();
}


// ###### Convert time point to UTC time string #############################
template <typename TimePoint> std::string timePointToUTCTimeString(const TimePoint& timePoint)
{
   const time_t      tt = std::chrono::system_clock::to_time_t(timePoint);
   tm                gmTime;
   std::stringstream ss;

   gmtime_r(&tt, &gmTime);
   ss << std::put_time(&gmTime, "%F %T");
   return ss.str();
}

#endif
