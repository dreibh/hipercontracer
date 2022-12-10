#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <iostream>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/address.hpp>

#include "ipv4header.h"
#include "icmpheader.h"
#include "udpheader.h"
#include "traceserviceheader.h"


// ###### Internet-16 checksum according to RFC 1071, computation part ######
template <typename Iterator> void processInternet16(uint32_t& sum, Iterator bodyBegin, Iterator bodyEnd)
{
   Iterator body_iter = bodyBegin;
   while (body_iter != bodyEnd) {
      sum += (static_cast<uint8_t>(*body_iter++) << 8);
      if (body_iter != bodyEnd) {
         sum += static_cast<uint8_t>(*body_iter++);
      }
   }
}


// ###### Internet-16 checksum according to RFC 1071, final part ############
inline uint16_t finishInternet16(uint32_t sum)
{
   sum = (sum >> 16) + (sum & 0xFFFF);
   sum += (sum >> 16);
   return static_cast<uint16_t>(~sum);
}


class raw_udp
{
   public:
   typedef boost::asio::ip::basic_endpoint<raw_udp> endpoint;
   typedef boost::asio::basic_raw_socket<raw_udp>   socket;
   typedef boost::asio::ip::basic_resolver<raw_udp> resolver;

   explicit raw_udp() : Protocol(IPPROTO_UDP), Family(AF_INET) { }
   explicit raw_udp(int protocol, int family) : Protocol(protocol), Family(family) { }

   static raw_udp v4() { return raw_udp(IPPROTO_UDP, AF_INET);  }
   static raw_udp v6() { return raw_udp(IPPROTO_UDP, AF_INET6); }

   int type()     const { return SOCK_RAW; }
   int protocol() const { return Protocol; }
   int family()   const { return Family;   }

   friend bool operator==(const raw_udp& p1, const raw_udp& p2) {
      return p1.Protocol == p2.Protocol && p1.Family == p2.Family;
   }
   friend bool operator!=(const raw_udp& p1, const raw_udp& p2) {
      return p1.Protocol != p2.Protocol || p1.Family != p2.Family;
   }

   private:
   int Protocol;
   int Family;
};


static std::map<boost::asio::ip::address, boost::asio::ip::address> SourceForDestinationMap;
static std::mutex                                                   SourceForDestinationMapMutex;

// ###### Find source address for given destination address #################
boost::asio::ip::address findSourceForDestination(const boost::asio::ip::address& destinationAddress)
{
   std::lock_guard<std::mutex> lock(SourceForDestinationMapMutex);

   // ====== Cache lookup ===================================================
   std::map<boost::asio::ip::address, boost::asio::ip::address>::const_iterator found =
      SourceForDestinationMap.find(destinationAddress);
   if(found != SourceForDestinationMap.end()) {
      return found->second;
   }

   // ====== Get source address from kernel =================================
   // Procedure:
   // - Create UDP socket
   // - Connect it to remote address
   // - Obtain local address
   // - Write this information into a cache for later lookup
   try {
      boost::asio::io_service        ioService;
      boost::asio::ip::udp::endpoint destinationEndpoint(destinationAddress, 7);
      boost::asio::ip::udp::socket   udpSpcket(ioService, (destinationAddress.is_v6() == true) ?
                                                             boost::asio::ip::udp::v6() :
                                                             boost::asio::ip::udp::v4());
      udpSpcket.connect(destinationEndpoint);
      const boost::asio::ip::address sourceAddress = udpSpcket.local_endpoint().address();
      SourceForDestinationMap.insert(std::pair<boost::asio::ip::address, boost::asio::ip::address>(destinationAddress,
                                                                                                   sourceAddress));
      return sourceAddress;
   }
   catch(...) {
      return boost::asio::ip::address();
   }
}


