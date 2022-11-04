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
const passwd* getUser(const char* user)
{
   passwd* pw = nullptr;
   if((user != nullptr) && (strlen(user) > 0)) {
      pw = getpwnam(user);
      if(pw == nullptr) {
         int userID = -1;
         if( (sscanf(user, "%d", &userID) != 1) ||
             ( (pw = getpwuid(userID)) == nullptr) ) {
            HPCT_LOG(error) << "Provided user \"" << user << "\" is not a user name or UID!";
            return nullptr;
         }
      }
   }
   return pw;
}


// ###### Check whether path1 is a subdirectory of path2 ####################
bool is_subdir_of(const std::filesystem::path& path1,
                  const std::filesystem::path& path2)
{
   try {
      const std::string ca1 = std::filesystem::canonical(std::filesystem::absolute(path1));
      const std::string ca2 = std::filesystem::canonical(std::filesystem::absolute(path2));
      return ca1.substr(0, ca2.size()) == ca2;
   }
   catch(...) {
      return false;
   }
}



// ###### Destructor ########################################################
std::filesystem::path relative_to(const std::filesystem::path& dataFile,
                                  const std::filesystem::path& basePath)
{
   std::error_code       ec;
   std::filesystem::path relativePath = std::filesystem::relative(dataFile, basePath, ec);
   if(ec) {
      // Failed to get relative path -> return existing path.
      return dataFile;
   }
   return relativePath;
}


// ###### Reduce privileges of process ######################################
bool reducePrivileges(const passwd* pw)
{
   // ====== Reduce permissions =============================================
   if((pw != nullptr) && (pw->pw_uid != 0)) {
      HPCT_LOG(info) << "Using UID " << pw->pw_uid << ", GID " << pw->pw_gid;
      if(setgid(pw->pw_gid) != 0) {
         HPCT_LOG(error) << "setgid(" << pw->pw_gid << ") failed: " << strerror(errno);
         return false;
      }
      if(setuid(pw->pw_uid) != 0) {
         HPCT_LOG(error) << "setuid(" << pw->pw_uid << ") failed: " << strerror(errno);
         return false;
      }
   }
   else {
      HPCT_LOG(warning) << "Working as root (uid 0). This is not recommended!";
      return true;
   }

   return true;
}


struct DSCPValue
{
    const char* Name;
    uint8_t    Value;
};

#define DSCP_TO_TRAFFICCLASS(x) ((uint8_t)(x) << 2)
DSCPValue DSCPValuesTable[] = {
   { "BE",   DSCP_TO_TRAFFICCLASS(0x00) },

   { "EF",   DSCP_TO_TRAFFICCLASS(0x2e) },

   { "AF11", DSCP_TO_TRAFFICCLASS(0x0a) },
   { "AF12", DSCP_TO_TRAFFICCLASS(0x0c) },
   { "AF13", DSCP_TO_TRAFFICCLASS(0x0e) },

   { "AF21", DSCP_TO_TRAFFICCLASS(0x12) },
   { "AF22", DSCP_TO_TRAFFICCLASS(0x14) },
   { "AF23", DSCP_TO_TRAFFICCLASS(0x16) },

   { "AF31", DSCP_TO_TRAFFICCLASS(0x1a) },
   { "AF32", DSCP_TO_TRAFFICCLASS(0x1c) },
   { "AF33", DSCP_TO_TRAFFICCLASS(0x1e) },

   { "AF41", DSCP_TO_TRAFFICCLASS(0x22) },
   { "AF42", DSCP_TO_TRAFFICCLASS(0x24) },
   { "AF43", DSCP_TO_TRAFFICCLASS(0x26) },

   { "CS1",  DSCP_TO_TRAFFICCLASS(0x08) },
   { "CS2",  DSCP_TO_TRAFFICCLASS(0x10) },
   { "CS3",  DSCP_TO_TRAFFICCLASS(0x18) },
   { "CS4",  DSCP_TO_TRAFFICCLASS(0x20) },
   { "CS5",  DSCP_TO_TRAFFICCLASS(0x28) },
   { "CS6",  DSCP_TO_TRAFFICCLASS(0x30) },
   { "CS7",  DSCP_TO_TRAFFICCLASS(0x38) }
};


