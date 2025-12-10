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
// Copyright (C) 2015-2026 by Thomas Dreibholz
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

#ifndef COMPRESSORTYPE_H
#define COMPRESSORTYPE_H

#include <filesystem>
#include <boost/algorithm/string.hpp>


enum CompressorType {
   CT_Invalid       = 0,
   CT_FromExtension = 1,

   CT_None          = 2,
   CT_GZip          = 3,
   CT_BZip2         = 4,
   CT_XZ            = 5,
   CT_ZSTD          = 6,
   CT_ZLIB          = 7
};

CompressorType getCompressorTypeFromName(const std::string& name);
CompressorType obtainCompressorFromExtension(const std::filesystem::path& fileName);
const char* getExtensionForCompressor(const CompressorType type);

#endif
