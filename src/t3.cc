#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <byteswap.h>

#include "traceserviceheader.h"


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   TraceServiceHeader ts;


   uint64_t a = 4102441199999999ULL;
   ts.sendTimeStamp(a);
   uint64_t b = *((uint64_t*)&ts.Data[8]);
   printf("a=%16llx b=%18llx\n", (unsigned long long)a, (unsigned long long)b);

   uint64_t c = a;
   uint64_t d = bswap_64(c);
   printf("c=%16llx d=%18llx\n", (unsigned long long)c, (unsigned long long)d);

//    assert(a == b);
//    assert(c == d);
//    assert(b == d);

   puts("OK!");
   return(0);
}
