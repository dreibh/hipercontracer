#include "sqlwriter.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <boost/format.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#if BOOST_VERSION >= 106600
#include <boost/uuid/detail/sha1.hpp>
#else
#include <boost/uuid/sha1.hpp>
#endif


int main(int argc, char** argv)
{
    std::string pathString;

//    pathString = "172.16.0.240-172.16.0.1-77.88.125.170-81.175.33.17-81.175.32.241-81.175.32.226-62.115.12.37-62.115.139.200-80.91.253.227-62.115.61.30-74.125.37.237-72.14.236.133-8.8.8.8";
//    pathString = "fd00:16:1::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:5::2-fd00:16:1::1-fd00:16:1::1";
//    pathString = "172.16.1.2-172.16.1.1-172.17.2.2";
   pathString = "fd00:17:1::2-fd00:17:1::1-fd00:17:2::2";

   boost::uuids::detail::sha1 sha1Hash;
   sha1Hash.process_bytes(pathString.c_str(), pathString.length());
   uint32_t digest[5];
   sha1Hash.get_digest(digest);

   const uint64_t a = (uint64_t)digest[0];
   const uint64_t b = (uint64_t)digest[1];
   const uint64_t newHash = (a << 32) | b;

   printf("%s\n", pathString.c_str());
   printf("%llx %llx %llx %lld\n",
          (long long int)newHash,
          (long long int)a, (long long int)b, (long long int)newHash);
}
