#include <filesystem>
#include <iostream>

// g++ t5.cc -o t5 -std=c++17


class ImporterManager
{


};


int main(int argc, char** argv)
{
   std::filesystem::path path(".");

   for (const std::filesystem::directory_entry& dirEntry : std::filesystem::recursive_directory_iterator(path)) {
      if(dirEntry.is_regular_file()) {
         std::cout << dirEntry.path() << std::endl;
      }
   }
}
