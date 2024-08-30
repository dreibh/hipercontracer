#include "tools.h"
#include <iostream>

typedef std::chrono::high_resolution_clock   ReaderClock;
typedef std::chrono::time_point<ReaderClock> ReaderTimePoint;
typedef ReaderClock::duration                ReaderTimeDuration;

typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemTimeDuration;

int main(int argc, char** argv)
{
   const unsigned long long testTimeStamp = 0x17972cfc4c932d87ULL;
   const ReaderTimePoint    testTimePoint = nanosecondsToTimePoint<ReaderTimePoint>(testTimeStamp);
   std::cout << "test:  ts=" << testTimeStamp
             << "\ttp=" << timePointToString<ReaderTimePoint>(testTimePoint, 9) << "\n";

   const ReaderTimePoint    nowTimePoint  = ReaderClock::now();
   const unsigned long long nowTimeStamp  = timePointToNanoseconds<ReaderTimePoint>(nowTimePoint);
   std::cout << "now:   ts=" << nowTimeStamp
             << "\ttp=" << timePointToString<ReaderTimePoint>(nowTimePoint, 9) << "\n";
   std::cout << "nowInSeconds=" << nowTimeStamp / 1000000000 << "\n";

   if(testTimeStamp > nowTimeStamp) {
      std::cerr << "Jævla faen TS!\n";
   }
   if(testTimePoint > nowTimePoint) {
      std::cerr << "Jævla faen TP!\n";
   }


   const unsigned long long testTSE = nsSinceEpoch<ReaderTimePoint>(testTimePoint);
   std::cout << "testTSE=" << testTSE << "\n";
   const unsigned long long nowTSE = nsSinceEpoch<ReaderTimePoint>(nowTimePoint);
   std::cout << "nowTSE= " << nowTSE << "\n";

   return 0;
}
