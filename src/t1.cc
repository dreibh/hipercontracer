#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "icmpheader.h"
#include "traceserviceheader.h"

#include <boost/date_time/posix_time/posix_time.hpp>


// ###### Convert ptime to microseconds #####################################
unsigned long long ptimeToMircoTime(const boost::posix_time::ptime t)
{
   static const boost::posix_time::ptime myEpoch(boost::gregorian::date(1976,9,29));
   boost::posix_time::time_duration difference = t - myEpoch;
   return(difference.ticks());
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   uint16_t Identifier  = 1234;
   uint16_t SeqNumber   = 61000;
   uint32_t MagicNumber = 0xcafe2909;
   unsigned int round   = 1;
//    unsigned int ttl     = 34;

   uint32_t targetChecksum = ~0;

   for(unsigned int i = 0; i < 1000000; i++) {
      for(unsigned int ttl = 1; ttl < 34; ttl++) {
         printf("------ i=%d\tttl=%d ------\n", i, ttl);

         ICMPHeader echoRequest;
         echoRequest.type(ICMPHeader::IPv4EchoRequest);
         echoRequest.code(0);
         echoRequest.identifier(Identifier);
         echoRequest.seqNumber(SeqNumber);
         TraceServiceHeader tsHeader;
         tsHeader.magicNumber(MagicNumber);
         tsHeader.sendTTL(ttl);
         tsHeader.round((unsigned char)round);
         tsHeader.checksumTweak(0);
         // const boost::posix_time::ptime sendTime = boost::posix_time::microsec_clock::universal_time();
         tsHeader.sendTimeStamp(0x474b7f7648180ULL);   // ptimeToMircoTime(sendTime));
         std::vector<unsigned char> tsHeaderContents = tsHeader.contents();

         // ------ No given target checksum ---------------------
         if(targetChecksum == ~((uint32_t)0)) {
            computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
            targetChecksum = echoRequest.checksum();
         }
         // ------ Target checksum given ------------------------
         else {
            computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
            const uint16_t originalChecksum = echoRequest.checksum();

            uint16_t diff;
            diff = 0xffff - (targetChecksum - originalChecksum);

   //          if((uint32_t)originalChecksum + (uint32_t)diff > 0xffff) {
   //             puts("x-1");
   //             diff--;
   //          }


   //          diff = (originalChecksum - targetChecksum);
   //          diff = ((originalChecksum - targetChecksum) & 0xffff);

            tsHeader.checksumTweak( diff );
            tsHeaderContents = tsHeader.contents();
            computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
            const uint16_t newChecksum = echoRequest.checksum();

               printf("ORIGIN=%x\n", originalChecksum);
               printf("TARGET=%x\n", targetChecksum);
               printf("NEW=   %x\n", newChecksum);
               printf("diff=%x\n", diff);

            if(newChecksum != targetChecksum) {
               std::cerr << "WARNING: Traceroute::sendICMPRequest() - Checksum differs from target checksum!" << std::endl;
               printf("D=   %x\n", (newChecksum - targetChecksum));
               ::abort();
            }
            computeInternet16_B(echoRequest, newChecksum);
         }
      }

      SeqNumber++;
   }

   return(0);
}
