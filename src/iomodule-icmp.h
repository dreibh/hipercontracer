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

#ifndef IOMODULE_ICMP_H
#define IOMODULE_ICMP_H

#include "iomodule-base.h"

#ifdef __linux__
// linux/icmp.h defines the socket option ICMP_FILTER, but this include
// conflicts with netinet/ip_icmp.h. Just adding the needed definitions here:
#define ICMP_FILTER 1
struct icmp_filter {
   uint32_t data;
};
#endif


class ICMPModule : public IOModuleBase
{
   public:
   ICMPModule(boost::asio::io_context&                 ioContext,
              std::map<unsigned short, ResultEntry*>&  resultsMap,
              const boost::asio::ip::address&          sourceAddress,
              const uint16_t                           sourcePort,
              const uint16_t                           destinationPort,
              std::function<void (const ResultEntry*)> newResultCallback,
              const unsigned int                       packetSize);
   virtual ~ICMPModule();

   virtual const ProtocolType getProtocolType() const { return ProtocolType::PT_ICMP; }
   virtual const std::string& getProtocolName() const {
      static std::string name = "ICMP";
      return name;
   }

   virtual bool prepareSocket();
   virtual void cancelSocket();
   virtual void expectNextReply(const int  socketDescriptor,
                                const bool readFromErrorQueue);

   virtual unsigned int sendRequest(const DestinationInfo& destination,
                                    const unsigned int     fromTTL,
                                    const unsigned int     toTTL,
                                    const unsigned int     fromRound,
                                    const unsigned int     toRound,
                                    uint16_t&              seqNumber,
                                    uint32_t*              targetChecksumArray);

   void handleResponse(const boost::system::error_code& errorCode,
                       const int                        socketDescriptor,
                       const bool                       readFromErrorQueue);

   virtual void handlePayloadResponse(const int     socketDescriptor,
                                      ReceivedData& receivedData);
   virtual void handleErrorResponse(const int          socketDescriptor,
                                    ReceivedData&      receivedData,
                                    sock_extended_err* socketError);

   protected:
   void updateSendTimeInResultEntry(const sock_extended_err* socketError,
                                    const scm_timestamping*  socketTimestamping);

   boost::asio::ip::icmp::socket  ICMPSocket;

   // For ICMP type, this UDP socket is only used to generate a
   // system-unique 16-bit ICMP Identifier!
   boost::asio::ip::udp::socket   UDPSocket;
   boost::asio::ip::udp::endpoint UDPSocketEndpoint;

   char                           MessageBuffer[65536 + 40];
   char                           ControlBuffer[1024];

   private:
   bool                           ExpectingReply;
   bool                           ExpectingError;
};

#endif
