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
// Copyright (C) 2015-2026 by Thomas Dreibholz
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

#include "jittermodule-rfc3550.h"
#include "logger.h"

#include <math.h>


const std::string JitterModuleRFC3550::JitterNameRFC3550 = "RFC3550";
const JitterType  JitterModuleRFC3550::JitterTypeRFC3550 = JT_RFC3550;


// ###### Constructor #######################################################
JitterModuleRFC3550::JitterModuleRFC3550(const unsigned int elements)
{
   Packets              = 0;
   Jitter               = 0.0;
   LatencySum           = 0.0;
   PrevSendTimeStamp    = 0;
   PrevReceiveTimeStamp = 0;
}


// ###### Get number of packets #############################################
unsigned int JitterModuleRFC3550::packets() const
{
   return Packets;
}


// ###### Get mean latency ##################################################
unsigned long long JitterModuleRFC3550::meanLatency() const
{
   if(Packets > 0) {
      return (unsigned long long)(LatencySum / Packets);
   }
   return 0;
}


// ###### Get jitter ########################################################
unsigned long long JitterModuleRFC3550::jitter()
{
   return (unsigned long long)rint(Jitter);
}


// ###### Process new packet's time stamps ##################################
void JitterModuleRFC3550::process(const uint8_t            timeSource,
                                  const unsigned long long sendTimeStamp,
                                  const unsigned long long receiveTimeStamp)
{
   if(Packets > 0) {
      if(timeSource != TimeSource) {
         // In some rare cases, the kernel seems to not deliver HW/SW time
         // stamps for the reception. The SW time stamp gets replaced by
         // the application time, but this is incompatible to SW time stamps.
         // => Not using such time stamps for jitter computation.

         // The time source has changed => do not accept these time stamps.
         HPCT_LOG(debug) << "Ignoring packet with incompatible time source "
                         << std::hex << (int)timeSource << " vs. " << (int)TimeSource;
         return;
      }

      // Jitter calculation according to Subsubsection 6.4.1 of RFC 3550:
      const double difference =
         ((double)receiveTimeStamp - (double)sendTimeStamp) -
         ((double)PrevReceiveTimeStamp - (double)PrevSendTimeStamp);
      Jitter = Jitter +  fabs(difference) / 16.0;
   }
   else {
      TimeSource = timeSource;
   }
   Packets++;
   LatencySum += (receiveTimeStamp - sendTimeStamp);
   PrevSendTimeStamp    = sendTimeStamp;
   PrevReceiveTimeStamp = receiveTimeStamp;
}
