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

#include "jitter-rfc3550.h"
#include "logger.h"


// ###### Constructor #######################################################
JitterRFC3550::JitterRFC3550()
{
   Packets    = 0;
   Jitter     = 0.0;
   LatencySum = 0.0;
}


// ###### Process new packet's time stamps ##################################
void JitterRFC3550::process(const uint8_t            timeSource,
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


#if 0
#include <stdio.h>

int main(int argc, char *argv[])
{
   JitterRFC3550 j;

   j.process(0xaa, 1000000000, 1100000000);
   j.process(0xaa, 2000000000, 2200000000);
   j.process(0xaa, 3000000000, 3100000000);
   j.process(0xaa, 4000000000, 4200000000);
   j.process(0x66, 5000000000, 5200000000);

   printf("P=%u\n", j.packets());
   printf("J=%llu\n", j.jitter() / 1000000ULL);
   printf("L=%llu\n", j.meanLatency() / 1000000ULL);

   return 0;
}
#endif
