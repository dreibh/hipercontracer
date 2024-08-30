#include "tools.h"
#include <iostream>

typedef std::chrono::steady_clock            ResultClock;
typedef std::chrono::time_point<ResultClock> ResultTimePoint;
typedef ResultClock::duration                ResultDuration;

int main(int argc, char** argv)
{
   const unsigned long long ts = 0x17972cfc4c932d87ULL;
   const ResultTimePoint timeStamp = nanosecondsToTimePoint<ResultTimePoint>(ts);

   std::cout << "t=" << timePointToString<ResultTimePoint>(timeStamp, 9) << "\n";
   return 0;
}
