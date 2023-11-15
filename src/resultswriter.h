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
// Copyright (C) 2015-2024 by Thomas Dreibholz
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
   BZip2 = 2,
   XZ    = 3
};


class ResultsWriter
{
   public:
   ResultsWriter(const std::string&            programID,
                 const unsigned int            measurementID,
                 const std::string&            directory,
                 const std::string&            uniqueID,
                 const std::string&            prefix,
                 const unsigned int            transactionLength,
                 const uid_t                   uid,
                 const gid_t                   gid,
                 const ResultsWriterCompressor compressor);
   virtual ~ResultsWriter();

   void specifyOutputFormat(const std::string& outputFormatName,
                            const unsigned int outputFormatVersion);

   inline unsigned int measurementID() const {
      return MeasurementID;
   }

   bool prepare();
   bool changeFile(const bool createNewFile = true);
   bool mayStartNewTransaction();
   void insert(const std::string& tuple);

   static ResultsWriter* makeResultsWriter(std::set<ResultsWriter*>&       resultsWriterSet,
                                           const std::string&              programID,
                                           const unsigned int              measurementID,
                                           const boost::asio::ip::address& sourceAddress,
                                           const std::string&              resultsPrefix,
                                           const std::string&              resultsDirectory,
                                           const unsigned int              resultsTransactionLength,
                                           const uid_t                     uid,
                                           const gid_t                     gid,
                                           const ResultsWriterCompressor   compressor = ResultsWriterCompressor::XZ);

   protected:
   const std::string                     ProgramID;
   const unsigned int                    MeasurementID;
   const boost::filesystem::path         Directory;
   const std::string                     Prefix;
   const unsigned int                    TransactionLength;
   const uid_t                           UID;
   const gid_t                           GID;
   const ResultsWriterCompressor         Compressor;

   std::string                           UniqueID;
   boost::filesystem::path               TempFileName;
   boost::filesystem::path               TargetFileName;
   size_t                                Inserts;
   unsigned long long                    SeqNumber;
   std::ofstream                         OutputFile;
   boost::iostreams::filtering_ostream   OutputStream;
   std::chrono::steady_clock::time_point OutputCreationTime;
   std::string                           OutputFormatName;
   unsigned int                          OutputFormatVersion;
};

#endif
