#include <iostream>

#include "inputstream.h"


void test(const char* name)
{
   unsigned int n = 0;
   InputStream is;
   try {
      is.openStream(name);
      std::string line;
      while(std::getline(is, line)) {
         n++;
      }
      is.closeStream();
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: " << e.what() << "\n";
   }
   std::cerr << "OK " << name << "\t" << n << "\n";
}


int main(int argc, char** argv)
{
   // InputStream is;
   //
   // is.openStream(std::cout);
   // is << "COUT-TEST\n";
   // is.closeStream();

   test("test.txt");
   test("test.txt.gz");
   test("test.txt.bz2");
   test("test.txt.xz");
   test("test.txt.zst");
   test("test.txt.zz");
   return 0;
}
