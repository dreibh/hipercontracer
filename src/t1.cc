#include <iostream>

#include "outputstream.h"


void test(const char* name)
{
   const std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();

   OutputStream of;
   try {
      of.openStream(name);
      for(unsigned int i = 0; i < 100000000; i++) {
         of << "Test! " << name << " abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
      }
      of.flush();
      if(!of.closeStream()) {
         std::cerr << "SYNC FAILED " << name << "\n";
      }
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: " << e.what() << "\n";
   }

   const std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();
   std::cerr << "OK " << name << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms\n";
}


int main(int argc, char** argv)
{
   OutputStream of;

   of.openStream(std::cout);
   of << "COUT-TEST\n";
   of.closeStream();

   test("test.txt");
   test("test.txt.gz");
   test("test.txt.bz2");
   test("test.txt.xz");
   test("test.txt.zst");
   test("test.txt.zz");
   return 0;
}
