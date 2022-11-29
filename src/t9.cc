#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <iostream>
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

   const char*        localAddress  = "10.44.33.110";
   const uint16_t     localPort     = 12345;
   const char*        remoteAddress = argv[1];
   const uint16_t     remotePort    = 7;
   const unsigned int payloadSize   = 16;
   const unsigned int round         = 1;
   const unsigned int magicNumber   = 0x12345678;
   const unsigned int ttl           = 8;

   sockaddr_in localEndpoint;
   localEndpoint.sin_family = AF_INET;
   localEndpoint.sin_addr.s_addr = inet_addr(localAddress);
   localEndpoint.sin_port = htons(localPort);

   sockaddr_in remoteEndpoint;
   remoteEndpoint.sin_family = AF_INET;
   remoteEndpoint.sin_addr.s_addr = inet_addr(remoteAddress);
   remoteEndpoint.sin_port = htons(remotePort);


   // ====== Obtain local address for given destination:
   sockaddr_in localTEST;
   socklen_t localTESTSize = sizeof(localTEST);
   int sdTEST = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if(connect(sdTEST, (sockaddr*)&remoteEndpoint, sizeof(sockaddr_in)) != 0) {
      perror("connect(sdTEST):");
      exit(1);
   }
   if(getsockname(sdTEST, (sockaddr*)&localTEST, &localTESTSize) != 0) {
      perror("getsockname():");
      exit(1);
   }
   boost::asio::ip::address_v4 localTESTaddress(ntohl(((sockaddr_in*)&localTEST)->sin_addr.s_addr));
   std::cout << "LOCAL=" << localTESTaddress << "\n";
   close(sdTEST);


   int sdINPUT = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if(bind(sdINPUT, (sockaddr*)&localEndpoint, sizeof(sockaddr_in)) != 0) {
      perror("bind(sdINPUT):");
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
            ipv4Header.sourceAddress(boost::asio::ip::address::from_string(localAddress).to_v4());
            ipv4Header.destinationAddress(boost::asio::ip::address::from_string(remoteAddress).to_v4());

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
            ssize_t sent = sendto(sd, boost::asio::buffer_cast<const char*>(request_buffer.data()), request_buffer.size(), 0,
                                  (sockaddr*)&remoteEndpoint, sizeof(remoteEndpoint));
            if(sent < 0) {
               perror("sendto:");
            }
         }

         usleep(1000000);
       }
   }
}
