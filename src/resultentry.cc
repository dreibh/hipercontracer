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

#include "logger.h"
#include "resultentry.h"
#include "tools.h"

#include <boost/format.hpp>


// ###### Constructor #######################################################
ResultEntry::ResultEntry()
{
}


// ###### Destructor ########################################################
ResultEntry::~ResultEntry()
{
}


// ###### Initialise ########################################################
void ResultEntry::initialise(const uint32_t                  timeStampSeqID,
                             const unsigned short            round,
                             const unsigned short            seqNumber,
                             const unsigned int              hop,
                             const unsigned int              packetSize,
                             const uint16_t                  checksum,
                             const ResultTimePoint&          sendTime,
                             const boost::asio::ip::address& source,
                             const DestinationInfo&          destination,
                             const HopStatus                 status)
{
   TimeStampSeqID = timeStampSeqID;
   Round          = round;
   SeqNumber      = seqNumber;
   Hop            = hop;
   PacketSize     = packetSize;
   Checksum       = checksum;
   Source         = source;
   Destination    = destination;
   Status         = status;
   for(unsigned int i = 0; i < TXTST_MAX + 1; i++) {
      SendTimeSource[i] = TimeSourceType::TST_Unknown;
   }
   for(unsigned int i = 0; i < RXTST_MAX + 1; i++) {
      ReceiveTimeSource[i] = TimeSourceType::TST_Unknown;
   }
   setSendTime(TXTimeStampType::TXTST_Application,    TimeSourceType::TST_SysClock, sendTime);
   // Set TXTST_TransmissionSW to system clock for now. It may be updated later!
   setSendTime(TXTimeStampType::TXTST_TransmissionSW, TimeSourceType::TST_SysClock, sendTime);
}


// ###### Expire ############################################################
void ResultEntry::expire(const unsigned int expiration)
{
   setStatus(Timeout);
   setReceiveTime(RXTimeStampType::RXTST_Application,
                  TimeSourceType::TST_SysClock,
                  sendTime(TXTimeStampType::TXTST_Application) +
                     std::chrono::milliseconds(expiration));
}


// ###### Expire ############################################################
void ResultEntry::failedToSend(const boost::system::error_code& errorCode)
{
   HopStatus hopStatus;
   switch(errorCode.value()) {
      case boost::system::errc::permission_denied:
         hopStatus = NotSentPermissionDenied;
       break;
      case boost::system::errc::network_unreachable:
         hopStatus = NotSentNetworkUnreachable;
       break;
      case boost::asio::error::host_unreachable:
         hopStatus = NotSentHostUnreachable;
       break;
      default:
         hopStatus = NotSentGenericError;
       break;
   }
   setStatus(hopStatus);
   setReceiveTime(RXTimeStampType::RXTST_Application,
                  TimeSourceType::TST_SysClock,
                  sendTime(TXTimeStampType::TXTST_Application));
}


