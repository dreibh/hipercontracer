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

#ifndef IOMODULE_BASE_H
#define IOMODULE_BASE_H

#include "destinationinfo.h"
#include "resultentry.h"

#include <list>
#include <map>
#include <boost/asio.hpp>


class ICMPHeader;
struct scm_timestamping;
struct sock_extended_err;

class IOModuleBase
{
   public:
   IOModuleBase(boost::asio::io_context&                 ioContext,
                std::map<unsigned short, ResultEntry*>&  resultsMap,
                const boost::asio::ip::address&          sourceAddress,
                const uint16_t                           sourcePort,
                const uint16_t                           destinationPort,
                std::function<void (const ResultEntry*)> newResultCallback);
   virtual ~IOModuleBase();

   virtual unsigned int sendRequest(const DestinationInfo& destination,
                                    const unsigned int     fromTTL,
                                    const unsigned int     toTTL,
                                    const unsigned int     fromRound,
                                    const unsigned int     toRound,
                                    uint16_t&              seqNumber,
                                    uint32_t*              targetChecksumArray) = 0;

   inline const std::string& getName() const { return Name; }
   inline void setName(const std::string& name) {
      Name = name + "/" + getProtocolName();
   }
   virtual const ProtocolType getProtocolType() const = 0;
   virtual const std::string& getProtocolName() const = 0;
   inline uint16_t getIdentifier()              const { return Identifier; }

   virtual bool prepareSocket() = 0;
   virtual void cancelSocket() = 0;

   static bool configureSocket(const int                      socketDescriptor,
                               const boost::asio::ip::address sourceAddress);

   static const boost::asio::ip::address& unspecifiedAddress(const bool ipv6);
   static boost::asio::ip::address findSourceForDestination(const boost::asio::ip::address& destinationAddress);


   struct ReceivedData {
      boost::asio::ip::udp::endpoint Source;
      boost::asio::ip::udp::endpoint Destination;
      boost::asio::ip::udp::endpoint ReplyEndpoint;
      ResultTimePoint                ApplicationReceiveTime;
      TimeSourceType                 ReceiveSWSource;
      ResultTimePoint                ReceiveSWTime;
      TimeSourceType                 ReceiveHWSource;
      ResultTimePoint                ReceiveHWTime;
      char*                          MessageBuffer;
      size_t                         MessageLength;
   };

   void recordResult(const ReceivedData&  receivedData,
                     const uint8_t        icmpType,
                     const uint8_t        icmpCode,
                     const unsigned short seqNumber,
                     const unsigned int   responseLength);

   static bool registerIOModule(const ProtocolType  moduleType,
                                const std::string&  moduleName,
                                IOModuleBase* (*createIOModuleFunction)(
                                   boost::asio::io_context&                 ioContext,
                                   std::map<unsigned short, ResultEntry*>&  resultsMap,
                                   const boost::asio::ip::address&          sourceAddress,
                                   const uint16_t                           sourcePort,
                                   const uint16_t                           destinationPort,
                                   std::function<void (const ResultEntry*)> newResultCallback,
                                   const unsigned int                       packetSize));
   static IOModuleBase* createIOModule(const std::string&                       moduleName,
                                       boost::asio::io_context&                 ioContext,
                                       std::map<unsigned short, ResultEntry*>&  resultsMap,
                                       const boost::asio::ip::address&          sourceAddress,
                                       const uint16_t                           sourcePort,
                                       const uint16_t                           destinationPort,
                                       std::function<void (const ResultEntry*)> newResultCallback,
                                       const unsigned int                       packetSize);
   static bool checkIOModule(const std::string& moduleName);

   protected:
   static boost::asio::ip::address          UnspecIPv4;
   static boost::asio::ip::address          UnspecIPv6;

   std::string                              Name;
   boost::asio::io_context&                 IOContext;
   std::map<unsigned short, ResultEntry*>&  ResultsMap;
   const boost::asio::ip::address&          SourceAddress;
   const uint16_t                           SourcePort;
   const uint16_t                           DestinationPort;
   unsigned int                             PayloadSize;
   unsigned int                             ActualPacketSize;
   std::function<void (const ResultEntry*)> NewResultCallback;
   const uint32_t                           MagicNumber;
   uint16_t                                 Identifier;
   uint32_t                                 TimeStampSeqID;

   private:
   struct RegisteredIOModule {
      std::string  Name;
      ProtocolType Type;
      IOModuleBase* (*CreateIOModuleFunction)(
         boost::asio::io_context&                 ioContext,
         std::map<unsigned short, ResultEntry*>&  resultsMap,
         const boost::asio::ip::address&          sourceAddress,
         const uint16_t                           sourcePort,
         const uint16_t                           destinationPort,
         std::function<void (const ResultEntry*)> newResultCallback,
         const unsigned int                       packetSize);
   };

   static std::list<RegisteredIOModule*>* IOModuleList;
};

#define REGISTER_IOMODULE(moduleType, moduleName, iomodule) \
   static IOModuleBase* createIOModule_##iomodule(boost::asio::io_context&                 ioContext, \
                                                  std::map<unsigned short, ResultEntry*>&  resultsMap, \
                                                  const boost::asio::ip::address&          sourceAddress, \
                                                  const uint16_t                           sourcePort, \
                                                  const uint16_t                           destinationPort, \
                                                  std::function<void (const ResultEntry*)> newResultCallback, \
                                                  const unsigned int                       packetSize) { \
      return new iomodule(ioContext, resultsMap, sourceAddress, sourcePort, destinationPort, newResultCallback, packetSize); \
   } \
   static bool Registered_##iomodule = IOModuleBase::registerIOModule(moduleType, moduleName, createIOModule_##iomodule);

#endif
