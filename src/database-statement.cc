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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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
