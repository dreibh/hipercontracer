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
#include <set>

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/program_options.hpp>



// ###### Dump results file #################################################
bool dumpResultsFile(boost::iostreams::filtering_ostream& outputStream,
                     const std::filesystem::path&         fileName)
{
   const std::string                   extension(fileName.extension());
   std::ifstream                       inputFile;
   boost::iostreams::filtering_istream inputStream;

   inputFile.open(fileName, std::ios_base::in | std::ios_base::binary);
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
      outputStream << line << "\n";
   }

   return true;
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   std::vector<std::filesystem::path> inputFileNameList;
   std::filesystem::path              outputFileName;

   // ====== Initialize =====================================================
   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )

      ( "output,o",
           boost::program_options::value<std::filesystem::path>(&outputFileName)->default_value(std::filesystem::path()),
           "Output file" )
   ;
   boost::program_options::options_description hiddenOptions;
   hiddenOptions.add_options()
      ("input,i", boost::program_options::value<std::vector<std::filesystem::path>>(&inputFileNameList))
   ;
   boost::program_options::options_description allOptions;
   allOptions.add(commandLineOptions);
   allOptions.add(hiddenOptions);
   boost::program_options::positional_options_description positionalParameters;
   positionalParameters.add("input", -1);

   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
                                       style(
                                          boost::program_options::command_line_style::style_t::default_style|
                                          boost::program_options::command_line_style::style_t::allow_long_disguise
                                       ).
                                       options(allOptions).positional(positionalParameters).
                                       run(), vm);
      boost::program_options::notify(vm);
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: Bad parameter: " << e.what() << "\n";
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << "\n"
                 << commandLineOptions;
       return 1;
   }


   // ====== Open output file ===============================================
   const std::string                   extension(outputFileName.extension());
   std::ofstream                       outputFile;
   boost::iostreams::filtering_ostream outputStream;
   if(outputFileName != std::filesystem::path()) {
      outputFile.open(outputFileName, std::ios_base::out | std::ios_base::binary);
      if(!outputFile.is_open()) {
         std::cerr << "ERROR: Failed to create " << outputFileName << "!\n";
         exit(1);
      }
   std::cout << "e=" << extension<<"\n";
      if(extension == ".xz") {
         outputStream.push(boost::iostreams::lzma_compressor());
      }
      else if(extension == ".bz2") {
         outputStream.push(boost::iostreams::bzip2_compressor());
      }
      else if(extension == ".gz") {
         outputStream.push(boost::iostreams::gzip_compressor());
      }
      outputStream.push(outputFile);
   }
   else {
      outputStream.push(std::cout);
   }

   // ====== Dump input files ===============================================
   std::set<std::filesystem::path> inputFileNameSet(inputFileNameList.begin(),
                                                    inputFileNameList.end());
   for(const std::filesystem::path& inputFileName : inputFileNameSet) {
      dumpResultsFile(outputStream, inputFileName);
   }

   return 0;
}
