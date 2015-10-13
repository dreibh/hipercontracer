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

#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <string>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/iostreams/filtering_stream.hpp>


class SQLWriter
{
   public:
   SQLWriter(const std::string& directory,
             const std::string& uniqueID,
             const std::string& tableName);
   virtual ~SQLWriter();

   bool prepare();
   bool changeFile(const bool createNewFile = true);
   void insert(const std::string& tuple);

   protected:
   const boost::filesystem::path       Directory;
   const std::string                   UniqueID;
   const std::string                   TableName;
   boost::filesystem::path             TempFileName;
   boost::filesystem::path             TargetFileName;
   size_t                              Inserts;
   unsigned long long                  SeqNumber;
   std::ofstream                       OutputFile;
   boost::iostreams::filtering_ostream OutputStream;
};

#endif
