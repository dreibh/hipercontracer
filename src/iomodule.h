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
   enum SocketIdentifier {
      SI_ICMP = 0,
      SI_UDP  = 1,
      SI_AUX  = 10  // ...
   };
   struct ReceivedData {
      boost::asio::ip::udp::endpoint                 ReplyEndpoint;
      std::chrono::high_resolution_clock::time_point ApplicationReceiveTime;
      TimeSourceType                                 ReceiveSWSource;
      std::chrono::high_resolution_clock::time_point ReceiveSWTime;
      TimeSourceType                                 ReceiveHWSource;
      std::chrono::high_resolution_clock::time_point ReceiveHWTime;
      char*                                          MessageBuffer;
      size_t                                         MessageLength;
      sock_extended_err*                             SocketError;
   };

   void expectNextReply();
   void handleResponse(const boost::system::error_code& errorCode,
                       const bool                       readFromErrorQueue,
                       const unsigned int               socketIdentifier);
   void handlePayloadResponse(const unsigned int  socketIdentifier,
                              const ReceivedData& receivedData);
   void handleErrorResponse(const unsigned int       socketIdentifier,
                            const ReceivedData&      receivedData,
                            const sock_extended_err* socketError);
   void updateSendTimeInResultEntry(const sock_extended_err* socketError,
                                    const scm_timestamping*  socketTimestamping);
   void recordResult(const ReceivedData&  receivedData,
                     const uint8_t        icmpType,
                     const uint8_t        icmpCode,
                     const unsigned short seqNumber);

   const unsigned int             PayloadSize;
   const unsigned int             ActualPacketSize;

   // For ICMP type, this UDP socket is only used to generate a
   // system-unique 16-bit ICMP Identifier!
   boost::asio::ip::udp::socket   UDPSocket;
   boost::asio::ip::udp::endpoint UDPSocketEndpoint;
   boost::asio::ip::icmp::socket  ICMPSocket;
   bool                           ExpectingReply;
   bool                           ExpectingError;
   char                           MessageBuffer[65536 + 40];
   char                           ControlBuffer[1024];
};


class UDPModule : public ICMPModule
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
//    void expectNextReply();
//    void handleResponse(const boost::system::error_code& errorCode,
//                        const bool                       readFromErrorQueue);
//    void recordResult(const std::chrono::high_resolution_clock::time_point& receiveTime,
//                      const unsigned int                           icmpType,
//                      const unsigned int                           icmpCode,
//                      const unsigned short                         seqNumber);

//    const unsigned int             PayloadSize;
//    const unsigned int             ActualPacketSize;
//
//    boost::asio::ip::udp::socket   UDPSocket;
//    boost::asio::ip::udp::endpoint ReplyEndpoint;    // Store UDP reply's source address    FIXME! Is this needed as attrib?
//    bool                           ExpectingReply;
//    bool                           ExpectingError;
//    char                           MessageBuffer[65536 + 40];
//    char                           ControlBuffer[1024];
};


#endif
