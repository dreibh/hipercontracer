#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <boost/interprocess/streams/bufferstream.hpp>
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
uint16_t finishInternet16(uint32_t sum)
{
   sum = (sum >> 16) + (sum & 0xFFFF);
   sum += (sum >> 16);
   return static_cast<uint16_t>(~sum);
}


int main(int argc, char *argv[])
{
   if(argc < 2) {
      puts("Usage: ... [IP]");
      exit(1);
   }

   const uint16_t     localPort   = 12345;
   const uint16_t     remotePort  = 7;
   const unsigned int payloadSize = 16;
   const unsigned int round       = 1;
   const unsigned int magicNumber = 0x12345678;
   const unsigned int ttl         = 64;

   sockaddr_in localAddress;
   localAddress.sin_family = AF_INET;
   localAddress.sin_addr.s_addr = inet_addr("0.0.0.0");
   localAddress.sin_port = htons(localPort);

   sockaddr_in remoteAddress;
   remoteAddress.sin_family = AF_INET;
   remoteAddress.sin_addr.s_addr = inet_addr(argv[1]);
   remoteAddress.sin_port = htons(remotePort);

   int sdINPUT = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if(bind(sdINPUT, (sockaddr*)&localAddress, sizeof(sockaddr_in)) != 0) {
      perror("bind():");
      exit(1);
   }

   int sd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
   if(sd >= 0) {
      const int on = 1;
      if(setsockopt(sd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
         perror("setsockopt() error");
         exit(1);
      }

      uint16_t seqNum = 0;
      while(true) {
         seqNum++;

         IPv4Header ipv4Header;
         ipv4Header.version(4);
         ipv4Header.typeOfService(0x00);
         ipv4Header.headerLength(20);
         ipv4Header.totalLength(20 + 8 + payloadSize);
         ipv4Header.identification(seqNum);
         ipv4Header.fragmentOffset(0);
         ipv4Header.protocol(IPPROTO_UDP);
         ipv4Header.timeToLive(ttl);
         ipv4Header.sourceAddress(boost::asio::ip::address::from_string("192.168.0.16").to_v4());
         ipv4Header.destinationAddress(boost::asio::ip::address::from_string(argv[1]).to_v4());

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

         uint32_t sum = 0;
         std::vector<uint8_t> ipv4HeaderContents = ipv4Header.contents();
         processInternet16(sum, ipv4HeaderContents.begin(), ipv4HeaderContents.end());
         ipv4Header.headerChecksum(finishInternet16(sum));
         printf("IPv4CS = %04x\n", ipv4Header.headerChecksum());

         sum = 0;
         ipv4HeaderContents = ipv4Header.contents();
         processInternet16(sum, ipv4HeaderContents.begin(), ipv4HeaderContents.end());
         printf("CHECK1 = %04x\n", finishInternet16(sum));


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
         ssize_t sent = sendto(sd, boost::asio::buffer_cast<const char*>(request_buffer.data()), request_buffer.size(), 0,
                               (sockaddr*)&remoteAddress, sizeof(remoteAddress));
         if(sent < 0) {
            perror("sendto:");
         }

         usleep(1000000);
       }
   }
}
