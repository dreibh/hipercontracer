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

#include "inputstream.h"

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
InputStream::InputStream()
{
   Source       = nullptr;
   StreamBuffer = nullptr;
   Compressor   = CT_None;
}


// ###### Destructor ########################################################
InputStream::~InputStream()
{
   closeStream();
}


// ###### Initialise input stream to std::istream ###########################
bool InputStream::openStream(std::istream& os)
{
   closeStream();
   push(os);
   return true;
}


// ###### Initialise input stream to input file #############################
bool InputStream::openStream(const std::filesystem::path& fileName,
                              const CompressorType        compressor)
{
   // ====== Reset ==========================================================
   closeStream();

   // ====== Initialise input steam to file =================================
   Compressor = compressor;
   FileName   = fileName;
   if(FileName != std::filesystem::path()) {
      // ------ Open temporary input file -----------------------------------
      int handle = ::open(FileName.c_str(), O_RDONLY);
      if(handle < 0) {
         throw std::runtime_error(std::string(strerror(errno)));
      }
#ifdef POSIX_FADV_SEQUENTIAL
      posix_fadvise(handle, 0, 0, POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED|POSIX_FADV_NOREUSE);
#endif
      Source = new boost::iostreams::file_descriptor_source(handle, boost::iostreams::file_descriptor_flags::close_handle);
      assert(Source != nullptr);
      // StreamBuffer = new boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>(
      //                   handle, boost::iostreams::file_descriptor_flags::close_handle);
      // assert(StreamBuffer != nullptr);

      // ------ Configure the compressor ------------------------------------
      if(Compressor == CT_FromExtension) {
         Compressor = obtainCompressorFromExtension(FileName);
      }
      switch(Compressor) {
         case CT_XZ: {
            const boost::iostreams::lzma_params params(
               boost::iostreams::lzma::default_compression,
               std::thread::hardware_concurrency());
            push(boost::iostreams::lzma_decompressor(params));
           }
          break;
         case CT_BZip2:
            push(boost::iostreams::bzip2_decompressor());
          break;
         case CT_GZip:
            push(boost::iostreams::gzip_decompressor());
          break;
         case CT_ZSTD:
            push(boost::iostreams::zstd_decompressor());
          break;
         case CT_ZLIB:
            push(boost::iostreams::zlib_decompressor());
          break;
         default:
          break;
      }
      push(*Source);
      // push(*StreamBuffer);

      return true;
   }
   return false;
}


// ###### Close input stream ###############################################
void InputStream::closeStream()
{
   reset();

   // ====== Clean up =======================================================
   if(Source) {
      delete Source;
      Source = nullptr;
   }
   if(StreamBuffer) {
      delete StreamBuffer;
      StreamBuffer = nullptr;
   }
   FileName = std::filesystem::path();
}
