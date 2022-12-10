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

#ifndef INTERNET16_H
#define INTERNET16_H

#include <stdint.h>


// ###### Internet-16 checksum according to RFC 1071, computation part ######
template <typename Iterator> void processInternet16(uint32_t& sum, Iterator bodyBegin, Iterator bodyEnd)
{
   Iterator body_iter = bodyBegin;
   while (body_iter != bodyEnd) {
      sum += (static_cast<uint8_t>(*body_iter++) << 8);
      if (body_iter != bodyEnd) {
         sum += static_cast<uint8_t>(*body_iter++);
      }
   }
}


// ###### Internet-16 checksum according to RFC 1071, final part ############
inline uint16_t finishInternet16(uint32_t sum)
{
   sum = (sum >> 16) + (sum & 0xFFFF);
   sum += (sum >> 16);
   return static_cast<uint16_t>(~sum);
}

#endif
