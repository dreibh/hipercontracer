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

#include <boost/algorithm/string.hpp>

#include "compressortype.h"

struct CompressorTableEntry
{
   CompressorType     Type;
   const std::string  Name;
   const char*        Extension;
};

static const CompressorTableEntry CompressorTable[] = {
   { None,    "None",        "",     },
   { XZ,      "XZ",          ".xz"   },
   { BZip2,   "BZip2",       ".bz2"  },
   { GZip,    "GZip",        ".gz"   },
   { ZSTD,    "ZSTD",        ".zst"  },
   { ZLIB,    "ZLIB",        ".zz"   },
   { Invalid, std::string(), nullptr }
};


// ###### Obtain compressor from file name extension ########################
CompressorType getCompressorTypeFromName(const std::string& name)
{
   std::string lowercaseName(name);
   boost::algorithm::to_lower(lowercaseName);

   unsigned int i = 0;
   while(CompressorTable[i].Type != Invalid) {
      if(CompressorTable[i].Name == name) {
         return CompressorTable[i].Type;
      }
      i++;
   }

   return Invalid;
}


// ###### Obtain compressor from file name extension ########################
CompressorType obtainCompressorFromExtension(const std::filesystem::path& fileName)
{
   std::string extension(fileName.extension());
   boost::algorithm::to_lower(extension);

   unsigned int i = 0;
   while(CompressorTable[i].Type != Invalid) {
      if(CompressorTable[i].Extension == extension) {
         return CompressorTable[i].Type;
      }
      i++;
   }

   return None;
}


// ###### Get file extension for compressor #################################
const char* getExtensionForCompressor(const CompressorType type)
{
   unsigned int i = 0;
   while(CompressorTable[i].Type != Invalid) {
      if(CompressorTable[i].Type == type) {
         return CompressorTable[i].Extension;
      }
      i++;
   }
   return "";
}
