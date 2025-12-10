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

#ifndef JITTERMODULE_RFC3550_H
#define JITTERMODULE_RFC3550_H

#include "jittermodule-base.h"


class JitterModuleRFC3550 : public JitterModuleBase
{
   public:
   JitterModuleRFC3550(const unsigned int elements);

   virtual const JitterType   getJitterType() const { return JitterModuleRFC3550::JitterTypeRFC3550; }
   virtual const std::string& getJitterName() const { return JitterModuleRFC3550::JitterNameRFC3550; }

   virtual unsigned int       packets()       const;
   virtual unsigned long long meanLatency()   const;
   virtual unsigned long long jitter();
   virtual void process(const uint8_t            timeSource,
                        const unsigned long long sendTimeStamp,
                        const unsigned long long receiveTimeStamp);

   private:
   static const std::string JitterNameRFC3550;
   static const JitterType  JitterTypeRFC3550;

   unsigned long long       PrevSendTimeStamp;
   unsigned long long       PrevReceiveTimeStamp;
   unsigned int             Packets;
   double                   Jitter;
   double                   LatencySum;
   uint8_t                  TimeSource;
};

#endif
