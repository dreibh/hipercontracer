#include "tools.h"
#include <iostream>

typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemTimeDuration;

typedef std::chrono::high_resolution_clock   ReaderClock;
typedef std::chrono::time_point<ReaderClock> ReaderTimePoint;
typedef ReaderClock::duration                ReaderTimeDuration;

// Approximate offset to system time:
// NOTE: This is an *approximation*, for checking whether a time time
//       appears to be resonable!
const ReaderTimeDuration ReaderClockOffsetFromSystemTime(
   std::chrono::nanoseconds(
      nsSinceEpoch<SystemTimePoint>(SystemClock::now()) -
      nsSinceEpoch<ReaderTimePoint>(ReaderClock::now())
   )
);


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

   const SystemTimePoint    sysTimePoint  = SystemClock::now();
   const unsigned long long sysTimeStamp  = timePointToNanoseconds<SystemTimePoint>(sysTimePoint);
   std::cout << "sys:   ts=" << sysTimeStamp
             << "\ttp=" << timePointToString<SystemTimePoint>(sysTimePoint, 9) << "\n";
   std::cout << "sysInSeconds=" << sysTimeStamp / 1000000000 << "\n";

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
   const unsigned long long sysTSE = nsSinceEpoch<SystemTimePoint>(sysTimePoint);
   std::cout << "sysTSE= " << sysTSE << "\n";

   std::cout << "OFFSET= " << sysTSE - nowTSE << "\n";


   const ReaderTimePoint now = ReaderClock::now() + ReaderClockOffsetFromSystemTime;
   std::cout << "offsettedNow:   tp=" << timePointToString<ReaderTimePoint>(now, 9) << "\n";

   return 0;
}