// ###### Add source address to set #########################################
bool addSourceAddress(std::map<boost::asio::ip::address, std::set<uint8_t>>& array,
                      const std::string&                                     addressString,
                      bool                                                   tryToResolve)
{
   boost::system::error_code errorCode;
   std::vector<std::string>  addressParameters;

   boost::split(addressParameters, addressString, boost::is_any_of(","));
   if(addressParameters.size() > 0) {
      const boost::asio::ip::address address = boost::asio::ip::address::from_string(addressParameters[0], errorCode);
      if(errorCode != boost::system::errc::success) {
         if(tryToResolve) {
            boost::asio::io_service ios;
            boost::asio::ip::tcp::resolver::query resolver_query(
               addressString, "0", boost::asio::ip::tcp::resolver::query::numeric_service);
            boost::asio::ip::tcp::resolver resolver(ios);
            boost::system::error_code ec;
            const boost::asio::ip::tcp::resolver::results_type endpoints =
               resolver.resolve(resolver_query, ec);
            if(ec) {
               std::cerr << "Failed to resolve a DNS name " << addressString << ": " << ec.message() << "\n";
               exit(1);
            }
            for (boost::asio::ip::tcp::resolver::iterator it = endpoints.cbegin(); it != endpoints.cend(); it++) {
               const boost::asio::ip::tcp::endpoint endpoint = *it;
               HPCT_LOG(info) << addressString << " -> " << endpoint.address().to_string();
               addSourceAddress(array, endpoint.address().to_string(), false);
            }
            return true;
         }
         std::cerr << "ERROR: Bad source address " << addressParameters[0] << "!\n";
         return false;
      }
      std::map<boost::asio::ip::address, std::set<uint8_t>>::iterator found = array.find(address);
      if(found == array.end()) {
         array.insert(std::pair<boost::asio::ip::address, std::set<uint8_t>>(address, std::set<uint8_t>()));
         found = array.find(address);
         assert(found != array.end());
      }
      if(addressParameters.size() > 1) {
         for(size_t i = 1; i  < addressParameters.size(); i++) {
            unsigned int trafficClass = ~0U;
            for(size_t j = 0; j < sizeof(DSCPValuesTable) / sizeof(DSCPValue); j++) {
               if(strcmp(DSCPValuesTable[j].Name, addressParameters[i].c_str()) == 0) {
                  trafficClass = DSCPValuesTable[j].Value;
                  break;
               }
            }

            if(trafficClass == ~0U) {
               trafficClass = std::strtoul(addressParameters[i].c_str(), nullptr, 16);
               if(trafficClass > 0xff) {
                  std::cerr << "ERROR: Bad traffic class " << addressParameters[i] << "!\n";
                  return false;
               }
            }
            array[address].insert(trafficClass);
         }
      }
      else {
         array[address].insert(0x00);
      }
   }
   else {
      std::cerr << "ERROR: Invalid source address specification " << addressString << "\n";
      return false;
   }
   return true;
}


// ###### Add destination address to set ####################################
bool addDestinationAddress(std::set<boost::asio::ip::address>& array,
                           const std::string&                  addressString,
                           bool                                tryToResolve)
{
   boost::system::error_code errorCode;
   boost::asio::ip::address  address = boost::asio::ip::address::from_string(addressString, errorCode);
   if(errorCode != boost::system::errc::success) {
      if(tryToResolve) {
         boost::asio::io_service ios;
         boost::asio::ip::tcp::resolver::query resolver_query(
            addressString, "0", boost::asio::ip::tcp::resolver::query::numeric_service);
         boost::asio::ip::tcp::resolver resolver(ios);
         boost::system::error_code ec;
         const boost::asio::ip::tcp::resolver::results_type endpoints =
            resolver.resolve(resolver_query, ec);
         if(ec) {
            std::cerr << "Failed to resolve a DNS name " << addressString << ": " << ec.message() << "\n";
            exit(1);
         }
         for (boost::asio::ip::tcp::resolver::iterator it = endpoints.cbegin(); it != endpoints.cend(); it++) {
            const boost::asio::ip::tcp::endpoint endpoint = *it;
            HPCT_LOG(info) << addressString << " -> " << endpoint.address().to_string();
            addDestinationAddress(array, endpoint.address().to_string(), false);
         }
         return true;
      }
      std::cerr << "ERROR: Bad destination address " << addressString << "!\n";
      return false;
   }
   array.insert(address);
   return true;
}
