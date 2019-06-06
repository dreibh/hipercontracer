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
// Copyright (C) 2015-2019 by Thomas Dreibholz
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

#ifndef DESTINATION_H
#define DESTINATION_H

#include <boost/asio/ip/address.hpp>


class AddressWithTrafficClass {
   public:
   AddressWithTrafficClass();
   AddressWithTrafficClass(const AddressWithTrafficClass& addressWithTrafficClass);
   AddressWithTrafficClass(boost::asio::ip::address address, const uint8_t trafficClassValue);

   inline const boost::asio::ip::address& address() const {
      return(Address);
   }
   inline const uint8_t& trafficClass() const {
      return(TrafficClass);
   }

   private:
   boost::asio::ip::address Address;
   uint8_t                  TrafficClass;
};


std::ostream& operator<<(std::ostream& os, const AddressWithTrafficClass& addressWithTrafficClass);
int operator<(const AddressWithTrafficClass& d1, const AddressWithTrafficClass& d2);
// int operator<=(const AddressWithTrafficClass& d1, const AddressWithTrafficClass& d2);
int operator==(const AddressWithTrafficClass& d1, const AddressWithTrafficClass& d2);

#endif
