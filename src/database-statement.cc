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

#include "databaseclient-base.h"

#include <stdexcept>
#include <boost/asio/ip/address.hpp>
#include <boost/beast/core/detail/base64.hpp>


// ###### Constructor #######################################################
Statement::Statement(const DatabaseBackendType backend)
   : Backend(backend)
{
   Rows    = 0;
   InTuple = false;
}


// ###### Destructor ########################################################
Statement::~Statement()
{
}


// ###### Encode IP address #################################################
std::string Statement::encodeAddress(const boost::asio::ip::address& address) const
{
   std::stringstream ss;

   if(Backend & DatabaseBackendType::SQL_Generic) {
      if( ((Backend & DatabaseBackendType::SQL_MariaDB) == DatabaseBackendType::SQL_MariaDB ) &&
          (address.is_v4()) ) {
         // MySQL/MariaDB only has INET6 datatype. Make IPv4 addresses mapped.
         ss << std::quoted(boost::asio::ip::make_address_v6(boost::asio::ip::v4_mapped, address.to_v4()).to_string(), '\'', '\\');
      }
      else {
         ss << std::quoted(address.to_string(), '\'', '\\');
      }
   }
   else if(Backend & DatabaseBackendType::NoSQL_Generic) {
      char   encoded[boost::beast::detail::base64::encoded_size(16)];
      size_t length;
      if(address.is_v4()) {
         const boost::asio::ip::address_v4::bytes_type& b = address.to_v4().to_bytes();
         length = boost::beast::detail::base64::encode(&encoded, (void*)&b, 4);
      }
      else {
         const boost::asio::ip::address_v6::bytes_type& b = address.to_v6().to_bytes();
         length = boost::beast::detail::base64::encode(&encoded, (void*)&b, 16);
      }
      ss << "{\"$type\":\"0\",\"$binary\":\"" << std::string(encoded, length) << "\"}";
   }
   else {
      abort();
   }

   return ss.str();
}


// ###### Decode IP address #################################################
boost::asio::ip::address Statement::decodeAddress(const std::string& string) const
{
   boost::asio::ip::address address;

   if(Backend & DatabaseBackendType::SQL_Generic) {
      address = boost::asio::ip::make_address(string);
   }
   else if(Backend & DatabaseBackendType::NoSQL_Generic) {

      // NOTE: The address is already decoded in the string!
      //       => No need for Base64 decoding.
#if 0
      char decoded[boost::beast::detail::base64::decoded_size(string.size())];
      const std::pair<std::size_t, std::size_t> result =
         boost::beast::detail::base64::decode(&decoded, string.c_str(), string.size());
      if(result.first == 4) {
         address = boost::asio::ip::make_address_v4(*((boost::asio::ip::address_v4::bytes_type*)&decoded));
      }
      else if(result.first == 16) {
         address = boost::asio::ip::make_address_v6(*((boost::asio::ip::address_v6::bytes_type*)&decoded));
      }
#endif

      if(string.size() == 4) {
         address = boost::asio::ip::make_address_v4(*((boost::asio::ip::address_v4::bytes_type*)string.c_str()));
      }
      else if(string.size() == 16) {
         address = boost::asio::ip::make_address_v6(*((boost::asio::ip::address_v6::bytes_type*)string.c_str()));
      }
      else {
         throw std::runtime_error("Not an base64-encoded address");
      }
   }

   if(address.is_v6()) {
      const boost::asio::ip::address_v6 v6 = address.to_v6();
      if(v6.is_v4_mapped()) {
         return boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, v6);
      }
   }

   return address;
}


// ###### << operator #######################################################
std::ostream& operator<<(std::ostream& os, const Statement& statement)
{
   if(statement.isEmpty()) {
      os << "(empty)";
   }
   else {
      os << statement.str();
   }
   return os;
}
