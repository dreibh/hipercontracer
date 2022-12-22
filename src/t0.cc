#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include <math.h>


class JitterRFC3550
{
   public:
   JitterRFC3550();

   inline unsigned long long jitter()  const { return (unsigned long long)rint(Jitter); }
   inline unsigned int       packets() const { return Packets;                          }

   void process(const unsigned long long sendTimeStamp,
                const unsigned long long receiveTimeStamp);

   private:
   unsigned long long PrevSendTimeStamp;
   unsigned long long PrevReceiveTimeStamp;
   unsigned int       Packets;
   double             Jitter;
};



// ###### Constructor #######################################################
JitterRFC3550::JitterRFC3550()
{
   Packets = 0;
   Jitter  = 0.0;
}


// ###### Process new packet's time stamps ##################################
void JitterRFC3550::process(const unsigned long long sendTimeStamp,
                            const unsigned long long receiveTimeStamp)
{
   if(Packets > 0) {
      // Jitter calculation according to Subsubsection 6.4.1 of RFC 3550:
      const double difference =
         ((double)receiveTimeStamp - (double)sendTimeStamp) -
         ((double)PrevReceiveTimeStamp - (double)PrevSendTimeStamp);
      Jitter = Jitter +  fabs(difference) / 16.0;
   }
   Packets++;
   PrevSendTimeStamp    = sendTimeStamp;
   PrevReceiveTimeStamp = receiveTimeStamp;
}


int main(int argc, char *argv[])
{
   JitterRFC3550 j;

   j.process(1000000000, 1111000000);
   j.process(2000000000, 2211000000);
   j.process(3000000000, 3111000000);
   j.process(3000000000, 3211000000);

   printf("P=%u\n", j.packets());
   printf("J=%llu\n", j.jitter());

   return 0;
}