// ##### Obtain send and receive time #######################################
bool ResultEntry::obtainSendReceiveTime(const RXTimeStampType rxTimeStampType,
                                        unsigned int&         timeSource,
                                        ResultTimePoint&      sendTime,
                                        ResultTimePoint&      receiveTime) const
{
   assert((unsigned int)rxTimeStampType <= RXTimeStampType::RXTST_MAX);

   // Get receiver/sender time stamp source as byte:
   timeSource = (ReceiveTimeSource[rxTimeStampType] << 4) | SendTimeSource[rxTimeStampType];

   // ====== Time source must not be unknown ================================
   if( ((ReceiveTimeSource[rxTimeStampType] == TST_Unknown) ||
        (SendTimeSource[rxTimeStampType]    == TST_Unknown)) ) {
      goto not_available;
   }

   // ====== Hardware timestamps are raw, not system time ===================
   // Hardware time stamps are only compatible to hardware time stamps!
   if( ( ((ReceiveTimeSource[rxTimeStampType] == TST_TIMESTAMPING_HW) ||
          (SendTimeSource[rxTimeStampType]    == TST_TIMESTAMPING_HW)) ) &&
       (ReceiveTimeSource[rxTimeStampType] != SendTimeSource[rxTimeStampType]) ) {
      goto not_available;
   }

   // ====== Check whether there are time stamps available ==================
   // NOTE: Indexing for both arrays (RX, TX) is the same!
   if( (ReceiveTime[rxTimeStampType] == ResultTimePoint())  ||
       (SendTime[rxTimeStampType]    == ResultTimePoint()) ) {
      HPCT_LOG(warning) << "Time stamp(s) not set?!";
/*
      std::cout << "\n"
         << "Ap: " << nsSinceEpoch(sendTime(TXTimeStampType::TXTST_Application))    << " -> "
                   << nsSinceEpoch(receiveTime(RXTimeStampType::RXTST_Application)) << "\n"

         << "Sw: " << nsSinceEpoch(sendTime(TXTimeStampType::TXTST_SchedulerSW))    << " -> "
                   << nsSinceEpoch(sendTime(TXTimeStampType::TXTST_TransmissionSW)) << " -> "
                   << nsSinceEpoch(receiveTime(RXTimeStampType::RXTST_ReceptionSW)) << "\n"

         << "Hw: " << nsSinceEpoch(sendTime(TXTimeStampType::TXTST_TransmissionHW)) << " -> "
                   << nsSinceEpoch(receiveTime(RXTimeStampType::RXTST_ReceptionHW)) << "\n";
*/
      goto not_available;
   }

   // ====== Check whether the time stamps make sense =======================
   if(SendTime[rxTimeStampType] > ReceiveTime[rxTimeStampType]) {
      // Time went backwards -> clock issue (may be NTP)?
      HPCT_LOG(warning) << "Send/receive time jump detected! May be NTP is adjusting the system clock?"
                        << " s=" << timePointToString<ResultTimePoint>(SendTime[rxTimeStampType], 9) << ", "
                        << " r=" << timePointToString<ResultTimePoint>(ReceiveTime[rxTimeStampType], 9);
      goto not_available;
   }

   // Time stamps are compatible:
   sendTime    = SendTime[rxTimeStampType];
   receiveTime = ReceiveTime[rxTimeStampType];
   return true;

not_available:
   // At least one value is missing, or they are incompatible, or invalid:
   return false;
}


// ##### Obtain scheduling and send time #####################################
bool ResultEntry::obtainSchedulingSendTime(unsigned int&         timeSource,
                                           ResultTimePoint&      schedulingTime,
                                           ResultTimePoint&      sendTime) const
{
   // Get send/scheduling time stamp source as byte:
   timeSource = (SendTimeSource[TXTST_TransmissionSW] << 4) | SendTimeSource[TXTST_SchedulerSW];

   // ====== Time source must not be unknown ================================
   if( ((SendTimeSource[TXTST_SchedulerSW]    == TST_Unknown) ||
        (SendTimeSource[TXTST_TransmissionSW] == TST_Unknown)) ) {
      goto not_available;
   }

   // ====== Time sources must be the same here =============================
   // Linux: time source must be from kernel (TST_TIMESTAMPING_SW)
   if(SendTimeSource[TXTST_SchedulerSW] != SendTimeSource[TXTST_TransmissionSW]) {
      // This is the case when TXTST_TransmissionSW is TST_SysClock
      // (as initialised by ResultEntry::initialise()) -> no queuing delay!
      goto not_available;
   }

   assert(SendTime[TXTST_SchedulerSW]    != ResultTimePoint());
   assert(SendTime[TXTST_TransmissionSW] != ResultTimePoint());

   // ====== Check whether the time stamps make sense =======================
   if(SendTime[TXTST_SchedulerSW] > SendTime[TXTST_TransmissionSW]) {
      // Time went backwards -> clock issue (may be NTP)?
      HPCT_LOG(warning) << "Queuing time jump detected! May be NTP is adjusting the system clock?"
                        << " q=" << timePointToString<ResultTimePoint>(SendTime[TXTST_SchedulerSW], 9) << ", "
                        << " s=" << timePointToString<ResultTimePoint>(SendTime[TXTST_TransmissionSW], 9);
      goto not_available;
   }

   // Time stamps are compatible:
   schedulingTime = SendTime[TXTST_SchedulerSW];
   sendTime       = SendTime[TXTST_TransmissionSW];
   return true;

not_available:
   // At least one value is missing, or they are incompatible, or invalid:
   return false;
}


