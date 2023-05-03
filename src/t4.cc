#include <iostream>

#include "tools.h"


typedef std::chrono::high_resolution_clock   ReaderClock;
typedef std::chrono::time_point<ReaderClock> ReaderTimePoint;


int main(int argc, char** argv)
{
   const std::string updatedAt = "2023-05-02T09:40:18.123456";
   ReaderTimePoint  timeStamp;

   if(!stringToTimePoint<ReaderTimePoint>(updatedAt, timeStamp, "%Y-%m-%dT%H:%M:%S")) {
       std::cerr << "ERROR\n";
   }
   else {
      std::cout << timePointToString(timeStamp, 6) << "\n";
   }
}
