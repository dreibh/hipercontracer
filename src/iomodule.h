#ifndef IOMODULE_H
#define IOMODULE_H

#include "destinationinfo.h"
#include "resultentry.h"

#include <boost/asio.hpp>

class ICMPHeader;
struct scm_timestamping;
struct sock_extended_err;

class IOModuleBase
{
   public:
   IOModuleBase(const std::string&                       name,
                boost::asio::io_service&                 ioService,
                std::map<unsigned short, ResultEntry*>&  resultsMap,
                const boost::asio::ip::address&          sourceAddress,
                const unsigned int                       packetSize,
                std::function<void (const ResultEntry*)> newResultCallback);
   virtual ~IOModuleBase();

   virtual ResultEntry* sendRequest(const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum) = 0;

   inline const std::string& getName() const { return Name; }

   virtual bool prepareSocket() = 0;
   virtual void cancelSocket() = 0;

   protected:
   const std::string                        Name;
   boost::asio::io_service&                 IOService;
   std::map<unsigned short, ResultEntry*>&  ResultsMap;
   const boost::asio::ip::address&          SourceAddress;
   const unsigned int                       PacketSize;
   std::function<void (const ResultEntry*)> NewResultCallback;
   const uint32_t                           MagicNumber;
   uint16_t                                 Identifier;
   uint32_t                                 TimeStampSeqID;
};


class ICMPModule : public IOModuleBase
{
   public:
   ICMPModule(const std::string&                       name,
              boost::asio::io_service&                 ioService,
              std::map<unsigned short, ResultEntry*>&  resultsMap,
              const boost::asio::ip::address&          sourceAddress,
              const unsigned int                       packetSize,
              std::function<void (const ResultEntry*)> newResultCallback);
   virtual ~ICMPModule();

   virtual bool prepareSocket();
   virtual void cancelSocket();

   virtual ResultEntry* sendRequest(const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum);

   protected:
   void expectNextReply();
   void handleResponse(const boost::system::error_code& errorCode,
                       const bool                       readFromErrorQueue);
   void updateSendTimeInResultEntry(const sock_extended_err* socketError,
                                    const scm_timestamping*  socketTimestamping);
//    void getReceptionTimes(
//            const std::chrono::system_clock::time_point& applicationReceiveTime,
//            const int                                    socketDescriptor,
//            const scm_timestamping*                      socketTimestamp,
//            TimeSourceType&                              rxReceiveSWSource,
//            std::chrono::system_clock::time_point&       rxReceiveSWTime,
//            TimeSourceType&                              rxReceiveHWSource,
//            std::chrono::system_clock::time_point&       rxReceiveHWTime);

   void recordResult(const std::chrono::system_clock::time_point& applicationReceiveTime,
                     const TimeSourceType                         rxReceiveSWSource,
                     const std::chrono::system_clock::time_point& rxReceiveSWTime,
                     const TimeSourceType                         rxReceiveHWSource,
                     const std::chrono::system_clock::time_point& rxReceiveHWTime,
                     const ICMPHeader&                            icmpHeader,
                     const unsigned short                         seqNumber);

   const unsigned int              PayloadSize;
   const unsigned int              ActualPacketSize;

   boost::asio::ip::icmp::socket   ICMPSocket;
   boost::asio::ip::icmp::endpoint ReplyEndpoint;    // Store ICMP reply's source address    FIXME! Is this needed as attrib?
   bool                            ExpectingReply;
   bool                            ExpectingError;
   char                            MessageBuffer[65536 + 40];
   char                            ControlBuffer[1024];
};


class UDPModule : public IOModuleBase
{
   public:
   UDPModule(const std::string&                       name,
             boost::asio::io_service&                 ioService,
             std::map<unsigned short, ResultEntry*>&  resultsMap,
             const boost::asio::ip::address&          sourceAddress,
             const unsigned int                       packetSize,
             std::function<void (const ResultEntry*)> newResultCallback);
   virtual ~UDPModule();

   virtual bool prepareSocket();
   virtual void cancelSocket();

   virtual ResultEntry* sendRequest(const DestinationInfo& destination,
                                    const unsigned int     ttl,
                                    const unsigned int     round,
                                    uint16_t&              seqNumber,
                                    uint32_t&              targetChecksum);

   protected:
   void expectNextReply();
   void handleResponse(const boost::system::error_code& errorCode,
                       const bool                       readFromErrorQueue);
   void recordResult(const std::chrono::system_clock::time_point& receiveTime,
                     const unsigned int                           icmpType,
                     const unsigned int                           icmpCode,
                     const unsigned short                         seqNumber);

   const unsigned int             PayloadSize;
   const unsigned int             ActualPacketSize;

   boost::asio::ip::udp::socket   UDPSocket;
   boost::asio::ip::udp::endpoint ReplyEndpoint;    // Store UDP reply's source address    FIXME! Is this needed as attrib?
   bool                           ExpectingReply;
   bool                           ExpectingError;
   char                           MessageBuffer[65536 + 40];
   char                           ControlBuffer[1024];
};


#endif