int main(int argc, char *argv[])
{
   if(argc < 2) {
      puts("Usage: ... [IP]");
      exit(1);
   }

   const boost::asio::ip::address remoteAddress = boost::asio::ip::address::from_string(argv[1]);
   const uint16_t                 remotePort    = 7;
   const uint16_t                 localPort     = 12345;
   const unsigned int             payloadSize   = 16;
   const unsigned int             round         = 1;
   const unsigned int             magicNumber   = 0x12345678;
   const unsigned int             ttl           = 8;

   boost::asio::io_service io_service;
   boost::asio::basic_raw_socket<raw_udp> rs(io_service);
   raw_udp::endpoint rep(remoteAddress.to_v4(), remotePort);
   rs.open();

   int sd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
   if(sd >= 0) {
      const int on = 1;
      if(setsockopt(sd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
         perror("setsockopt() error");
         exit(1);
      }

      if(setsockopt(rs.native_handle(), IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
         perror("setsockopt() error");
         exit(1);
      }

      uint16_t seqNum = 0;
      while(true) {

         for(int t = ttl; t > 0; t--) {
            seqNum++;

            IPv4Header ipv4Header;
            ipv4Header.version(4);
            ipv4Header.typeOfService(0x00);
            ipv4Header.headerLength(20);
            ipv4Header.totalLength(20 + 8 + payloadSize);
            ipv4Header.identification(seqNum);
            ipv4Header.fragmentOffset(0);
            ipv4Header.protocol(IPPROTO_UDP);
            ipv4Header.timeToLive(t);
            const boost::asio::ip::address localAddress = findSourceForDestination(remoteAddress);
            ipv4Header.sourceAddress(localAddress.to_v4());
            ipv4Header.destinationAddress(remoteAddress.to_v4());

            UDPHeader udpHeader;
            udpHeader.sourcePort(localPort);
            udpHeader.destinationPort(remotePort);
            udpHeader.length(8 + payloadSize);

            TraceServiceHeader tsHeader(payloadSize);
            tsHeader.magicNumber(magicNumber);
            tsHeader.sendTTL(ipv4Header.timeToLive());
            tsHeader.round((unsigned char)round);
            tsHeader.checksumTweak(seqNum);
            tsHeader.sendTimeStamp(std::chrono::system_clock::now());


            // Header checksum:
            uint32_t sum = 0;
            std::vector<uint8_t> ipv4HeaderContents = ipv4Header.contents();
            processInternet16(sum, ipv4HeaderContents.begin(), ipv4HeaderContents.end());
            ipv4Header.headerChecksum(finishInternet16(sum));
            printf("IPv4CS = %04x\n", ipv4Header.headerChecksum());

            sum = 0;
            ipv4HeaderContents = ipv4Header.contents();
            processInternet16(sum, ipv4HeaderContents.begin(), ipv4HeaderContents.end());
            printf("CHECK1 = %04x\n", finishInternet16(sum));


            // UDP checksum:
            sum = 0;
            std::vector<uint8_t> udpHeaderContents = udpHeader.contents();
            processInternet16(sum, udpHeaderContents.begin(), udpHeaderContents.end());
            std::vector<uint8_t> tsHeaderContents = tsHeader.contents();
            processInternet16(sum, tsHeaderContents.begin(), tsHeaderContents.end());

            IPv4PseudoHeader ph(ipv4Header, udpHeader.length());
            std::vector<uint8_t> pseudoHeaderContents = ph.contents();
            processInternet16(sum, pseudoHeaderContents.begin(), pseudoHeaderContents.end());

            udpHeader.checksum(finishInternet16(sum));


            sum = 0;
            udpHeaderContents = udpHeader.contents();
            tsHeaderContents = tsHeader.contents();
            pseudoHeaderContents = ph.contents();
            processInternet16(sum, udpHeaderContents.begin(), udpHeaderContents.end());
            processInternet16(sum, tsHeaderContents.begin(), tsHeaderContents.end());
            processInternet16(sum, pseudoHeaderContents.begin(), pseudoHeaderContents.end());
            printf("CHECK2 = %04x\n", finishInternet16(sum));


            // ====== Encode the request packet ================================
            boost::asio::streambuf request_buffer;
            std::ostream os(&request_buffer);
            os << ipv4Header << udpHeader << tsHeader;

            // ====== Send the request =========================================
//             ssize_t sent = sendto(sd, boost::asio::buffer_cast<const char*>(request_buffer.data()), request_buffer.size(), 0,
//                                   (sockaddr*)&remoteEndpoint, sizeof(remoteEndpoint));
/*
            if(sent < 0) {
               perror("sendto:");
            }*/

            rs.send_to(request_buffer.data(), rep);
         }

         usleep(1000000);
       }
   }
}
