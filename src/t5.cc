#include "tools.h"
#include <iostream>

typedef std::chrono::high_resolution_clock   ReaderClock;
typedef std::chrono::time_point<ReaderClock> ReaderTimePoint;
typedef ReaderClock::duration                ReaderTimeDuration;

int main(int argc, char** argv)
{
   const unsigned long long ts = 0x17972cfc4c932d87ULL;

   const ReaderTimePoint now       = ReaderClock::now();
   const ReaderTimePoint timeStamp = nanosecondsToTimePoint<ReaderTimePoint>(ts);

   std::cout << "n=" << timePointToString<ReaderTimePoint>(now, 9)       << "\n";
   std::cout << "t=" << timePointToString<ReaderTimePoint>(timeStamp, 9) << "\n";
   if(now < timeStamp) {
      std::cerr << "Jævla faen!\n";
   }
   return 0;
}
