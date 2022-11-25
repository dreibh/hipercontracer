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

#include <set>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/format.hpp>


// ###### Convert time to microseconds since the epoch ######################
template<class TimePoint>
uint64_t usSinceEpoch(const TimePoint& timePoint)
{
   const auto duration = timePoint.time_since_epoch();
   return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}


// ###### Convert time to nanoseconds since the epoch #######################
template<class TimePoint>
uint64_t nsSinceEpoch(const TimePoint& timePoint)
{
   const auto duration = timePoint.time_since_epoch();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}


// ###### Convert duration to string ########################################
template <typename Duration>
static std::string durationToString(const Duration& duration,
                                    const char*     format = "%9.6fms",
                                    const double    div    = 1000000.0,
                                    const char*     null   = "NULL")
{
   std::stringstream ss;
   if(duration != Duration::min()) {
      ss << boost::format(format) %
               (std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / div);
   }
   else {
      ss << null;
   }
   return ss.str();
}


const passwd* getUser(const char* user);
bool reducePrivileges(const passwd* pw);

bool addSourceAddress(std::map<boost::asio::ip::address, std::set<uint8_t>>& array,
                      const std::string&                                     addressString,
                      bool                                                   tryToResolve = true);
bool addDestinationAddress(std::set<boost::asio::ip::address>& array,
                           const std::string&                  addressString,
                           bool                                tryToResolve = true);

#endif
