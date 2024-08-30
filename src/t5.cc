#include "tools.h"
#include <iostream>

typedef std::chrono::high_resolution_clock   ReaderClock;
typedef std::chrono::time_point<ReaderClock> ReaderTimePoint;
typedef ReaderClock::duration                ReaderTimeDuration;

int main(int argc, char** argv)
{
   const unsigned long long ts = 0x17972cfc4c932d87ULL;
   const ReaderTimePoint timeStamp = nanosecondsToTimePoint<ReaderTimePoint>(ts);

   std::cout << "t=" << timePointToString<ReaderTimePoint>(timeStamp, 9) << "\n";
   return 0;
}
