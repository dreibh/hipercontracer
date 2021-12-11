#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <chrono>
#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/basic_resolver.hpp>


// ###### Convert chrono time to string with given format ###################
template<typename T> std::string getTimeString(const typename T::time_point& time,
                                               const char*                   format = "%F %T")
{
   const std::time_t timeInTimeT = T::to_time_t(time);
   std::tm           timeInTM;
   std::stringstream ss;

   gmtime_r(&timeInTimeT, &timeInTM);
   ss << std::put_time(&timeInTM, format);
   return ss.str();
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{

   boost::asio::io_service io_service;

   boost::asio::ip::tcp::resolver resolver(io_service);
   boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve("www.ietf.org", "http");
   for(boost::asio::ip::tcp::resolver::iterator iterator = endpoints.begin(); iterator != endpoints.end(); iterator++) {
       const boost::asio::ip::tcp::endpoint& endpoint = *iterator;
       std::cout << endpoint.address() << std::endl;
   }

   return 0;



   std::cout << "A=" << boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::universal_time()) << std::endl;


   const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

   std::time_t tt = std::chrono::system_clock::to_time_t(now);
   std::cout << "B=" << std::put_time(std::localtime(&tt), "%Y/%m/%d %T") << std::endl;

   std::cout << "C1=" << getTimeString<std::chrono::system_clock>(now) << std::endl;
   std::cout << "C2=" << getTimeString<std::chrono::system_clock>(now, "%Y%m%dT%H%M%S") << std::endl;


   const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
   usleep(1234567);
   const std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
   unsigned long long diff = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

   std::cout << "d=" << diff << std::endl;

   return(0);
}
