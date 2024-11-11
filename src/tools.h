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

#ifndef TOOLS_H
#define TOOLS_H

#include <math.h>
#include <pwd.h>
#include <time.h>

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
bool addSourceAddressesFromFile(std::map<boost::asio::ip::address, std::set<uint8_t>>& array,
                                const std::filesystem::path&                           inputFileName,
                                bool                                                   tryToResolve = true);
bool addDestinationAddressesFromFile(std::set<boost::asio::ip::address>& array,
                                     const std::filesystem::path&        inputFileName,
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
   long long         ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
   if(ns >= 0) {
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
   timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   return TimePoint(std::chrono::seconds(ts.tv_sec) +
                    std::chrono::nanoseconds(ts.tv_nsec));
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


// ###### Convert nanoseconds since the epoch to time point #################
template <typename TimePoint> TimePoint nanosecondsToTimePoint(const unsigned long long nanoTime)
{
   const std::chrono::nanoseconds nanoseconds(nanoTime);
   const TimePoint                timePoint(nanoseconds);
   return timePoint;
}


// ###### Convert nanoseconds since the epoch to time point #################
template <typename TimePoint> unsigned long long timePointToNanoseconds(const TimePoint& timePoint)
{
   const std::chrono::nanoseconds nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(timePoint.time_since_epoch());
   return nanoseconds.count();
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
         << (unsigned int)floor(fseconds * pow(10.0, precision));
   }

   return ss.str();
}


// ###### Convert UTC time string to time point #############################
template <typename TimePoint> bool stringToTimePoint(
                                      const std::string& string,
                                      TimePoint&         timePoint,
                                      const char*        format = "%Y-%m-%d %H:%M:%S",
                                      const bool         utc    = true)
{
   // ====== Handle time in seconds granularity =============================
   std::istringstream iss(string);
   std::tm            tm = {};
   if(!(iss >> std::get_time(&tm, format))) {
      return false;
   }
   const std::time_t t = (utc == true) ? timegm(&tm) : mktime(&tm);
   timePoint = TimePoint(std::chrono::seconds(t));
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


// ###### Make directory hierarchy from a file, relative to importer path ###
// Example:
// dataFile="4133/udpping/2024-06-12/uping_10382.dat.2024-06-12_13-10-22.xz"
// timestamp=(2024-06-12 13:10:22)
// directoryLevels=0 timestampLevels=5 -> "2024/06/11/15:10"
// directoryLevels=0 timestampLevels=4 -> "2024/06/11/15:00"
// directoryLevels=0 timestampLevels=3 -> "2024/06/11"
// directoryLevels=0 timestampLevels=2 -> "2024/06"
// directoryLevels=0 timestampLevels=1 -> "2024"
// directoryLevels=0 timestampLevels=0 -> ""
// directoryLevels=1 timestampLevels=5 -> "4133/2024/06/11/15:10"
// directoryLevels=1 timestampLevels=4 -> "4133/2024/06/11/15:00"
// directoryLevels=1 timestampLevels=3 -> "4133/2024/06/11"
// directoryLevels=1 timestampLevels=2 -> "4133/2024/06"
// directoryLevels=1 timestampLevels=1 -> "4133/2024"
// directoryLevels=1 timestampLevels=0 -> "4133"
// directoryLevels=2 timestampLevels=5 -> "4133/udpping/2024/06/11/15:10"
// directoryLevels=2 timestampLevels=4 -> "4133/udpping/2024/06/11/15:00"
// directoryLevels=2 timestampLevels=3 -> "4133/udpping/2024/06/11"
// directoryLevels=2 timestampLevels=2 -> "4133/udpping/2024/06"
// directoryLevels=2 timestampLevels=1 -> "4133/udpping/2024"
// directoryLevels=2 timestampLevels=0 -> "4133/udpping"
template <typename TimePoint>
   std::filesystem::path makeDirectoryHierarchy(const std::filesystem::path& basePath,
                                                const std::filesystem::path& dataFile,
                                                const TimePoint&             timeStamp,
                                                const unsigned int           directoryLevels,
                                                const unsigned int           timestampLevels)
{
   std::filesystem::path hierarchy;

   // Get the relative directory the file is located in:
   std::error_code             ec;
   const std::filesystem::path relPath =
      std::filesystem::relative(dataFile.parent_path(), basePath, ec);

   if(!ec) {
      unsigned int d = 0;
      for(std::filesystem::path::const_iterator iterator = relPath.begin(); iterator != relPath.end(); iterator++) {
         if(d++ >= directoryLevels) {
            break;
         }
         else if( (d == 1) && ((*iterator) == ".") ) {
            // First directory is ".": there is no hierarchy -> stop here.
            break;
         }
         hierarchy = hierarchy / (*iterator);
      }
   }

   if(timestampLevels > 0) {
      const char* format = nullptr;
      if(timestampLevels >= 5) {
         format = "%Y/%m/%d/%H:00/%H:%M";
      }
      else if(timestampLevels == 4) {
         format = "%Y/%m/%d/%H:00";
      }
      else if(timestampLevels == 3) {
         format = "%Y/%m/%d";
      }
      else if(timestampLevels == 2) {
         format = "%Y/%m";
      }
      else if(timestampLevels == 1) {
         format = "%Y";
      }
      hierarchy = hierarchy / timePointToString<TimePoint>(timeStamp, 0, format);
   }

   // std::cout << dataFile << " -> " << hierarchy << "\n";
   return hierarchy;
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
