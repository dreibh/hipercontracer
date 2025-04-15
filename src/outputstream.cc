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

#include "outputstream.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <thread>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/zstd.hpp>


// ###### Constructor #######################################################
OutputStream::OutputStream()
{
   Sink       = nullptr;
   Compressor = CT_None;
}


// ###### Destructor ########################################################
OutputStream::~OutputStream()
{
   closeStream(false);
}


// ###### Initialise output stream to std::ostream ##########################
bool OutputStream::openStream(std::ostream& os)
{
   closeStream(false);
   push(os);
   return true;
}


// ###### Initialise output stream to output file ###########################
bool OutputStream::openStream(const std::filesystem::path& fileName,
                              const CompressorType         compressor)
{
   // ====== Reset ==========================================================
   closeStream(false);

   // ====== Initialise output steam to file ================================
   Compressor = compressor;
   FileName   = fileName;
   if(FileName != std::filesystem::path()) {
      TmpFileName = FileName.string() + ".tmp";

      // ------ Remove output file, if it is existing -----------------------
      std::filesystem::remove(FileName);

      // ------ Open temporary output file ----------------------------------
      int handle = ::open(TmpFileName.c_str(),
                          O_CREAT|O_TRUNC|O_WRONLY,
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if(handle < 0) {
         throw std::runtime_error(std::string(strerror(errno)));
      }
#ifdef POSIX_FADV_SEQUENTIAL
      posix_fadvise(handle, 0, 0, POSIX_FADV_SEQUENTIAL|POSIX_FADV_NOREUSE);
#endif
      Sink = new boost::iostreams::file_descriptor_sink(handle, boost::iostreams::file_descriptor_flags::close_handle);
      assert(Sink != nullptr);

      // ------ Configure the compressor ------------------------------------
      if(Compressor == CT_FromExtension) {
         Compressor = obtainCompressorFromExtension(FileName);
      }
      switch(Compressor) {
         case CT_XZ: {
            const boost::iostreams::lzma_params params(
               boost::iostreams::lzma::default_compression,
               std::thread::hardware_concurrency());
            push(boost::iostreams::lzma_compressor(params));
           }
          break;
         case CT_BZip2:
            push(boost::iostreams::bzip2_compressor());
          break;
         case CT_GZip:
            push(boost::iostreams::gzip_compressor());
          break;
         case CT_ZSTD:
            push(boost::iostreams::zstd_compressor());
          break;
         case CT_ZLIB:
            push(boost::iostreams::zlib_compressor());
          break;
         default:
          break;
      }
      push(*Sink);

      return true;
   }
   return false;
}


// ###### Close output stream ###############################################
void OutputStream::closeStream(const bool sync)
{
   bool success = false;

   // ====== Synchronise ====================================================
   if(sync) {
      if(is_complete()) {
         flush();
         strict_sync();
         success = ( is_complete() && good() );
      }
      if(!success) {
         throw std::runtime_error("Incomplete output stream");
      }
   }

   // ====== Close file =====================================================
   reset();
   if(Sink) {
      Sink->close();
      delete Sink;
      Sink = nullptr;
   }

   // ====== Rename temporary output file ===================================
   if(!FileName.empty()) {
      if(success) {
         std::filesystem::rename(TmpFileName, FileName);
      }
      else {
         std::filesystem::remove(TmpFileName);
      }
   }

   // ====== Clean up =======================================================
   FileName    = std::filesystem::path();
   TmpFileName = std::filesystem::path();
}
