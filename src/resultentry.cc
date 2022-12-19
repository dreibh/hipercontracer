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
#include "tools.h"

#include <boost/format.hpp>


// ###### Constructor #######################################################
ResultEntry::ResultEntry(const uint32_t                                        timeStampSeqID,
                         const unsigned short                                  round,
                         const unsigned short                                  seqNumber,
                         const unsigned int                                    hop,
                         const unsigned int                                    packetSize,
                         const uint16_t                                        checksum,
                         const std::chrono::high_resolution_clock::time_point& sendTime,
                         const boost::asio::ip::address&                       source,
                         const DestinationInfo&                                destination,
                         const HopStatus                                       status)
   : TimeStampSeqID(timeStampSeqID),
     Round(round),
     SeqNumber(seqNumber),
     Hop(hop),
     PacketSize(packetSize),
     Checksum(checksum),
     Source(source),
     Destination(destination),
     Status(status)
{
   setSendTime(TXTimeStampType::TXTST_Application, TimeSourceType::TST_SysClock, sendTime);
}


// ###### Destructor ########################################################
ResultEntry::~ResultEntry()
{
}


// ##### Compute RTT ########################################################
std::chrono::high_resolution_clock::duration ResultEntry::rtt(const RXTimeStampType rxTimeStampType) const {
   assert((unsigned int)rxTimeStampType <= RXTimeStampType::RXTST_MAX);
   // NOTE: Indexing for both arrays (RX, TX) is the same!
   if( (ReceiveTime[rxTimeStampType] == std::chrono::high_resolution_clock::time_point())  ||
       (SendTime[rxTimeStampType]    == std::chrono::high_resolution_clock::time_point()) ) {
      // At least one value is missing -> return "invalid" duration.
      return std::chrono::high_resolution_clock::duration::min();
   }
   return(ReceiveTime[rxTimeStampType] - SendTime[rxTimeStampType]);
}


// ##### Compute queuing delay ##############################################
std::chrono::high_resolution_clock::duration ResultEntry::queuingDelay() const {
   if( (SendTime[TXTST_TransmissionSW] == std::chrono::high_resolution_clock::time_point())  ||
       (SendTime[TXTST_SchedulerSW]    == std::chrono::high_resolution_clock::time_point()) ) {
      // At least one value is missing -> return "invalid" duration.
      return std::chrono::high_resolution_clock::duration::min();
   }
   return(SendTime[TXTST_TransmissionSW] - SendTime[TXTST_SchedulerSW]);
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry)
{
   const std::chrono::high_resolution_clock::duration rttApplication = resultEntry.rtt(RXTimeStampType::RXTST_Application);
   const std::chrono::high_resolution_clock::duration rttSoftware    = resultEntry.rtt(RXTimeStampType::RXTST_ReceptionSW);
   const std::chrono::high_resolution_clock::duration rttHardware    = resultEntry.rtt(RXTimeStampType::RXTST_ReceptionHW);
   const std::chrono::high_resolution_clock::duration queuingDelay   = resultEntry.queuingDelay();

   os << boost::format("#%08x")           % resultEntry.TimeStampSeqID
      << "\t" << boost::format("R%d")     % resultEntry.Round
      << "\t" << boost::format("#%05d")   % resultEntry.SeqNumber
      << "\t" << boost::format("%2d")     % resultEntry.Hop
      << "\tA:" << durationToString<std::chrono::high_resolution_clock::duration>(rttApplication)
      << "\tS:" << durationToString<std::chrono::high_resolution_clock::duration>(rttSoftware)
      << "\tH:" << durationToString<std::chrono::high_resolution_clock::duration>(rttHardware)
      << "\tq:" << durationToString<std::chrono::high_resolution_clock::duration>(queuingDelay)
      << "\t" << boost::format("%3d")     % resultEntry.Status
      << "\t" << boost::format("%04x")    % resultEntry.Checksum
      << "\t" << boost::format("%d")      % resultEntry.PacketSize
      << "\t" << resultEntry.Destination;

   os << "\n"
      << "Ap: " << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_Application))    << " -> "
                << nsSinceEpoch(resultEntry.receiveTime(RXTimeStampType::RXTST_Application)) << "\n"

      << "Sw: " << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_SchedulerSW))    << " -> "
                << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_TransmissionSW)) << " -> "
                << nsSinceEpoch(resultEntry.receiveTime(RXTimeStampType::RXTST_ReceptionSW)) << "\n"

      << "Hw: " << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_TransmissionHW)) << " -> "
                << nsSinceEpoch(resultEntry.receiveTime(RXTimeStampType::RXTST_ReceptionHW)) << "\n";
   return(os);
}
