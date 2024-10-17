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

#include "destinationinfo.h"

#include <boost/format.hpp>


// ###### Constructor #######################################################
DestinationInfo::DestinationInfo()
   : Identifier(0),
     Address(),
     TrafficClass(0x00)
{
}


// ###### Constructor #######################################################
DestinationInfo::DestinationInfo(const DestinationInfo& destinationInfo)
   : Identifier(destinationInfo.identifier()),
     Address(destinationInfo.address()),
     TrafficClass(destinationInfo.trafficClass())
{
}


// ###### Constructor #######################################################
DestinationInfo::DestinationInfo(const boost::asio::ip::address& address,
                                 const uint8_t                   trafficClassValue,
                                 const uint32_t                  identifier)
   : Identifier(identifier),
     Address(address),
     TrafficClass(trafficClassValue)
{
}


// ###### Output operator ###################################################
std::ostream& operator<<(std::ostream& os, const DestinationInfo& destinationInfo)
{
   os << destinationInfo.address() << "/"
      << str(boost::format("0x%02x") % (unsigned int)destinationInfo.trafficClass());
   return os;
}


// ###### Comparison operator ###############################################
int operator<(const DestinationInfo& destinationInfo1,
              const DestinationInfo& destinationInfo2)
{
   return ( (destinationInfo1.address() < destinationInfo2.address()) ||
            ( (destinationInfo1.address() == destinationInfo2.address()) &&
              (destinationInfo1.trafficClass() < destinationInfo2.trafficClass()) ) );
}


// ###### Comparison operator ###############################################
int operator==(const DestinationInfo& destinationInfo1,
               const DestinationInfo& destinationInfo2)
{
   return ( (destinationInfo1.address()      == destinationInfo2.address()) &&
            (destinationInfo1.trafficClass() == destinationInfo2.trafficClass()) );
}
