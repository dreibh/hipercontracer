#include <iostream>

#include "outputstream.h"


void test(const char* name)
{
   OutputStream of;
   try {
      of.openStream(name);
      for(unsigned int i = 0; i < 1000; i++) {
         of << "Test! " << name << "\n";
      }
      of.flush();
      if(!of.closeStream()) {
         std::cerr << "SYNC FAILED " << name << "\n";
      }
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: " << e.what() << "\n";
   }
   std::cerr << "OK " << name << "\n";
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
