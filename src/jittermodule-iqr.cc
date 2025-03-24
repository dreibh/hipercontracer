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

#include "jittermodule-iqr.h"
#include "logger.h"

#include <math.h>


const std::string JitterModuleIQR::JitterNameIQR = "IQR";
const JitterType  JitterModuleIQR::JitterTypeIQR = JT_IQR;


// ###### Constructor #######################################################
JitterModuleIQR::JitterModuleIQR()
{
}


// ###### Get number of packets #############################################
unsigned int JitterModuleIQR::packets() const
{
   return RTTVector.size();
}


// ###### Get mean latency ##################################################
unsigned long long JitterModuleIQR::meanLatency() const
{
   if(RTTVector.size() > 0) {
      double rttSum = 0.0;
      for(unsigned long long rtt : RTTVector) {
         rttSum += (double)rtt;
      }
      return (unsigned long long)(rttSum / RTTVector.size());
   }
   return 0;
}


// ###### Get jitter ########################################################
unsigned long long JitterModuleIQR::jitter()
{
   if(RTTVector.size() >= 2) {
      std::sort(RTTVector.begin(), RTTVector.end());
      const unsigned int qi = RTTVector.size() / 4;
      const unsigned int qi25 = qi;
      const unsigned int qi75 = RTTVector.size() - 1 - qi;
      printf("s=%u i1=%u i2=%u\n", (unsigned int)RTTVector.size(), qi25, qi75);
      return RTTVector[qi75] - RTTVector[qi25];
   }
   return 0;
}


// ###### Process new packet's time stamps ##################################
void JitterModuleIQR::process(const uint8_t            timeSource,
                              const unsigned long long sendTimeStamp,
                              const unsigned long long receiveTimeStamp)
{
   unsigned long long rtt = receiveTimeStamp - sendTimeStamp;
   RTTVector.push_back(rtt);
   if(RTTVector.size() == 1) {
      TimeSource = timeSource;
   }
}
