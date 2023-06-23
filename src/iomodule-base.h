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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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
   IOModuleBase(boost::asio::io_service&                 ioService,
                std::map<unsigned short, ResultEntry*>&  resultsMap,
                const boost::asio::ip::address&          sourceAddress,
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

   static boost::asio::ip::address findSourceForDestination(const boost::asio::ip::address& destinationAddress);


   struct ReceivedData {
      boost::asio::ip::udp::endpoint                 Source;
      boost::asio::ip::udp::endpoint                 Destination;
      boost::asio::ip::udp::endpoint                 ReplyEndpoint;
      std::chrono::high_resolution_clock::time_point ApplicationReceiveTime;
      TimeSourceType                                 ReceiveSWSource;
      std::chrono::high_resolution_clock::time_point ReceiveSWTime;
      TimeSourceType                                 ReceiveHWSource;
      std::chrono::high_resolution_clock::time_point ReceiveHWTime;
      char*                                          MessageBuffer;
      size_t                                         MessageLength;
   };

   void recordResult(const ReceivedData&  receivedData,
                     const uint8_t        icmpType,
                     const uint8_t        icmpCode,
                     const unsigned short seqNumber,
                     const unsigned int   responseLength);

   static bool registerIOModule(const ProtocolType  moduleType,
                                const std::string&  moduleName,
                                IOModuleBase* (*createIOModuleFunction)(
                                   boost::asio::io_service&                 ioService,
                                   std::map<unsigned short, ResultEntry*>&  resultsMap,
                                   const boost::asio::ip::address&          sourceAddress,
                                   std::function<void (const ResultEntry*)> newResultCallback,
                                   const unsigned int                       packetSize,
                                   const uint16_t                           destinationPort));
   static IOModuleBase* createIOModule(const std::string&                       moduleName,
                                       boost::asio::io_service&                 ioService,
                                       std::map<unsigned short, ResultEntry*>&  resultsMap,
                                       const boost::asio::ip::address&          sourceAddress,
                                       std::function<void (const ResultEntry*)> newResultCallback,
                                       const unsigned int                       packetSize,
                                       const uint16_t                           destinationPort);
   static bool checkIOModule(const std::string& moduleName);

   protected:
   static std::map<boost::asio::ip::address, boost::asio::ip::address> SourceForDestinationMap;
   static std::mutex                                                   SourceForDestinationMapMutex;

   std::string                              Name;
   boost::asio::io_service&                 IOService;
   std::map<unsigned short, ResultEntry*>&  ResultsMap;
   const boost::asio::ip::address&          SourceAddress;
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
         boost::asio::io_service&                 ioService,
         std::map<unsigned short, ResultEntry*>&  resultsMap,
         const boost::asio::ip::address&          sourceAddress,
         std::function<void (const ResultEntry*)> newResultCallback,
         const unsigned int                       packetSize,
         const uint16_t                           destinationPort);
   };

   static std::list<RegisteredIOModule*>* IOModuleList;
};

#define REGISTER_IOMODULE(moduleType, moduleName, iomodule) \
   static IOModuleBase* createIOModule_##iomodule(boost::asio::io_service&                 ioService, \
                                                  std::map<unsigned short, ResultEntry*>&  resultsMap, \
                                                  const boost::asio::ip::address&          sourceAddress, \
                                                  std::function<void (const ResultEntry*)> newResultCallback, \
                                                  const unsigned int                       packetSize, \
                                                  const uint16_t                           destinationPort) { \
      return new iomodule(ioService, resultsMap, sourceAddress, newResultCallback, packetSize, destinationPort); \
   } \
   static bool Registered_##iomodule = IOModuleBase::registerIOModule(moduleType, moduleName, createIOModule_##iomodule);

#endif
