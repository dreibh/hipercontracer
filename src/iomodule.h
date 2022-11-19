#ifndef IOMODULE_H
#define IOMODULE_H

#include "destinationinfo.h"
#include "resultentry.h"

#include <boost/asio.hpp>


class IOModuleBase
{
   public:
   IOModuleBase(const std::string&              name,
                boost::asio::io_service&        ioService,
                const boost::asio::ip::address& sourceAddress,
                const unsigned int              packetSize);
   virtual ~IOModuleBase();

   virtual ResultEntry* sendRequest(const uint16_t         identifier,
                                    const uint32_t         magicNumber,
                                    const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum) = 0;

   inline const std::string& getName() const { return Name; }

   protected:
   const std::string               Name;
   boost::asio::io_service&        IOService;
   const boost::asio::ip::address& SourceAddress;
   const unsigned int              PacketSize;
};


class ICMPModule : public IOModuleBase
{
   public:
   ICMPModule(const std::string&              name,
              boost::asio::io_service&        ioService,
              const boost::asio::ip::address& sourceAddress,
              const unsigned int              packetSize);
   virtual ~ICMPModule();

   virtual bool prepareSocket();
   virtual void expectNextReply();
   virtual void cancelSocket();

   virtual ResultEntry* sendRequest(const uint16_t         identifier,
                                    const uint32_t         magicNumber,
                                    const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum);
   virtual void handleResponse(const boost::system::error_code& errorCode,
                               std::size_t                      length);

   protected:
   const unsigned int              PayloadSize;
   const unsigned int              ActualPacketSize;

   boost::asio::ip::icmp::socket   ICMPSocket;
   boost::asio::ip::icmp::endpoint ReplyEndpoint;    // Store ICMP reply's source address
   bool                            ExpectingReply;
   char                            MessageBuffer[65536 + 40];
};


#endif
