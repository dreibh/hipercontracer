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

#ifndef DATABASE_STATEMENT_H
#define DATABASE_STATEMENT_H

#include <sstream>

#include "database-statement.h"


// Forward declarations to avoid dependency on BOOST headers:
namespace boost::asio::ip { class address; };


class Statement : public std::stringstream
{
   public:
   Statement(const DatabaseBackendType backend);
   ~Statement();

   inline DatabaseBackendType getBackend() const {
      return Backend;
   }

   inline void clear() {
      str(std::string());
      Rows    = 0;
      InTuple = false;
   }

   inline bool isEmpty() const {
      return str().size() == 0;
   }

   inline bool isValid() const {
      return (!InTuple) && ((Rows > 0) || (!str().empty()));
   }

   inline size_t getRows() const {
      return Rows;
   }

   inline void beginRow(const bool multipleLines = true) {
      assert(!InTuple);
      InTuple = true;
      if(Backend & DatabaseBackendType::SQL_Generic) {
         if(multipleLines) {
            *this << ((Rows == 0) ? "\n(" : ",\n(");
         }
         else {
            *this << ((Rows == 0) ? "(" : ",(");
         }
      }
      else if(Backend & DatabaseBackendType::NoSQL_Generic) {
         if(multipleLines) {
            *this << ((Rows == 0) ? "\n{" : ",\n{");
         }
         else {
            *this << ((Rows == 0) ? "{" : ",{");
         }
      }
      else {
         abort();
      }
   }

   inline void endRow() {
      assert(InTuple);
      InTuple = false;
      Rows++;
      if(Backend & DatabaseBackendType::SQL_Generic) {
         *this << ")";
      }
      else if(Backend & DatabaseBackendType::NoSQL_Generic) {
         *this << "}";
      }
      else {
         abort();
      }
   }

   inline const char* sep() const {
      assert(InTuple);
      if(Backend & DatabaseBackendType::SQL_Generic) {
         return ",";
      }
      else if(Backend & DatabaseBackendType::NoSQL_Generic) {
         return ", ";
      }
      else {
         abort();
      }
   }

   inline std::string quote(const std::string& string) const {
      assert(InTuple);
      std::stringstream ss;
      if(Backend & DatabaseBackendType::SQL_Generic) {
         ss << std::quoted(string, '\'', '\\');
      }
      else if(Backend & DatabaseBackendType::NoSQL_Generic) {
         ss << std::quoted(string, '"', '\\');
      }
      else {
         abort();
      }
      return ss.str();
   }

   inline std::string quoteOrNull(const std::string& string) const {
      if(string.size() == 0) {
         if(Backend & DatabaseBackendType::SQL_Generic) {
            return "NULL";
         }
         else if(Backend & DatabaseBackendType::NoSQL_Generic) {
            return "null";
         }
         else {
            abort();
         }
      }
      return quote(string);
   }

   std::string encodeAddress(const boost::asio::ip::address& address) const;
   boost::asio::ip::address decodeAddress(const std::string& string) const;

   friend std::ostream& operator<<(std::ostream& os, const Statement& statement);

   private:
   const DatabaseBackendType Backend;
   size_t                    Rows;
   bool                      InTuple;
};

#endif
