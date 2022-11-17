#include <iostream>
#include <filesystem>
#include <cassert>

#include <boost/asio.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include "tools.h"


typedef std::chrono::high_resolution_clock   ReaderClock;
typedef std::chrono::time_point<ReaderClock> ReaderTimePoint;


// ###### Helper function to calculate "min" value ##########################
template<typename ReaderTimePoint> ReaderTimePoint makeMin(const ReaderTimePoint& timePoint)
{
   unsigned long long us = timePointToMicroseconds<ReaderTimePoint>(timePoint);
   us = us - (us % (60000000ULL));   // floor to minute
   return  microsecondsToTimePoint<ReaderTimePoint>(us);
}


int main()
{
   const unsigned long long t1 = 1666261441000000;
   const unsigned long long t2 = 1000000000000000;
   const unsigned long long t3 = 2000000000000000;
   const unsigned long long t4 = 1000000000123456;
   const ReaderTimePoint tp1 = microsecondsToTimePoint<ReaderTimePoint>(t1);
   const ReaderTimePoint tp2 = microsecondsToTimePoint<ReaderTimePoint>(t2);
   const ReaderTimePoint tp3 = microsecondsToTimePoint<ReaderTimePoint>(t3);
   const ReaderTimePoint tp4 = microsecondsToTimePoint<ReaderTimePoint>(t4);
   const std::string ts1 = timePointToString<ReaderTimePoint>(tp1, 0);
   const std::string ts2 = timePointToString<ReaderTimePoint>(tp2, 6);
   const std::string ts3 = timePointToString<ReaderTimePoint>(tp3, 0);
   const std::string ts4 = timePointToString<ReaderTimePoint>(tp4, 6);
   const ReaderTimePoint dp1 = makeMin<ReaderTimePoint>(tp1);
   const ReaderTimePoint dp2 = makeMin<ReaderTimePoint>(tp2);
   const ReaderTimePoint dp3 = makeMin<ReaderTimePoint>(tp3);
   const ReaderTimePoint dp4 = makeMin<ReaderTimePoint>(tp4);
   const std::string ds1 = timePointToString<ReaderTimePoint>(dp1, 0);
   const std::string ds2 = timePointToString<ReaderTimePoint>(dp2, 6);
   const std::string ds3 = timePointToString<ReaderTimePoint>(dp3, 0);
   const std::string ds4 = timePointToString<ReaderTimePoint>(dp4, 6);

   std::cout << t1 << "\t" << ts1 << "\t\t" << ds1 << "\n";
   std::cout << t2 << "\t" << ts2 << "\t" << ds2 << "\n";
   std::cout << t3 << "\t" << ts3 << "\t\t" << ds3 << "\n";
   std::cout << t4 << "\t" << ts4 << "\t" << ds4 << "\n";


   assert(ts1 == "2022-10-20 10:24:01");
   assert(ts2 == "2001-09-09 01:46:40.000000");
   assert(ts3 == "2033-05-18 03:33:20");
   assert(ts4 == "2001-09-09 01:46:40.123456");

   assert(ds1 == "2022-10-20 10:24:00");
   assert(ds2 == "2001-09-09 01:46:00.000000");
   assert(ds3 == "2033-05-18 03:33:00");
   assert(ds4 == "2001-09-09 01:46:00.000000");

   ReaderTimePoint now = nowInUTC<ReaderTimePoint>();
   std::cout << "now=" << timePointToString<ReaderTimePoint>(now, 6) << "\n";

//    for(unsigned int i = 0; i < 1000000; i++) {
//       now = nowInUTC<ReaderTimePoint>();
//    }

   std::cout << "now=" << timePointToString<ReaderTimePoint>(now, 6) << "\n";

   auto a1 = boost::asio::ip::make_address_v6(boost::asio::ip::v4_mapped,  boost::asio::ip::make_address_v4("1.2.3.4"));
   std::cout << "a1=" << a1 << "\n";


   unsigned long long x1 = 0x7fffffffffffffffULL;
   unsigned long long x2 = 0x8000000000000000ULL;
   unsigned long long x3 = 0xffffffffffffffffULL;

   long long y1 = (long long)x1;
   long long y2 = (long long)x2;
   long long y3 = (long long)x3;

   unsigned long long z1 = (unsigned long long)y1;
   unsigned long long z2 = (unsigned long long)y2;
   unsigned long long z3 = (unsigned long long)y3;

   boost::multiprecision::cpp_int w1 = (y1 < 0) ? boost::multiprecision::cpp_int(y1) : boost::multiprecision::cpp_int(y1) + 0x8000000000000000ULL;
   boost::multiprecision::cpp_int w2 = (y2 < 0) ? boost::multiprecision::cpp_int(y2) : boost::multiprecision::cpp_int(y2) + 0x8000000000000000ULL;
   boost::multiprecision::cpp_int w3 = (y3 < 0) ? boost::multiprecision::cpp_int(y3) : boost::multiprecision::cpp_int(y3) + 0x8000000000000000ULL;

   printf("%llu -> %lld -> %llu\n", x1, y1, z1);
   printf("%llu -> %lld -> %llu\n", x2, y2, z2);
   printf("%llu -> %lld -> %llu\n", x3, y3, z3);
   std::cout << w1 << "\n";
   std::cout << w2 << "\n";
   std::cout << w3 << "\n";

   return 0;
}
