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

#include "tools.h"
#include "logger.h"

#include <string.h>
#include <unistd.h>

#include <iostream>

#include <boost/algorithm/string.hpp>


// ###### Convert time to microseconds since the epoch ######################
uint64_t usSinceEpoch(const std::chrono::system_clock::time_point& time)
{
   const std::chrono::system_clock::duration duration = time.time_since_epoch();
   return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}


// ###### Reduce permissions of process #####################################
passwd* getUser(const char* user)
{
   passwd* pw = NULL;
   if(user != NULL) {
      pw = getpwnam(user);
      if(pw == NULL) {
         pw = getpwuid(atoi(user));
         if(pw == NULL) {
            HPCT_LOG(fatal) << "Provided user " << user << " is not a user name or UID!";
            ::exit(1);
         }
      }
   }
   return pw;
}


// ###### Reduce permissions of process #####################################
bool reducePermissions(const passwd* pw)
{
   // ====== Reduce permissions =============================================
   if((pw != NULL) && (pw->pw_uid != 0)) {
      HPCT_LOG(info) << "Using UID " << pw->pw_uid << ", GID " << pw->pw_gid;
      if(setgid(pw->pw_gid) != 0) {
         HPCT_LOG(fatal) << "setgid(" << pw->pw_gid << ") failed: " << strerror(errno);
         ::exit(1);
      }
      if(setuid(pw->pw_uid) != 0) {
         HPCT_LOG(fatal) << "setuid(" << pw->pw_uid << ") failed: " << strerror(errno);
         ::exit(1);
      }
   }
   else {
      HPCT_LOG(info) << "Working as root (uid 0). This is not recommended!";
      return false;
   }

   return true;
}


// ###### Add source address to set #########################################
void addSourceAddress(std::set<std::pair<boost::asio::ip::address,uint8_t>>& array,
                      const std::string&                                     addressString)
{
   boost::system::error_code errorCode;

   std::vector<std::string> addressParameters;
   boost::split(addressParameters, addressString, boost::is_any_of(","));
   if(addressParameters.size() > 0) {
      unsigned int trafficClass = 0x00;
      if(addressParameters.size() > 1) {
         trafficClass = std::strtoul(addressParameters[1].c_str(), NULL, 16);
         if(trafficClass > 0xff) {
            std::cerr << "ERROR: Bad traffic class " << addressParameters[1] << "!" << std::endl;
            ::exit(1);
         }
      }
      boost::asio::ip::address address = boost::asio::ip::address::from_string(addressParameters[0], errorCode);
      if(errorCode != boost::system::errc::success) {
         std::cerr << "ERROR: Bad source address " << addressParameters[0] << "!" << std::endl;
         ::exit(1);
      }
      array.insert(std::pair<boost::asio::ip::address,uint8_t>(address, trafficClass));
   }
   else {
      std::cerr << "ERROR: Invalid source address specification " << addressString << std::endl;
      ::exit(1);
   }
}


// ###### Add destination address to set ####################################
void addDestinationAddress(std::set<boost::asio::ip::address>& array,
                           const std::string&                  addressString)
{
   boost::system::error_code errorCode;
   boost::asio::ip::address address = boost::asio::ip::address::from_string(addressString, errorCode);
   if(errorCode != boost::system::errc::success) {
      std::cerr << "ERROR: Bad destination address " << addressString << "!" << std::endl;
      ::exit(1);
   }
   array.insert(address);
}
