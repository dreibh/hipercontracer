#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <iostream>

#include "icmpheader.h"
#include "traceserviceheader.h"


// #include <boost/date_time/posix_time/posix_time.hpp>
//
// // ###### Convert ptime to microseconds #####################################
// unsigned long long ptimeToMircoTime(const boost::posix_time::ptime t)
// {
//    static const boost::posix_time::ptime myEpoch(boost::gregorian::date(1976,9,29));
//    boost::posix_time::time_duration difference = t - myEpoch;
//    return(difference.ticks());
// }


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
//       printf("------ i=%d ------\n", i);
      for(unsigned int ttl = 1; ttl < 42; ttl++) {
//          printf("------ i=%d\tttl=%d ------\n", i, ttl);

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
         tsHeader.sendTimeStamp(0x474b7f7648180ULL + random());   // ptimeToMircoTime(sendTime));
         std::vector<unsigned char> tsHeaderContents = tsHeader.contents();

         // ------ No given target checksum ---------------------
         if(targetChecksum == ~0U) {
            computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
            targetChecksum = echoRequest.checksum();
         }
         // ------ Target checksum given ------------------------
         else {
            // Compute current checksum
            computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
            const uint16_t originalChecksum = echoRequest.checksum();

            // Compute value to tweak checksum to target value
            uint16_t diff = 0xffff - (targetChecksum - originalChecksum);
            if(originalChecksum > targetChecksum) {    // Handle necessary sum wrap!
               diff++;
            }
            tsHeader.checksumTweak(diff);

            // Compute new checksum (must be equal to target checksum!)
            tsHeaderContents = tsHeader.contents();
            computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
            assert(echoRequest.checksum() == targetChecksum);

//             const uint16_t newChecksum = echoRequest.checksum();
//             if(newChecksum != targetChecksum) {
//                std::cerr << "ERROR: Traceroute::sendICMPRequest() - Checksum differs from target checksum!" << std::endl;
//                // printf("ORIGIN=%x\n", originalChecksum);
//                // printf("TARGET=%x\n", targetChecksum);
//                // printf("NEW=   %x\n", newChecksum);
//                // printf("diff=%x\n", diff);
//                ::abort();
//             }
         }
      }

      SeqNumber++;
   }

   return(0);
}
