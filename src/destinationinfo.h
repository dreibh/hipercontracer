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

#ifndef DESTINATIONINFO_H
#define DESTINATIONINFO_H

#include <boost/asio/ip/address.hpp>


class DestinationInfo {
   public:
   DestinationInfo();
   DestinationInfo(const DestinationInfo& destinationInfo);
   DestinationInfo(const boost::asio::ip::address& address,
                   const uint8_t                   trafficClassValue,
                   const uint32_t                  identifier = 0);

   inline uint32_t identifier() const {
      return(Identifier);
   }
   inline const boost::asio::ip::address& address() const {
      return(Address);
   }
   inline const uint8_t& trafficClass() const {
      return(TrafficClass);
   }

   inline void setIdentifier(const uint8_t identifier) {
      Identifier = identifier;
   }
   inline void setAddress(const boost::asio::ip::address& address) {
      Address = address;
   }
   inline void setTrafficClass(const uint8_t trafficClass) {
      TrafficClass = trafficClass;
   }

   private:
   uint32_t                 Identifier;
   boost::asio::ip::address Address;
   uint8_t                  TrafficClass;
};


std::ostream& operator<<(std::ostream& os, const DestinationInfo& destinationInfo);
int operator<(const DestinationInfo& destinationInfo1, const DestinationInfo& destinationInfo2);
int operator==(const DestinationInfo& destinationInfo1, const DestinationInfo& destinationInfo2);

#endif
