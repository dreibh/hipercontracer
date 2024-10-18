#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <boost/interprocess/streams/bufferstream.hpp>
#include <vector>

#include "ipv4header.h"
#include "icmpheader.h"
#include "udpheader.h"
#include "traceserviceheader.h"


int main(int argc, char *argv[])
{
   sockaddr_in localAddress;
   localAddress.sin_family = AF_INET;
   localAddress.sin_addr.s_addr = inet_addr("10.44.33.110");
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


               // Level 1: Outer IPv4 header
               IPv4Header outerIPv4Header;
               is >> outerIPv4Header;
               if(!(is && (outerIPv4Header.protocol() == IPPROTO_ICMP))) {
                puts("x-1");
                  continue;
               }

               // Level 2: Outer ICMP header
               ICMPHeader outerICMPHeader;
               is >> outerICMPHeader;
               if(!is) {
                puts("x-2");
                  continue;
               }

               // Level 3: Inner IPv4 header
               IPv4Header innerIPv4Header;
               is >> innerIPv4Header;
               if(!(is && (innerIPv4Header.protocol() == IPPROTO_UDP))) {
                puts("x-3");
                  continue;
               }
               printf("IPv4 Identification = %04x\n", innerIPv4Header.identification());

               // Level 4: UDP header
               UDPHeader udpHeader;
               is >> udpHeader;
               if(!is) {
                puts("x-4");
                  continue;
               }
               printf("UDP::");

               // Level 5: TraceService header
               TraceServiceHeader tsHeader;
               is >> tsHeader;
               if(is) {
                  printf("::HPCT seq=%u", tsHeader.checksumTweak());
               }

            }
         }
      }
   }
}
