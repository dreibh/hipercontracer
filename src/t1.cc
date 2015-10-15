#include "sqlwriter.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <boost/date_time/posix_time/posix_time.hpp>


int main(int argc, char** argv)
{
   std::string sqlDirectory = "/storage/xy";

   // ====== Initialize =====================================================
   SQLWriter w(sqlDirectory, "UniqueID", "TracerouteTable", 60);
   if(!w.prepare()) {
      return 1;
   }

   w.insert("1,2,3,4");
   w.changeFile();
   w.insert("5,6,7,8");
   w.insert("5,6,7,7");
   w.changeFile();
   w.insert("8,8,8,8");
   w.insert("8,8,9,9");
   w.mayStartNewTransaction();


   const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
   std::cout << "now=" << boost::posix_time::to_simple_string(now) << std::endl;
}
