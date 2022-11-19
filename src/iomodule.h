#ifndef IOMODULE_H
#define IOMODULE_H

#include "destinationinfo.h"
#include "resultentry.h"

#include <boost/asio.hpp>

class ICMPHeader;

class IOModuleBase
{
   public:
   IOModuleBase(const std::string&                       name,
                boost::asio::io_service&                 ioService,
                std::map<unsigned short, ResultEntry*>&  resultsMap,
                const boost::asio::ip::address&          sourceAddress,
                const unsigned int                       packetSize,
                std::function<void (const ResultEntry*)> newResultFunction);
   virtual ~IOModuleBase();

   virtual ResultEntry* sendRequest(const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum) = 0;

   inline const std::string& getName() const { return Name; }

   virtual bool prepareSocket() = 0;
   virtual void expectNextReply() = 0;
   virtual void cancelSocket() = 0;

   protected:
   const std::string                        Name;
   boost::asio::io_service&                 IOService;
   std::map<unsigned short, ResultEntry*>&  ResultsMap;
   const boost::asio::ip::address&          SourceAddress;
   const unsigned int                       PacketSize;
   std::function<void (const ResultEntry*)> NewResultFunction;
   const uint32_t                           MagicNumber;
   uint16_t                                 Identifier;
};


class ICMPModule : public IOModuleBase
{
   public:
   ICMPModule(const std::string&                       name,
              boost::asio::io_service&                 ioService,
              std::map<unsigned short, ResultEntry*>&  resultsMap,
              const boost::asio::ip::address&          sourceAddress,
              const unsigned int                       packetSize,
              std::function<void (const ResultEntry*)> newResultFunction);
   virtual ~ICMPModule();

   virtual bool prepareSocket();
   virtual void expectNextReply();
   virtual void cancelSocket();

   virtual ResultEntry* sendRequest(const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum);
   virtual void handleResponse(const boost::system::error_code& errorCode,
                               std::size_t                      length);
   void recordResult(const std::chrono::system_clock::time_point& receiveTime,
                     const ICMPHeader&                            icmpHeader,
                     const unsigned short                         seqNumber);

   protected:
   const unsigned int              PayloadSize;
   const unsigned int              ActualPacketSize;

   boost::asio::ip::icmp::socket   ICMPSocket;
   boost::asio::ip::icmp::endpoint ReplyEndpoint;    // Store ICMP reply's source address
   bool                            ExpectingReply;
   char                            MessageBuffer[65536 + 40];
};


#endif
