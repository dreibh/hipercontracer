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

#ifndef JITTER_H
#define JITTER_H

#include "ping.h"


class Jitter : public Traceroute
{
   public:
   Jitter(const std::string                moduleName,
          ResultsWriter*                   resultsWriter,
          const OutputFormatType           outputFormat,
          const unsigned int               iterations,
          const bool                       removeDestinationAfterRun,
          const boost::asio::ip::address&  sourceAddress,
          const std::set<DestinationInfo>& destinationArray,
          const unsigned long long         interval        =  1000,
          const unsigned int               expiration      = 10000,
          const unsigned int               rounds          =    16,
          const unsigned int               ttl             =    64,
          const unsigned int               packetSize      =     0,
          const uint16_t                   destinationPort =     7);
   virtual ~Jitter();

   virtual const std::string& getName() const;

   protected:
   virtual void processResults();

   private:
   const std::string JitterInstanceName;
};

#endif