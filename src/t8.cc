#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <boost/interprocess/streams/bufferstream.hpp>

#include "ipv4header.h"
#include "icmpheader.h"
#include "traceserviceheader.h"


int main(int argc, char *argv[])
{
   sockaddr_in localAddress;
   localAddress.sin_family = AF_INET;
   localAddress.sin_addr.s_addr = inet_addr("192.168.0.16");
   localAddress.sin_port = htons(0);

   int sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
   if(sd >= 0) {
      if(bind(sd, (sockaddr*)&localAddress, sizeof(sockaddr)) == 0) {
         char             buffer[65536];
         sockaddr_storage address;
         socklen_t        addressLength = sizeof(address);
         while(true) {
            ssize_t r = recvfrom(sd, (char*)&buffer, sizeof(buffer), 0,
                                 (sockaddr*)&address, &addressLength);
            printf("r=%d\n", (int)r);
            if(r > 0) {
               boost::interprocess::bufferstream is(buffer, r);

               IPv4Header ipv4Header;
               is >> ipv4Header;
               if(is) {
                  printf("IPv4::");
                  ICMPHeader icmpHeader;
                  is >> icmpHeader;
                  if(is) {
                     printf("ICMP::");
                     if(icmpHeader.type() == ICMPHeader::IPv4TimeExceeded) {
                        printf("TimeExceeded");
                     }
                     else {
                        printf("Type=%d", icmpHeader.type());
                     }

                     TraceServiceHeader tsHeader;
                     is >> tsHeader;
                     if(is) {
                        printf("::HPCT seq=%u", tsHeader.checksumTweak());
                     }
                  }
                  puts("");
               }
            }
         }
      }
   }
}
