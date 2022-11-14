#include <string>
#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>

#include <boost/asio.hpp>

#include <ctype.h>


// boost::multiprecision::cpp_int addressToPacked(const boost::asio::ip::address& address)
// {
//    boost::multiprecision::cpp_int packed;
//    if(address.is_v4()) {
//       packed = address.to_v4().to_ulong();
//    }
//    else {
//       packed = 0; //address.to_v6().to_ulong();
//    }
//    return packed;
// }


// ###### Convert IP address into bytes string format #######################
std::string addressToBytesString(const boost::asio::ip::address& address)
{
   std::stringstream byteString;
   if(address.is_v4()) {
      const boost::asio::ip::address_v4::bytes_type b = address.to_v4().to_bytes();
      for(unsigned int i = 0; i < 4; i++) {
         byteString << "\\x" << std::hex << (unsigned int)b[i];
      }
   }
   else {
      const boost::asio::ip::address_v6::bytes_type b = address.to_v6().to_bytes();
      for(unsigned int i = 0; i < 16; i++) {
         byteString << "\\x" << std::hex << (unsigned int)b[i];
      }
   }
   return byteString.str();
}


int main() {
   const char* str[] = {
      "8.8.8.8",
      "127.0.0.1",
      "193.99.144.80",
      "224.244.244.224",
      "2400:cb00:2048:1::6814:155",
      "::1",
      "3ffe::1:2:3:4",
      "2001:700:4100:101::1"
   };
   for(unsigned int i = 0;i < 8; i++) {
      const boost::asio::ip::address a = boost::asio::ip::address::from_string(str[i]);
      std::cout << "a" << i + 1 << ": " << str[i] << "=" << a << " -> " << addressToBytesString(a) << "\n";
   }
   return 0;
}
