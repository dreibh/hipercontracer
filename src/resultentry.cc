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

#include "assure.h"
#include "logger.h"
#include "assure.h"
#include "resultentry.h"
#include "tools.h"

#include <boost/format.hpp>


// ###### Get ANSI coloring sequence according to hop status ################
const char* getStatusColor(const HopStatus hopStatus)
{
   if(hopStatus == Timeout) {
      return "\e[37m";
   }
   else if(hopStatus == TimeExceeded) {
      return "\e[36m";
   }
   else if(statusIsUnreachable(hopStatus)) {
      return "\e[31m";
   }
   else if(statusIsSendError(hopStatus)) {
      return "\e[31;47m";
   }
   else if(hopStatus == Success) {
      return "\e[32m";
   }
   return "\e[38m";
}


// ###### Get name for status ###############################################
#define MakeCase(x) \
   case x: \
      return #x; \
    break;
const char* getStatusName(const HopStatus hopStatus)
{
   // Mask out the flags here, so only the status remains:
   const unsigned int status = (unsigned int)hopStatus & 0xff;
   switch(status) {
      MakeCase(Success)
      MakeCase(Timeout)
      MakeCase(Unknown)
      MakeCase(TimeExceeded)
      MakeCase(UnreachableScope)
      MakeCase(UnreachableNetwork)
      MakeCase(UnreachableHost)
      MakeCase(UnreachableProtocol)
      MakeCase(UnreachablePort)
      MakeCase(UnreachableProhibited)
      MakeCase(UnreachableUnknown)
      MakeCase(NotSentGenericError)
      MakeCase(NotSentPermissionDenied)
      MakeCase(NotSentNetworkUnreachable)
      MakeCase(NotSentHostUnreachable)
      MakeCase(NotAvailableAddress)
      MakeCase(NotValidMsgSize)
      MakeCase(NotEnoughBufferSpace)
      default:
         return "Unknown";
       break;
   }
}


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
                             const unsigned short            roundNumber,
                             const unsigned short            seqNumber,
                             const unsigned int              hopNumber,
                             const unsigned int              packetSize,
                             const uint16_t                  checksum,
                             const uint16_t                  sourcePort,
                             const uint16_t                  destinationPort,
                             const ResultTimePoint&          sendTime,
                             const boost::asio::ip::address& source,
                             const DestinationInfo&          destination,
                             const HopStatus                 status)
{
   TimeStampSeqID  = timeStampSeqID;
   RoundNumber     = roundNumber;
   SeqNumber       = seqNumber;
   HopNumber       = hopNumber;
   PacketSize      = packetSize;
   ResponseSize    = 0;
   Checksum        = checksum;
   SourcePort      = sourcePort;
   DestinationPort = destinationPort;
   Source          = dropScopeID(source);
   Destination     = DestinationInfo(dropScopeID(destination.address()), destination.trafficClass(), destination.identifier());
   Hop             = Destination.address();
   Status          = status;
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


// ###### Update source address #############################################
void ResultEntry::updateSourceAddress(const boost::asio::ip::address& sourceAddress)
{
   // Only allow update from unspecified address!
   assure(Source.is_unspecified());
   Source = sourceAddress;
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
      case boost::system::errc::permission_denied:       // EACCES
         hopStatus = NotSentPermissionDenied;
       break;
      case boost::system::errc::network_unreachable:     // ENETUNREACH
         hopStatus = NotSentNetworkUnreachable;
       break;
      case boost::system::errc::host_unreachable:        // EHOSTUNREACH
         hopStatus = NotSentHostUnreachable;
       break;
      case boost::system::errc::address_not_available:   // EADDRNOTAVAIL
         hopStatus = NotAvailableAddress;
       break;
      case boost::system::errc::message_size:            // EMSGSIZE
         hopStatus = NotValidMsgSize;
       break;
      case boost::system::errc::no_buffer_space:         // ENOBUFS
         hopStatus = NotEnoughBufferSpace;
       break;
      default:   // all other errors
         HPCT_LOG(debug) << "failedToSend(" << (int)errorCode.value() << ")";
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
   assure((unsigned int)rxTimeStampType <= RXTimeStampType::RXTST_MAX);

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
bool ResultEntry::obtainSchedulingSendTime(unsigned int&    timeSource,
                                           ResultTimePoint& schedulingTime,
                                           ResultTimePoint& sendTime) const
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

   assure(SendTime[TXTST_SchedulerSW]    != ResultTimePoint());
   assure(SendTime[TXTST_TransmissionSW] != ResultTimePoint());

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


// ##### Obtain application send and scheduling time ########################
bool ResultEntry::obtainApplicationSendSchedulingTime(unsigned int&    timeSource,
                                                      ResultTimePoint& appSendTime,
                                                      ResultTimePoint& schedulingTime) const
{
   // Get send/scheduling time stamp source as byte:
   timeSource = (SendTimeSource[TXTST_SchedulerSW] << 4) | SendTimeSource[TXTST_Application];

   // ====== Time source must not be unknown ================================
   if( ((SendTimeSource[TXTST_Application] == TST_Unknown) ||
        (SendTimeSource[TXTST_SchedulerSW] == TST_Unknown)) ) {
      goto not_available;
   }

   // ====== Time sources must be the same here =============================
   if( (SendTimeSource[TXTST_Application] != TST_SysClock) ||
       (SendTimeSource[TXTST_SchedulerSW] != TST_TIMESTAMPING_SW) ) {
      goto not_available;
   }

   assure(SendTime[TXTST_Application] != ResultTimePoint());
   assure(SendTime[TXTST_SchedulerSW] != ResultTimePoint());

   // ====== Check whether the time stamps make sense =======================
   if(SendTime[TXTST_SchedulerSW] < SendTime[TXTST_Application]) {
      // Time went backwards -> clock issue (may be NTP)?
      HPCT_LOG(warning) << "Queuing time jump detected! May be NTP is adjusting the system clock?"
                        << " aO=" << timePointToString<ResultTimePoint>(SendTime[TXTST_Application], 9) << ", "
                        << " s="  << timePointToString<ResultTimePoint>(SendTime[TXTST_SchedulerSW], 9);
      goto not_available;
   }

   // Time stamps are compatible:
   appSendTime    = SendTime[TXTST_Application];
   schedulingTime = SendTime[TXTST_SchedulerSW];
   return true;

not_available:
   // At least one value is missing, or they are incompatible, or invalid:
   return false;
}


// ##### Obtain application send and scheduling time ########################
bool ResultEntry::obtainReceptionApplicationReceiveTime(unsigned int&    timeSource,
                                                        ResultTimePoint& receiveTime,
                                                        ResultTimePoint& appReceiveTime) const
{
   // Get reception/receive time stamp source as byte:
   timeSource = (ReceiveTimeSource[RXTST_Application] << 4) | ReceiveTimeSource[RXTST_ReceptionSW];

   // ====== Time source must not be unknown ================================
   if( ((ReceiveTimeSource[RXTST_ReceptionSW] == TST_Unknown) ||
        (ReceiveTimeSource[RXTST_Application] == TST_Unknown)) ) {
      goto not_available;
   }

   // ====== Time sources must be the same here =============================
   if( (ReceiveTimeSource[RXTST_ReceptionSW] != TST_TIMESTAMPING_SW) ||
       (ReceiveTimeSource[RXTST_Application] != TST_SysClock) ) {
      goto not_available;
   }

   assure(ReceiveTime[RXTST_ReceptionSW] != ResultTimePoint());
   assure(ReceiveTime[RXTST_Application] != ResultTimePoint());

   // ====== Check whether the time stamps make sense =======================
   if(ReceiveTime[RXTST_Application] < ReceiveTime[RXTST_ReceptionSW]) {
      // Time went backwards -> clock issue (may be NTP)?
      HPCT_LOG(warning) << "Queuing time jump detected! May be NTP is adjusting the system clock?"
                        << " s="  << timePointToString<ResultTimePoint>(ReceiveTime[RXTST_ReceptionSW], 9) << ", "
                        << " aI=" << timePointToString<ResultTimePoint>(ReceiveTime[RXTST_Application], 9);
      goto not_available;
   }

   // Time stamps are compatible:
   receiveTime    = ReceiveTime[RXTST_ReceptionSW];
   appReceiveTime = ReceiveTime[RXTST_Application];
   return true;

not_available:
   // At least one value is missing, or they are incompatible, or invalid:
   return false;
}


// ##### Compute RTT ########################################################
ResultDuration ResultEntry::getRTT(const RXTimeStampType rxTimeStampType,
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
ResultDuration ResultEntry::getQueuingDelay(unsigned int& timeSource) const {
   ResultTimePoint schedulingTime;
   ResultTimePoint sendTime;
   if(obtainSchedulingSendTime(timeSource, schedulingTime, sendTime)) {
      // Time stamps are compatible => compute queuing delay
      return sendTime - schedulingTime;
   }
   // Return "invalid" duration of -1ns
   return ResultDuration(std::chrono::nanoseconds(-1));
}


// ##### Compute application output delay ###################################
ResultDuration ResultEntry::getAppSendDelay(unsigned int& timeSource) const {
   ResultTimePoint appSendTime;
   ResultTimePoint schedulingTime;
   if(obtainApplicationSendSchedulingTime(timeSource, appSendTime, schedulingTime)) {
      // Time stamps are compatible => compute queuing delay
      return schedulingTime - appSendTime;
   }
   // Return "invalid" duration of -1ns
   return ResultDuration(std::chrono::nanoseconds(-1));
}


// ##### Compute application output delay ###################################
ResultDuration ResultEntry::getAppReceiveDelay(unsigned int& timeSource) const {
   ResultTimePoint receiveTime;
   ResultTimePoint appReceiveTime;
   if(obtainReceptionApplicationReceiveTime(timeSource, receiveTime, appReceiveTime)) {
      // Time stamps are compatible => compute queuing delay
      return appReceiveTime - receiveTime;
   }
   // Return "invalid" duration of -1ns
   return ResultDuration(std::chrono::nanoseconds(-1));
}


// ###### Obtain RTT and delay ##############################################
void ResultEntry::obtainResultsValues(unsigned int&   timeSource,
                                      ResultDuration& rttApplication,
                                      ResultDuration& rttSoftware,
                                      ResultDuration& rttHardware,
                                      ResultDuration& delayQueuing,
                                      ResultDuration& delayAppSend,
                                      ResultDuration& delayAppReceive) const
{
   // ====== Obtain values ==================================================
   unsigned int timeSourceApplication;
   unsigned int timeSourceSoftware;
   unsigned int timeSourceHardware;
   unsigned int timeSourceAppSend;
   unsigned int timeSourceAppReceive;
   unsigned int timeSourceQueuing;

   rttApplication  = getRTT(RXTimeStampType::RXTST_Application, timeSourceApplication);
   rttSoftware     = getRTT(RXTimeStampType::RXTST_ReceptionSW, timeSourceSoftware);
   rttHardware     = getRTT(RXTimeStampType::RXTST_ReceptionHW, timeSourceHardware);
   delayAppSend    = getAppSendDelay(timeSourceAppSend);
   delayAppReceive = getAppReceiveDelay(timeSourceAppReceive);
   delayQueuing    = getQueuingDelay(timeSourceQueuing);
   timeSource      = (timeSourceApplication << 24) |
                     (timeSourceQueuing     << 16) |
                     (timeSourceSoftware    << 8)  |
                     timeSourceHardware;

   // ====== Verify values ==================================================
   if(__builtin_expect(SendTime[TXTST_Application] + rttApplication != ReceiveTime[RXTST_Application], 0)) {
      HPCT_LOG(fatal) << "rttApplication check failed!";
      abort();
   }
   if((long long)std::chrono::duration_cast<std::chrono::nanoseconds>(rttSoftware).count() >= 0) {
      if(__builtin_expect(SendTime[TXTST_TransmissionSW] + rttSoftware != ReceiveTime[RXTST_ReceptionSW], 0)) {
         HPCT_LOG(fatal) << "rttSoftware check failed!";
         abort();
      }
   }
   if((long long)std::chrono::duration_cast<std::chrono::nanoseconds>(rttHardware).count() >= 0) {
      if(__builtin_expect(SendTime[TXTST_TransmissionHW] + rttHardware != ReceiveTime[RXTST_ReceptionHW], 0)) {
         HPCT_LOG(fatal) << "rttHardware check failed!";
         abort();
      }
   }
   if((long long)std::chrono::duration_cast<std::chrono::nanoseconds>(delayQueuing).count() >= 0) {
      if(__builtin_expect(SendTime[TXTST_SchedulerSW] + delayQueuing != SendTime[TXTST_TransmissionSW], 0)) {
         HPCT_LOG(fatal) << "delayQueuing check failed!";
         abort();
      }
   }
   if((long long)std::chrono::duration_cast<std::chrono::nanoseconds>(delayAppSend).count() >= 0) {
      if(__builtin_expect(SendTime[TXTST_Application] + delayAppSend != SendTime[TXTST_SchedulerSW], 0)) {
         HPCT_LOG(fatal) << "delayAppSend check failed!";
         abort();
      }
   }
   if((long long)std::chrono::duration_cast<std::chrono::nanoseconds>(delayAppReceive).count() >= 0) {
      if(__builtin_expect(ReceiveTime[RXTST_ReceptionSW] + delayAppReceive != ReceiveTime[RXTST_Application], 0)) {
         HPCT_LOG(fatal) << "delayAppReceive check failed!";
         abort();
      }
   }
}


// ##### Obtain most accurate RTT ###########################################
ResultDuration ResultEntry::obtainMostAccurateRTT(const RXTimeStampType rxTimeStampType,
                                                  unsigned int&         timeSource) const
{
   // Try hardware first:
   ResultDuration rttValue = getRTT(RXTimeStampType::RXTST_ReceptionHW, timeSource);
   if(rttValue.count() <= 0) {
      // Try software next:
      rttValue = getRTT(RXTimeStampType::RXTST_ReceptionSW, timeSource);
      if(rttValue.count() <= 0) {
         // Finally, try application:
         rttValue = getRTT(RXTimeStampType::RXTST_Application, timeSource);
      }
   }
   return rttValue;
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const ResultEntry& resultEntry)
{
   unsigned int   timeSource;
   ResultDuration rttApplication;
   ResultDuration rttSoftware;
   ResultDuration rttHardware;
   ResultDuration delayAppSend;
   ResultDuration delayAppReceive;
   ResultDuration delayQueuing;

   resultEntry.obtainResultsValues(timeSource,
                                   rttApplication, rttSoftware, rttHardware,
                                   delayQueuing, delayAppSend, delayAppReceive);

   os << boost::format("#%08x")           % resultEntry.TimeStampSeqID
      << "\t" << boost::format("R%d")     % resultEntry.RoundNumber
      << "\t" << boost::format("#%05d")   % resultEntry.SeqNumber
      << "\t" << boost::format("%2d")     % resultEntry.Hop
      << "\tTS:" << boost::format("%08x") % timeSource
      << "\taS:" << durationToString<ResultDuration>(delayAppSend)
      << "\tq:"  << durationToString<ResultDuration>(delayQueuing)
      << "\taR:" << durationToString<ResultDuration>(delayAppReceive)
      << "\tA:"  << durationToString<ResultDuration>(rttApplication)
      << "\tS:"  << durationToString<ResultDuration>(rttSoftware)
      << "\tH:"  << durationToString<ResultDuration>(rttHardware)
      << "\t" << boost::format("%3d")     % resultEntry.Status
      << "\t" << ((resultEntry.Checksum != 0) ? (boost::format("%04x")    % resultEntry.Checksum) :
                                                (boost::format("%d/%d")   % resultEntry.SourcePort % resultEntry.DestinationPort))
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
   return os;
}
