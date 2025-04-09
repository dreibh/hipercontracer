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
   Compressor = None;
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
   closeStream();

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
      if(Compressor == FromExtension) {
         Compressor = obtainCompressorFromExtension(FileName);
      }
      switch(Compressor) {
         case XZ: {
            const boost::iostreams::lzma_params params(
               boost::iostreams::lzma::default_compression,
               std::thread::hardware_concurrency());
            push(boost::iostreams::lzma_compressor(params));
           }
          break;
         case BZip2:
            push(boost::iostreams::bzip2_compressor());
          break;
         case GZip:
            push(boost::iostreams::gzip_compressor());
          break;
         case ZSTD:
            push(boost::iostreams::zstd_compressor());
          break;
         case ZLIB:
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
bool OutputStream::closeStream(const bool sync)
{
   // ====== Synchronise and close file =====================================
   bool success = false;
   if(FileName != std::filesystem::path()) {
      if(sync) {
         // ------ Synchronise, then rename temporary output file --------------
         strict_sync();
         success = good();
         if(success) {
            std::filesystem::rename(TmpFileName, FileName);
            success = true;
         }
      }
      if(!success) {
         std::filesystem::remove(TmpFileName);
      }
   }
   reset();

   // ====== Clean up =======================================================
   if(Sink) {
      delete Sink;
      Sink = nullptr;
   }
   FileName    = std::filesystem::path();
   TmpFileName = std::filesystem::path();

   return success;
}
