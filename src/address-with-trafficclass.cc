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

#include "address-with-trafficclass.h"

#include <boost/format.hpp>


// ###### Constructor #######################################################
AddressWithTrafficClass::AddressWithTrafficClass()
   : Address(), TrafficClass(0x00)
{
}


// ###### Constructor #######################################################
AddressWithTrafficClass::AddressWithTrafficClass(const AddressWithTrafficClass& addressWithTrafficClass)
   : Address(addressWithTrafficClass.address()), TrafficClass(addressWithTrafficClass.trafficClass())
{
}


// ###### Constructor #######################################################
AddressWithTrafficClass::AddressWithTrafficClass(boost::asio::ip::address address, const uint8_t trafficClassValue)
   : Address(address), TrafficClass(trafficClassValue)
{
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const AddressWithTrafficClass& addressWithTrafficClass)
{
   os << addressWithTrafficClass.address() << "/"
      << str(boost::format("%02x") % (unsigned int)addressWithTrafficClass.trafficClass());
   return os;
}


// ###### Comparison operator ###############################################
int operator<(const AddressWithTrafficClass& d1, const AddressWithTrafficClass& d2)
{
   return ( (d1.address() < d2.address()) ||
            ( (d1.address() == d2.address()) && (d1.trafficClass() < d2.trafficClass()) ) );
}


// ###### Comparison operator ###############################################
int operator==(const AddressWithTrafficClass& d1, const AddressWithTrafficClass& d2)
{
   return ( (d1.address() == d2.address()) &&
            (d1.trafficClass() == d2.trafficClass()) );
}
