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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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

#include <math.h>
#include <pwd.h>

#include <chrono>
#include <filesystem>
#include <cstring>
#include <iomanip>
#include <map>
#include <set>

#include <boost/asio.hpp>
#include <boost/format.hpp>


const passwd* getUser(const char* user);
bool reducePrivileges(const passwd* pw);
int subDirectoryOf(const std::filesystem::path& path1,
                   const std::filesystem::path& path2);
std::filesystem::path relativeTo(const std::filesystem::path& dataFile,
                                 const std::filesystem::path& basePath);

bool addSourceAddress(std::map<boost::asio::ip::address, std::set<uint8_t>>& array,
                      const std::string&                                     addressString,
                      bool                                                   tryToResolve = true);
bool addDestinationAddress(std::set<boost::asio::ip::address>& array,
                           const std::string&                  addressString,
                           bool                                tryToResolve = true);


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
std::string durationToString(const Duration& duration,
                             const char*     format = "%9.6fms",
                             const double    div    = 1000000.0,
                             const char*     null   = "NULL")
{
   std::stringstream ss;
   double            ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
   if(ns >= 0.0) {
      ss << boost::format(format) % (ns / div);
   }
   else {
      ss << null;
   }
   return ss.str();
}


// ###### Get current time in UTC ###########################################
template<typename TimePoint> TimePoint nowInUTC()
{
   std::time_t t;
   time(&t);

   std::tm tm = {};
   gmtime_r(&t, &tm);

   TimePoint timePoint = TimePoint(std::chrono::seconds(std::mktime(&tm)));
   return timePoint;
}


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
                                             const char*        format    = "%Y-%m-%d %H:%M:%S",
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
   ss << std::put_time(&tm, format);
   if(precision > 0) {
      ss << '.' << std::setw(precision) << std::setfill('0')
         << (unsigned int)rint(fseconds * pow(10.0, precision));
   }

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
   const size_t l = strlen(format);
   if( (format[l - 2] == '%') &&
       (format[l - 1] == 'S') ) {
      double f;
      if (iss.peek() != '.' || !(iss >> f)) {
         return false;
      }
      const size_t fseconds = f * std::chrono::high_resolution_clock::period::den / std::chrono::high_resolution_clock::period::num;
      timePoint += std::chrono::high_resolution_clock::duration(fseconds);
   }

   return true;
}


// ###### Convert sockaddr to endpoint ######################################
template<class Endpoint>
Endpoint sockaddrToEndpoint(const sockaddr* address, const socklen_t socklen)
{
   if(socklen >= sizeof(sockaddr_in)) {
      if(address->sa_family == AF_INET) {
         return Endpoint( boost::asio::ip::address_v4(ntohl(((sockaddr_in*)address)->sin_addr.s_addr)),
                          ntohs(((sockaddr_in*)address)->sin_port) );
      }
      else if(address->sa_family == AF_INET6) {
         const unsigned char* aptr = ((sockaddr_in6*)address)->sin6_addr.s6_addr;
         boost::asio::ip::address_v6::bytes_type v6address;
         std::copy(aptr, aptr + v6address.size(), v6address.data());
         return Endpoint( boost::asio::ip::address_v6(v6address, ((sockaddr_in6*)address)->sin6_scope_id),
                          ntohs(((sockaddr_in6*)address)->sin6_port) );
      }
   }
   return Endpoint();
}

#endif
