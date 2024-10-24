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

   inline const boost::asio::ip::address& address() const {
      return Address;
   }
   inline const uint8_t& trafficClass() const {
      return TrafficClass;
   }
   inline uint32_t identifier() const {
      return Identifier;
   }

   inline void setAddress(const boost::asio::ip::address& address) {
      Address = address;
   }
   inline void setTrafficClass(const uint8_t trafficClass) {
      TrafficClass = trafficClass;
   }
   inline void setIdentifier(const uint32_t identifier) {
      Identifier = identifier;
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
