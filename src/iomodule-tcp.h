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

#ifndef IOMODULE_TCP_H
#define IOMODULE_TCP_H

#include "iomodule-icmp.h"


class raw_tcp
{
   public:
   typedef boost::asio::ip::basic_endpoint<raw_tcp> endpoint;
   typedef boost::asio::basic_raw_socket<raw_tcp>   socket;
   typedef boost::asio::ip::basic_resolver<raw_tcp> resolver;

   explicit raw_tcp() : Protocol(IPPROTO_TCP), Family(AF_INET) { }
   explicit raw_tcp(int protocol, int family) : Protocol(protocol), Family(family) { }

   static raw_tcp v4() { return raw_tcp(IPPROTO_TCP, AF_INET);  }
   static raw_tcp v6() { return raw_tcp(IPPROTO_TCP, AF_INET6); }

   int type()     const { return SOCK_RAW; }
   int protocol() const { return Protocol; }
   int family()   const { return Family;   }

   friend bool operator==(const raw_tcp& p1, const raw_tcp& p2) {
      return p1.Protocol == p2.Protocol && p1.Family == p2.Family;
   }
   friend bool operator!=(const raw_tcp& p1, const raw_tcp& p2) {
      return p1.Protocol != p2.Protocol || p1.Family != p2.Family;
   }

   private:
   int Protocol;
   int Family;
};


class TCPHeader;

class TCPModule : public ICMPModule
{
   public:
   TCPModule(boost::asio::io_service&                 ioService,
             std::map<unsigned short, ResultEntry*>&  resultsMap,
             const boost::asio::ip::address&          sourceAddress,
             const uint16_t                           sourcePort,
             const uint16_t                           destinationPort,
             std::function<void (const ResultEntry*)> newResultCallback,
             const unsigned int                       packetSize);
   virtual ~TCPModule();

   virtual const ProtocolType getProtocolType() const { return ProtocolType::PT_TCP; }
   virtual const std::string& getProtocolName() const {
      static std::string name = "TCP";
      return name;
   }

   virtual bool prepareSocket();
   virtual void cancelSocket();

   virtual void expectNextReply(const int  socketDescriptor,
                                const bool readFromErrorQueue);
   virtual void handlePayloadResponse(const int     socketDescriptor,
                                      ReceivedData& receivedData);
   virtual void handleErrorResponse(const int          socketDescriptor,
                                    ReceivedData&      receivedData,
                                    sock_extended_err* socketError);

   virtual unsigned int sendRequest(const DestinationInfo& destination,
                                    const unsigned int     fromTTL,
                                    const unsigned int     toTTL,
                                    const unsigned int     fromRound,
                                    const unsigned int     toRound,
                                    uint16_t&              seqNumber,
                                    uint32_t*              targetChecksumArray);

   protected:
   static bool extractSeqNumberFromTimestampOption(const TCPHeader& tcpHeader,
                                                   uint32_t&        timeStampValue,
                                                   uint32_t&        timeStampReply);

   boost::asio::basic_raw_socket<raw_tcp> RawTCPSocket;
};

#endif
