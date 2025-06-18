#include "jittermodule-iqr.h"
#include "jittermodule-rfc3550.h"

#include <stdio.h>


int main(int argc, char *argv[])
{
   JitterModuleIQR j;
   // JitterModuleRFC3550 j;

   j.process(0xaa, 1000000000, 1100000000);
   j.process(0xaa, 2000000000, 2200000000);
   j.process(0xaa, 3000000000, 3100000000);
   j.process(0xaa, 4000000000, 4200000000);
   j.process(0x66, 5000000000, 5200000000);

   printf("P=%u\n", j.packets());
   printf("J=%llu\n", j.jitter() / 1000000ULL);
   printf("L=%llu\n", j.meanLatency() / 1000000ULL);

   return 0;
}
