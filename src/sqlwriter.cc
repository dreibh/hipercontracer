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
// Copyright (C) 2015 by Thomas Dreibholz
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

#include "sqlwriter.h"

#include <boost/format.hpp>


// ###### Constructor #######################################################
SQLWriter::SQLWriter(const std::string& directory,
                     const std::string& uniqueID,
                     const std::string& tableName)
   : Directory(directory),
     UniqueID(uniqueID),
     TableName(tableName)
{
   Inserts   = 0;
   SeqNumber = 0;
}


// ###### Destructor ########################################################
SQLWriter::~SQLWriter()
{
   changeFile(false);
}


// ###### Prepare directories ###############################################
bool SQLWriter::prepare()
{
   try {
      boost::filesystem::create_directory(Directory);
      boost::filesystem::create_directory(Directory / "tmp");
   }
   catch(std::exception const& e) {
      std::cerr << "ERROR: Unable to prepare directories - " << e.what() << std::endl;
      return(false);
   }
   return(changeFile());
}


// ###### Change output file ################################################
bool SQLWriter::changeFile(const bool createNewFile)
{
   // ====== Close current file =============================================
   if(OutputFile.is_open()) {
      OutputFile << "COMMIT TRANSACTION" << std::endl;
      OutputFile.close();
      try {
         if(Inserts == 0) {
            // empty file -> just remove it!
            boost::filesystem::remove(TempFileName);
         }
         else {
            // file has contents -> move it!
            boost::filesystem::rename(TempFileName, TargetFileName);
         }
      }
      catch(std::exception const& e) {
         std::cerr << "ERROR: SQLWriter::changeFile() - " << e.what() << std::endl;
      }
   }

   // ====== Create new file ================================================
   Inserts = 0;
   SeqNumber++;
   if(createNewFile) {
      try {
         const std::string name = UniqueID + str(boost::format("-%09d.sql") % SeqNumber);
         TempFileName   = Directory / "tmp" / name;
         TargetFileName = Directory / name;
         OutputFile.open(TempFileName.c_str());
         OutputFile << "START TRANSACTION" << std::endl;
         return(OutputFile.good());
      }
      catch(std::exception const& e) {
         std::cerr << "ERROR: SQLWriter::changeFile() - " << e.what() << std::endl;
         return(false);
      }
   }
   return(true);
}


// ###### Generate INSERT statement #########################################
void SQLWriter::insert(const std::string& tuple)
{
   OutputFile << "INSERT INTO " << TableName << " VALUES ("
              << tuple
              << ");" << std::endl;
   Inserts++;
}
