#include <stdio.h>
#include <stdlib.h>

// #include <boost/random/mersenne_twister.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

// boost::random::mt19937 gen;
boost::random::random_device gen;

int main(int argc, char** argv)
{
   boost::random::uniform_int_distribution<unsigned int> dist;   // (0, 0xffffffff);

   for(unsigned int i = 0; i < 200; i++) {
      unsigned int x = dist(gen);
      printf("%u:\t%u\n", i, x);
   }

   return 0;
}
