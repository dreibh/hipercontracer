#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>


// ###### Randomly deviate interval with given deviation percentage #########
// The random value will be chosen out of:
// [interval - deviation * interval, interval + deviation * interval]
unsigned long long deviatedInterval(const unsigned long long interval,
                                    const double             deviation)
{
   assert(deviation >= 0.0);
   assert(deviation <= 1.0);

   const long long          d     = (long long)(interval * deviation);
   const unsigned long long value =
      ((long long)interval - d) + (std::rand() % (2 * d + 1));
   return value;
}


int main(int argc, char** argv)
{
   const unsigned int n = 1000000000;

   std::srand(std::time(nullptr));

   long long a = +1000000000;
   long long b = -1000000000;
   double c = 0;
   for(unsigned int i = 0; i < n; i++) {
      const long long x = deviatedInterval(1000000, 0.1);
      a = std::min(a, (long long)x);
      b = std::max(b, (long long)x);
      c = c + x;
   }
   c /= n;

   printf("min = %llu\n", a);
   printf("max = %llu\n", b);
   printf("avg = %1.2f\n", c);

   return 0;
}
