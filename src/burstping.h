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
// Copyright (C) 2015-2020 by Thomas Dreibholz
// Copyright (C) 2020 Alfred Arouna
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
// Contact: alfred@simula.no

#ifndef BURSTPING_H
#define BURSTPING_H

#include "ping.h"


class Burstping : public Ping
{
   public:
   Burstping(ResultsWriter*              resultsWriter,
        const unsigned int               iterations,
        const bool                       removeDestinationAfterRun,
        const boost::asio::ip::address&  sourceAddress,
        const std::set<DestinationInfo>& destinationArray,
        const unsigned long long         interval   =  1000,
        const unsigned int               expiration = 10000,
        const unsigned int               ttl        =    64,
        const unsigned int               payload    =    56,
        const unsigned int               burst      =    1);
   virtual ~Burstping();

   virtual const std::string& getName() const;

   protected:
   virtual void sendRequests();
   virtual void sendBurstICMPRequest(const DestinationInfo& destination,
                        const unsigned int             ttl,
                        const unsigned int             round,
                        uint32_t&                      targetChecksum,
                        const unsigned int             payload);

   private:
   const std::string BurstpingInstanceName;
   const unsigned int payload;
   const unsigned int burst;
};

#endif
