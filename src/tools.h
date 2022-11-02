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
#include <filesystem>
#include <iomanip>
#include <set>

#include <boost/asio.hpp>


uint64_t usSinceEpoch(const std::chrono::system_clock::time_point& time);

const passwd* getUser(const char* user);
bool reducePrivileges(const passwd* pw);
bool is_subdir_of(const std::filesystem::path& path1,
                  const std::filesystem::path& path2);
std::filesystem::path relative_to(const std::filesystem::path& dataFile,
                                  const std::filesystem::path& basePath);

bool addSourceAddress(std::map<boost::asio::ip::address, std::set<uint8_t>>& array,
                      const std::string&                                     addressString,
                      bool                                                   tryToResolve = true);
bool addDestinationAddress(std::set<boost::asio::ip::address>& array,
                           const std::string&                  addressString,
                           bool                                tryToResolve = true);


// ###### Convert microseconds since the epoch to time point ################
template <typename TimePoint> TimePoint microsecondsToTimePoint(const unsigned long long microTime)
{
   const std::chrono::microseconds microseconds(microTime);
   const TimePoint                 timePoint(microseconds);
   return timePoint;
}


// ###### Convert microseconds since the epoch to time point ################
template <typename TimePoint> unsigned long long timePointToMicroseconds(const TimePoint& timePoint)
{
   const std::chrono::microseconds microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(timePoint.time_since_epoch());
   return microseconds.count();
}


// ###### Convert time point to UTC time string #############################
template <typename TimePoint> std::string timePointToString(
                                             const TimePoint&   timePoint,
                                             const unsigned int precision = 0,
                                             const char*        format    = "%Y-%m-%d %H:%M:",   // %S omitted!
                                             const bool         utc       = true)
{
   double seconds        = double(timePoint.time_since_epoch().count()) * TimePoint::period::num / TimePoint::period::den;
   const double fseconds = std::modf(seconds, &seconds);
   const time_t tt       = seconds;

   std::stringstream ss;
   std::tm           tm;
   if(utc) {
      gmtime_r(&tt, &tm);
   }
   else {
      localtime_r(&tt, &tm);
   }
   ss << std::put_time(&tm, format)
      << std::setw((precision == 0) ? 2 : precision + 3) << std::setfill('0')
      << std::fixed << std::setprecision(precision)
      << tm.tm_sec + fseconds;

   return ss.str();
}


// ###### Convert UTC time string to time point #############################
template <typename TimePoint> bool stringToTimePoint(
                                      const std::string& string,
                                      TimePoint&         timePoint,
                                      const char*        format = "%Y-%m-%d %H:%M:%S")
{
   // ====== Handle time in seconds granularity =============================
   std::istringstream iss(string);
   std::tm            tm = {};
   if(!(iss >> std::get_time(&tm, format))) {
      return false;
   }
   timePoint = TimePoint(std::chrono::seconds(std::mktime(&tm)));
   if(iss.eof()) {
      return true;
   }

   // ====== Handle fractional seconds ======================================
   double f;
   if (iss.peek() != '.' || !(iss >> f)) {
      return false;
   }
   const size_t fseconds = f * std::chrono::high_resolution_clock::period::den / std::chrono::high_resolution_clock::period::num;
   timePoint += std::chrono::high_resolution_clock::duration(fseconds);

   return true;
}

#endif
