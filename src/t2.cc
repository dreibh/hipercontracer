#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
// #include <linux/if_ether.h>
#include <netdb.h>


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   int sd;
   // sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
   sd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
   if(sd < 0) {
      perror("socket()");
      exit(1);
   }

   char buffer[65536];
   memset((char*)&buffer, 0 , sizeof(buffer));
   struct sockaddr_storage sa;
   memset((char*)&sa, 0 , sizeof(sa));
   socklen_t salen = sizeof(sa);

   ssize_t r = recvfrom(sd, (char*)&buffer, sizeof(buffer), 0, (sockaddr*)&sa, &salen);
   while(r > 0) {
      char address[256];
      if(getnameinfo((const sockaddr*)&sa, salen, (char*)&address, sizeof(address), NULL, 0, NI_NUMERICHOST) != 0) {
          snprintf((char*)&address, sizeof(address), "(invalid)");
      }
      printf("r=%d from %s\n", (int)r, address);
      
      r = recvfrom(sd, (char*)&buffer, sizeof(buffer), 0, (sockaddr*)&sa, &salen);
   }
   
   perror("RESULT");
   puts("Done!");
   return(0);
}
