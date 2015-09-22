// NorNet Trace Service
// Copyright (C) 2015 by Thomas Dreibholz
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <set>
#include <iostream>
#include <boost/asio.hpp>


std::set<boost::asio::ip::address> sourceArray;
std::set<boost::asio::ip::address> destinationArray;


void addAddress(std::set<boost::asio::ip::address>& array,
                const std::string&                  addressString)
{
   boost::system::error_code errorCode;
   boost::asio::ip::address address = boost::asio::ip::address::from_string(addressString, errorCode);
   if(errorCode != boost::system::errc::success) {
      std::cerr << "ERROR: Bad address " << addressString << "!" << std::endl;
      ::exit(1);
   }
   array.insert(address);
}



int main(int argc, char** argv)
{
   for(int i = 1; i < argc; i++) {
      if(strncmp(argv[i], "-source=", 8) == 0) {
         addAddress(sourceArray, (const char*)&argv[i][8]);
      }
      else if(strncmp(argv[i], "-destination=", 13) == 0) {
         addAddress(destinationArray, (const char*)&argv[i][13]);
      }
      else {
         std::cerr << "Usage: " << argv[0] << " -source=source ... -destination=destination ..." << std::endl;
      }
   }
}
