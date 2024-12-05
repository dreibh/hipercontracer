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

#include <chrono>
#include <iostream>
#include <sys/utsname.h>

#include <boost/version.hpp>

#include "check.h"
#include "package-version.h"
#include "tools.h"


// ###### Check environment #################################################
void checkEnvironment(const char* programName)
{
   std::cout << programName << " " << HPCT_VERSION << "\n";

   // ====== System information =============================================
   utsname sysInfo;
   if(uname(&sysInfo) == 0) {
      std::cout << "System Information:\n"
                << "* System: \t" << sysInfo.sysname  << "\n"
                << "* Name:   \t" << sysInfo.nodename << "\n"
                << "* Release:\t" << sysInfo.release  << "\n"
                << "* Version:\t" << sysInfo.version  << "\n"
                << "* Machine:\t" << sysInfo.machine  << "\n";
   }

   // ====== Build environment ==============================================
   std::cout << "Build Environment:\n"
             << "* BOOST Version:  \t" << BOOST_VERSION  << "\n"
             << "* BOOST Compiler: \t" << BOOST_COMPILER << "\n"
             << "* BOOST StdLib:   \t" << BOOST_STDLIB   << "\n"
             << "* C++ Standard:   \t" << __cplusplus    << "\n";
#if 0
#ifdef BOOST_HAS_CLOCK_GETTIME
   std::cout << "* clock_gettime():\tyes\n";
#else
   std::cout << "* clock_gettime():\tno\n";
#endif
#ifdef BOOST_HAS_GETTIMEOFDAY
   std::cout << "* gettimeofday(): \tyes\n";
#else
   std::cout << "* gettimeofday(): \tno\n";
#endif
#endif

   // ====== Clock granularities ============================================
   const std::chrono::time_point<std::chrono::system_clock>          n1a = std::chrono::system_clock::now();
   // const std::chrono::time_point<std::chrono::system_clock>          n1b = nowInUTC<std::chrono::time_point<std::chrono::system_clock>>();
   const std::chrono::time_point<std::chrono::steady_clock>          n2a = std::chrono::steady_clock::now();
   const std::chrono::time_point<std::chrono::steady_clock>          n2b = nowInUTC<std::chrono::time_point<std::chrono::steady_clock>>();
   const std::chrono::time_point<std::chrono::high_resolution_clock> n3a = std::chrono::high_resolution_clock::now();
   const std::chrono::time_point<std::chrono::high_resolution_clock> n3b = nowInUTC<std::chrono::time_point<std::chrono::high_resolution_clock>>();

   timespec ts1;
   timespec ts2;
   clock_getres(CLOCK_REALTIME,  &ts1);
   clock_getres(CLOCK_MONOTONIC, &ts2);

   std::cout << "Clocks Granularities:\n"

             << "* std::chrono::system_clock:        \t"
             << std::chrono::time_point<std::chrono::system_clock>::period::num << "/"
             << std::chrono::time_point<std::chrono::system_clock>::period::den << " s\t"
             << (std::chrono::system_clock::is_steady ? "steady    " : "not steady") << "\t"
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n1a.time_since_epoch()).count() << " ns\n"
             // << std::chrono::duration_cast<std::chrono::nanoseconds>(n1b.time_since_epoch()).count() << " ns since epoch\n"

             << "* std::chrono::steady_clock:        \t"
             << std::chrono::time_point<std::chrono::steady_clock>::period::num << "/"
             << std::chrono::time_point<std::chrono::steady_clock>::period::den << " s\t"
             << (std::chrono::steady_clock::is_steady ? "steady    " : "not steady") << "\t"
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n2a.time_since_epoch()).count() << " ns / "
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n2b.time_since_epoch()).count() << " ns since epoch\n"

             << "* std::chrono::high_resolution_clock:\t"
             << std::chrono::time_point<std::chrono::high_resolution_clock>::period::num << "/"
             << std::chrono::time_point<std::chrono::high_resolution_clock>::period::den << " s\t"
             << (std::chrono::high_resolution_clock::is_steady ? "steady    " : "not steady") << "\t"
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n3a.time_since_epoch()).count() << " ns / "
             << std::chrono::duration_cast<std::chrono::nanoseconds>(n3b.time_since_epoch()).count() << " ns since epoch\n"

             << "* clock_getres(CLOCK_REALTIME):  s=" << ts1.tv_sec << " ns=" << ts1.tv_nsec << "\n"
             << "* clock_getres(CLOCK_MONOTONIC): s=" << ts2.tv_sec << " ns=" << ts2.tv_nsec << "\n"

             ;
}
