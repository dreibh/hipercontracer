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

#ifndef JITTER_RFC3550_H
#define JITTER_RFC3550_H

#include <math.h>
#include <stdint.h>


class JitterRFC3550
{
   public:
   JitterRFC3550();

   inline unsigned int       packets()     const { return Packets;                          }
   inline unsigned long long jitter()      const { return (unsigned long long)rint(Jitter); }
   inline unsigned long long meanLatency() const {
      if(Packets > 0) {
         return (unsigned long long)(LatencySum / Packets);
      }
      return 0;
   }

   void process(const uint8_t            timeSource,
                const unsigned long long sendTimeStamp,
                const unsigned long long receiveTimeStamp);

   private:
   unsigned long long PrevSendTimeStamp;
   unsigned long long PrevReceiveTimeStamp;
   unsigned int       Packets;
   double             Jitter;
   double             LatencySum;
   uint8_t            TimeSource;
};

#endif
