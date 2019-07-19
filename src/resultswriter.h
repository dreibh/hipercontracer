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
// Copyright (C) 2015-2019 by Thomas Dreibholz
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

#ifndef RESULTSWRITER_H
#define RESULTSWRITER_H

#include <chrono>
#include <fstream>
#include <set>
#include <string>

#include <boost/asio/ip/address.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filtering_stream.hpp>


enum ResultsWriterCompressor {
   None  = 0,
   GZip  = 1,
   BZip2 = 2
   // XZ = 3
};


class ResultsWriter
{
   public:
   ResultsWriter(const std::string&            directory,
                 const std::string&            uniqueID,
                 const std::string&            formatName,
                 const unsigned int            transactionLength,
                 const uid_t                   uid,
                 const gid_t                   gid,
                 const ResultsWriterCompressor compressor);
   virtual ~ResultsWriter();

   bool prepare();
   bool changeFile(const bool createNewFile = true);
   bool mayStartNewTransaction();
   void insert(const std::string& tuple);

   static ResultsWriter* makeResultsWriter(std::set<ResultsWriter*>&       resultsWriterSet,
                                           const boost::asio::ip::address& sourceAddress,
                                           const std::string&              resultsFormat,
                                           const std::string&              resultsDirectory,
                                           const unsigned int              resultsTransactionLength,
                                           const uid_t                     uid,
                                           const gid_t                     gid,
                                           const ResultsWriterCompressor   compressor = BZip2);

   protected:
   const boost::filesystem::path         Directory;
   const std::string                     UniqueID;
   const std::string                     FormatName;
   const ResultsWriterCompressor         Compressor;
   const unsigned int                    TransactionLength;
   const uid_t                           UID;
   const gid_t                           GID;
   boost::filesystem::path               TempFileName;
   boost::filesystem::path               TargetFileName;
   size_t                                Inserts;
   unsigned long long                    SeqNumber;
   std::ofstream                         OutputFile;
   boost::iostreams::filtering_ostream   OutputStream;
   std::chrono::steady_clock::time_point OutputCreationTime;
};

#endif
