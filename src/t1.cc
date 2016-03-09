#include "sqlwriter.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <boost/format.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/uuid/sha1.hpp>


int main(int argc, char** argv)
{
   std::string pathString = "172.16.0.240-172.16.0.1-77.88.125.170-81.175.33.17-81.175.32.241-81.175.32.226-62.115.12.37-62.115.139.200-80.91.253.227-62.115.61.30-74.125.37.237-72.14.236.133-8.8.8.8";

   boost::uuids::detail::sha1 sha1Hash;
   sha1Hash.process_bytes(pathString.c_str(), pathString.length());
   uint32_t digest[5];
   sha1Hash.get_digest(digest);
   const uint64_t newHash = ((uint64_t)digest[0] << 32) | (uint64_t)digest[1];
/*
a = newHash[0:8]
b = newHash[8:16]
c = (int(a, 16) << 32) | int(b, 16)*/

   const uint64_t a = (uint64_t)digest[0];
   const uint64_t b = (uint64_t)digest[1];
   const uint64_t c = (a << 32) | b;

   printf("%s\n", pathString.c_str());
   printf("%llx %llx %llx %llx\n",
          (long long int)newHash,
          (long long int)a, (long long int)b, (long long int)c);
}
