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

#include "internet16.h"


// ###### Internet-16 checksum according to RFC 1071, computation part ######
void computeInternet16(uint32_t& sum, const uint8_t* data, const unsigned int datalen)
{
   const uint8_t*       ptr = data;
   const uint8_t* const end = &data[datalen];

   // ------ Compute checksum in steps of 20 bytes --------------------------
   // 20-bytes-steps handle an IPv4 header in 1 step, an IPv6 header in 2 steps.
   while(ptr + 19 < end) {
      sum = sum +
         ((const uint16_t*)ptr)[0] +
         ((const uint16_t*)ptr)[1] +
         ((const uint16_t*)ptr)[2] +
         ((const uint16_t*)ptr)[3] +
         ((const uint16_t*)ptr)[4] +
         ((const uint16_t*)ptr)[5] +
         ((const uint16_t*)ptr)[6] +
         ((const uint16_t*)ptr)[7] +
         ((const uint16_t*)ptr)[8] +
         ((const uint16_t*)ptr)[9];
      ptr += 20;
   }

   // ------ Compute checksum in steps of 8 bytes ---------------------------
   // 8-bytes-steps handle an ICMP or UDP header in 1 step, and a default-sized
   // TraceServiceHeader in 2 steps.
   while(ptr + 7 < end) {
      sum = sum +
         ((const uint16_t*)ptr)[0] +
         ((const uint16_t*)ptr)[1] +
         ((const uint16_t*)ptr)[2] +
         ((const uint16_t*)ptr)[3];
      ptr += 8;
   }

   // ------ Compute checksum in steps of 2 bytes ---------------------------
   while(ptr + 1 < end) {
      sum = sum + *(const uint16_t*)ptr;
      ptr += 2;
   }

   // ------ Handle a final byte --------------------------------------------
   if(ptr < end) {
      sum = sum + *ptr;
   }
}
