// ==========================================================================
//     _   _ _ ____            ____          _____
//    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
//    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
//    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
//    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
//
//       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
//                 https://www.nntb.no/~dreibh/hipercontracer/
// ==========================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2025 by Thomas Dreibholz
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
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include <boost/asio/ip/address.hpp>
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
                 const unsigned int            timestampDepth,
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
                                           const unsigned int              resultsTimestampDepth,
                                           const uid_t                     uid,
                                           const gid_t                     gid,
                                           const ResultsWriterCompressor   compressor = ResultsWriterCompressor::XZ);

   protected:
   const std::string                     ProgramID;
   const unsigned int                    MeasurementID;
   const std::filesystem::path           Directory;
   const std::string                     Prefix;
   const unsigned int                    TransactionLength;
   const unsigned int                    TimestampDepth;
   const uid_t                           UID;
   const gid_t                           GID;
   const ResultsWriterCompressor         Compressor;

   std::string                           UniqueID;
   std::filesystem::path                 TempFileName;
   std::filesystem::path                 TargetFileName;
   size_t                                Inserts;
   unsigned long long                    SeqNumber;
   std::ofstream                         OutputFile;
   boost::iostreams::filtering_ostream   OutputStream;
   std::chrono::steady_clock::time_point OutputCreationTime;
   std::string                           OutputFormatName;
   unsigned int                          OutputFormatVersion;
};

#endif
