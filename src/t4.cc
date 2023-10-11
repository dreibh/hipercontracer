#include <iostream>

#include "tools.h"


// typedef std::chrono::high_resolution_clock   ResultClock;
typedef std::chrono::steady_clock            ResultClock;
typedef std::chrono::time_point<ResultClock> ResultTimePoint;
typedef ResultClock::duration                ResultDuration;


typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemDuration;


int main(int argc, char** argv)
{
   static const ResultTimePoint nowRT                             = nowInUTC<ResultTimePoint>();
//    static const SystemTimePoint nowST                             = nowInUTC<SystemTimePoint>();
//    static const SystemTimePoint HiPerConTracerEpochST             = SystemClock::from_time_t(212803200);
//    static const SystemDuration durationSinceHiPerConTracerEpochST = nowST - HiPerConTracerEpochST;
//    static const std::chrono::seconds secsSinceHiPerConTracerEpoch(    std::chrono::duration_cast<std::chrono::seconds>(durationSinceHiPerConTracerEpochST).count());
//    static const ResultTimePoint HiPerConTracerEpochRT = nowRT - secsSinceHiPerConTracerEpoch;

   static const ResultTimePoint HiPerConTracerEpochRT = nowRT - std::chrono::seconds(212803200);


//    const SystemTimePoint HiPerConTracerEpoch1 = SystemClock::from_time_t(212803200);
//    const unsigned long long ms1 = std::chrono::duration_cast<std::chrono::microseconds>(nowST - HiPerConTracerEpochST).count();
   const unsigned long long ms2 = std::chrono::duration_cast<std::chrono::microseconds>(nowRT - HiPerConTracerEpochRT).count();

//    std::cout << "ms1 = " << ms1 << "\t" << timePointToString(HiPerConTracerEpochST, 6) << "\n";
   std::cout << "ms2 = " << ms2 << "\t" << timePointToString(HiPerConTracerEpochRT, 6) << "\n";
}
