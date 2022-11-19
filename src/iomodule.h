#ifndef IOMODULE_H
#define IOMODULE_H

#include "destinationinfo.h"

#include <boost/asio.hpp>


class IOModuleBase
{
   public:
   IOModuleBase(const std::string&              name,
                boost::asio::io_service&        ioService,
                const boost::asio::ip::address& sourceAddress,
                const unsigned int              packetSize);
   virtual ~IOModuleBase();

   virtual void sendRequest(const DestinationInfo& destination,
                            const unsigned int             ttl,
                            const unsigned int             round,
                            uint32_t&                      targetChecksum) = 0;

   inline const std::string& getName() const { return Name; }

   protected:
   const std::string               Name;
   boost::asio::io_service&        IOService;
   const boost::asio::ip::address& SourceAddress;
   const unsigned int              PacketSize;
   const uint32_t                  MagicNumber;
};


class ICMPModule : public IOModuleBase
{
   public:
   ICMPModule(const std::string&              name,
              boost::asio::io_service&        ioService,
              const boost::asio::ip::address& sourceAddress,
              const unsigned int              packetSize);
   virtual ~ICMPModule();

   virtual void sendRequest(const DestinationInfo& destination,
                            const unsigned int             ttl,
                            const unsigned int             round,
                            uint32_t&                      targetChecksum);

   protected:
   const unsigned int            PayloadSize;
   const unsigned int            ActualPacketSize;

   boost::asio::ip::icmp::socket ICMPSocket;
   uint16_t                      Identifier;
   uint16_t                      SeqNumber;
};


#endif
