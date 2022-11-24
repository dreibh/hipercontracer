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

#include "resultentry.h"

#include <boost/format.hpp>


// ###### Constructor #######################################################
ResultEntry::ResultEntry(const uint32_t                                        timeStampSeqID,
                         const unsigned short                                  round,
                         const unsigned short                                  seqNumber,
                         const unsigned int                                    hop,
                         const unsigned int                                    packetSize,
                         const uint16_t                                        checksum,
                         const std::chrono::high_resolution_clock::time_point& sendTime,
                         const DestinationInfo&                                destination,
                         const HopStatus                                       status)
   : TimeStampSeqID(timeStampSeqID),
     Round(round),
     SeqNumber(seqNumber),
     Hop(hop),
     PacketSize(packetSize),
     Checksum(checksum),
     Destination(destination),
     Status(status)
{
   setSendTime(TXTimeStampType::TXTST_Application, TimeSource::TS_SysClock, sendTime);
}


// ###### Destructor ########################################################
ResultEntry::~ResultEntry()
{
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry)
{
   os << boost::format("#%08xu")          % resultEntry.TimeStampSeqID
      << boost::format("R%d")             % resultEntry.Round
      << "\t" << boost::format("#%05d")   % resultEntry.SeqNumber
      << "\t" << boost::format("%2d")     % resultEntry.Hop
      << "\tA:" << boost::format("%9.6fms") % (std::chrono::duration_cast<std::chrono::nanoseconds>(resultEntry.rtt(RXTimeStampType::RXTST_Application)).count() / 1000000.0)
      << "\tS:" << boost::format("%9.6fms") % (std::chrono::duration_cast<std::chrono::nanoseconds>(resultEntry.rtt(RXTimeStampType::RXTST_ReceptionSW)).count() / 1000000.0)
      << "\tH:" << boost::format("%9.6fms") % (std::chrono::duration_cast<std::chrono::nanoseconds>(resultEntry.rtt(RXTimeStampType::RXTST_ReceptionHW)).count() / 1000000.0)
      << "\t" << boost::format("%3d")     % resultEntry.Status
      << "\t" << boost::format("%04x")    % resultEntry.Checksum
      << "\t" << boost::format("%d")      % resultEntry.PacketSize
      << "\t" << resultEntry.Destination;
   return(os);
}
