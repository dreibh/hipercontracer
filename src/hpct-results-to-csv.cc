// =================================================================
//          #     #                 #     #
//          ##    #   ####   #####  ##    #  ######   #####
//          # #   #  #    #  #    # # #   #  #          #
//          #  #  #  #    #  #    # #  #  #  #####      #
//          #   # #  #    #  #####  #   # #  #          #
//          #    ##  #    #  #   #  #    ##  #          #
//          #     #   ####   #    # #     #  ######     #
//
//       ---   The NorNet Testbed for Multi-Homed Systems  ---
//                       https://www.nntb.no
// =================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2023 by Thomas Dreibholz
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact: dreibh@simula.no

#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>


// ###### Dump results file #################################################
bool dumpResultsFile(const std::filesystem::path& fileName)
{
   const std::string                   extension(fileName.extension());
   std::ifstream                       inputFile;
   boost::iostreams::filtering_istream inputStream;

   inputFile.open(fileName, std::ios_base::out | std::ios_base::binary);
   if(!inputFile.is_open()) {
      std::cerr << "ERROR: Failed to read " << fileName << "!\n";
      exit(1);
   }
   if(extension == ".xz") {
      inputStream.push(boost::iostreams::lzma_decompressor());
   }
   else if(extension == ".bz2") {
      inputStream.push(boost::iostreams::bzip2_decompressor());
   }
   else if(extension == ".gz") {
      inputStream.push(boost::iostreams::gzip_decompressor());
   }
   inputStream.push(inputFile);

   std::string line;
   while(std::getline(inputStream, line)) {
      std::cout << line << "\n";
   }

   return true;
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Initialize =====================================================

   for(unsigned int i = 1; i < argc; i++) {
      dumpResultsFile(std::filesystem::path(argv[i]));
   }

   return 0;
}
