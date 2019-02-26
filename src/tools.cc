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


// ###### Convert ptime to microseconds since the epoch #####################
uint64_t usSinceEpoch(const boost::posix_time::ptime& time)
{
   const static boost::posix_time::ptime epoch     = boost::posix_time::from_time_t(0);
   const boost::posix_time::time_duration duration = time - epoch;
   return duration.total_microseconds();
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
            std::cerr << "ERROR: Provided user " << user << " is not a user name or UID!" << std::endl;
            ::exit(1);
         }
      }
   }
   return pw;
}


// ###### Reduce permissions of process #####################################
bool reducePermissions(const passwd* pw, const bool verboseMode)
{
   // ====== Reduce permissions =============================================
   if((pw != NULL) && (pw->pw_uid != 0)) {
      if(verboseMode) {
         std::cerr << "NOTE: Using UID " << pw->pw_uid
                   << ", GID " << pw->pw_gid << std::endl;
      }
      if(setgid(pw->pw_gid) != 0) {
         std::cerr << "ERROR: setgid(" << pw->pw_gid << ") failed: " << strerror(errno) << std::endl;
         ::exit(1);
      }
      if(setuid(pw->pw_uid) != 0) {
         std::cerr << "ERROR: setuid(" << pw->pw_uid << ") failed: " << strerror(errno) << std::endl;
         ::exit(1);
      }
   }
   else {
      std::cerr << "NOTE: Working as root (uid 0). This is not recommended!" << std::endl;
      return false;
   }

   return true;
}
