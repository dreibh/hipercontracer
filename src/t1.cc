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
   uint16_t SeqNumber   = 20000;
   uint32_t MagicNumber = 0xcafe2909;
   unsigned int round   = 1;
   unsigned int ttl     = 34;

   uint32_t targetChecksum = ~0;

   for(unsigned int i = 0; i < 1000; i++) {
      printf("i=%d\n", i);

      ICMPHeader echoRequest;
      echoRequest.type(ICMPHeader::IPv4EchoRequest);
      echoRequest.code(0);
      echoRequest.identifier(Identifier);
      echoRequest.seqNumber(SeqNumber);
      TraceServiceHeader tsHeader;
      tsHeader.magicNumber(MagicNumber);
      tsHeader.sendTTL(ttl);
      tsHeader.round((unsigned char)round);
      tsHeader.checksumTweak(0x8000);
      // const boost::posix_time::ptime sendTime = boost::posix_time::microsec_clock::universal_time();
      tsHeader.sendTimeStamp(0x474b7f7648180ULL);   // ptimeToMircoTime(sendTime));
      std::vector<unsigned char> tsHeaderContents = tsHeader.contents();

      // ------ No given target checksum ---------------------
      if(targetChecksum == ~((uint32_t)0)) {
         targetChecksum = computeInternet16_A(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
         computeInternet16_B(echoRequest, targetChecksum);
      }
      // ------ Target checksum given ------------------------
      else {
         const uint32_t originalChecksum = computeInternet16_A(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());

         uint32_t diff;
         diff = (originalChecksum - targetChecksum) & 0xffff;

         tsHeader.checksumTweak( diff );
         tsHeaderContents = tsHeader.contents();
         const uint32_t newChecksum = computeInternet16_A(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
         if(newChecksum != targetChecksum) {
            std::cerr << "WARNING: Traceroute::sendICMPRequest() - Checksum differs from target checksum!" << std::endl;
            printf("OLD=   %x\n", originalChecksum);
            printf("TARGET=%x\n", targetChecksum);
            printf("NEW=   %x\n", newChecksum);
            printf("diff=%x\n", diff);
            ::abort();
         }
         computeInternet16_B(echoRequest, newChecksum);
      }

      SeqNumber++;
   }

   return(0);
}
