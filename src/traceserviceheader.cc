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

#include "traceserviceheader.h"
#include "tools.h"


static ResultTimePoint getHiPerConTracerEpoch();


const ResultTimePoint HiPerConTracerEpoch = getHiPerConTracerEpoch();


// ###### Get HiPerConTracer Epoch (1976-09-29, 00:00:00 UTC) ###############
static ResultTimePoint getHiPerConTracerEpoch()
{
   // NOTE: This computation is only used for the send time stamp inside
   //       the packets (e.g. for Wireshark analysis). It is *not* used
   //       to compute packet timing!

   static const ResultTimePoint nowRT = nowInUTC<ResultTimePoint>();
   static const SystemTimePoint nowST = nowInUTC<SystemTimePoint>();

   // For HiPerConTracer packets: time stamp is microseconds since 1976-09-29.
   static const SystemTimePoint hiPerConTracerEpochST             = SystemClock::from_time_t(212803200);
   static const SystemDuration durationSinceHiPerConTracerEpochST = nowST - hiPerConTracerEpochST;
   static const std::chrono::seconds secsSinceHiPerConTracerEpoch(std::chrono::duration_cast<std::chrono::seconds>(durationSinceHiPerConTracerEpochST).count());

   static const ResultTimePoint hiPerConTracerEpochRT = nowRT - secsSinceHiPerConTracerEpoch;
   return hiPerConTracerEpochRT;
}