// ##### Obtain most accurate RTT ###########################################
ResultDuration ResultEntry::obtainMostAccurateRTT(const RXTimeStampType rxTimeStampType,
                                                  unsigned int&         timeSource) const
{
   // Try hardware first:
   ResultDuration rttValue = rtt(RXTimeStampType::RXTST_ReceptionHW, timeSource);
   if(rttValue.count() <= 0) {
      // Try software next:
      rttValue = rtt(RXTimeStampType::RXTST_ReceptionSW, timeSource);
      if(rttValue.count() <= 0) {
         // Finally, try application:
         rttValue = rtt(RXTimeStampType::RXTST_Application, timeSource);
      }
   }
   return rttValue;
}


// ##### Compute RTT ########################################################
ResultDuration ResultEntry::rtt(const RXTimeStampType rxTimeStampType,
                                unsigned int&         timeSource) const
{
   ResultTimePoint sendTime;
   ResultTimePoint receiveTime;
   if(obtainSendReceiveTime(rxTimeStampType, timeSource, sendTime, receiveTime)) {
      // Time stamps are available and compatible => compute RTT
      return receiveTime - sendTime;
   }
   // Return "invalid" duration of -1ns
   return ResultDuration(std::chrono::nanoseconds(-1));
}


// ##### Compute queuing delay ##############################################
ResultDuration ResultEntry::queuingDelay(unsigned int& timeSource) const {
   ResultTimePoint schedulingTime;
   ResultTimePoint sendTime;
   if(obtainSchedulingSendTime(timeSource, schedulingTime, sendTime)) {
      // Time stamps are compatible => compute queuing delay
      return sendTime - schedulingTime;
   }
   // Return "invalid" duration of -1ns
   return ResultDuration(std::chrono::nanoseconds(-1));
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry)
{
   unsigned int timeSourceApplication;
   unsigned int timeSourceQueuing;
   unsigned int timeSourceSoftware;
   unsigned int timeSourceHardware;
   const ResultDuration rttApplication = resultEntry.rtt(RXTimeStampType::RXTST_Application, timeSourceApplication);
   const ResultDuration rttSoftware    = resultEntry.rtt(RXTimeStampType::RXTST_ReceptionSW, timeSourceSoftware);
   const ResultDuration rttHardware    = resultEntry.rtt(RXTimeStampType::RXTST_ReceptionHW, timeSourceHardware);
   const ResultDuration queuingDelay   = resultEntry.queuingDelay(timeSourceQueuing);
   const unsigned int   timeSource     = (timeSourceApplication << 24) |
                                         (timeSourceQueuing     << 16) |
                                         (timeSourceSoftware    << 8) |
                                         timeSourceHardware;

   os << boost::format("#%08x")           % resultEntry.TimeStampSeqID
      << "\t" << boost::format("R%d")     % resultEntry.Round
      << "\t" << boost::format("#%05d")   % resultEntry.SeqNumber
      << "\t" << boost::format("%2d")     % resultEntry.Hop
      << "\tTS:" << boost::format("%08x") % timeSource
      << "\tA:" << durationToString<ResultDuration>(rttApplication)
      << "\tS:" << durationToString<ResultDuration>(rttSoftware)
      << "\tH:" << durationToString<ResultDuration>(rttHardware)
      << "\tq:" << durationToString<ResultDuration>(queuingDelay)
      << "\t" << boost::format("%3d")     % resultEntry.Status
      << "\t" << boost::format("%04x")    % resultEntry.Checksum
      << "\t" << boost::format("%d")      % resultEntry.PacketSize
      << "\t" << resultEntry.Destination;

#if 0
   os << "\n"
      << "Ap: " << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_Application))    << " -> "
                << nsSinceEpoch(resultEntry.receiveTime(RXTimeStampType::RXTST_Application)) << "\n"

      << "Sw: " << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_SchedulerSW))    << " -> "
                << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_TransmissionSW)) << " -> "
                << nsSinceEpoch(resultEntry.receiveTime(RXTimeStampType::RXTST_ReceptionSW)) << "\n"

      << "Hw: " << nsSinceEpoch(resultEntry.sendTime(TXTimeStampType::TXTST_TransmissionHW)) << " -> "
                << nsSinceEpoch(resultEntry.receiveTime(RXTimeStampType::RXTST_ReceptionHW)) << "\n";
#endif
   return(os);
}
