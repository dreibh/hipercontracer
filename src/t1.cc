#include <fcntl.h>
#include <sys/stat.h>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filter/zstd.hpp>


int main(int argc, char** argv)
{
   int handle = ::open("test.txt.zst", O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
   if(handle < 0) {
      perror("open()");
      return 1;
   }
   printf("H=%d\n", handle);
   boost::iostreams::filtering_ostream os;
   boost::iostreams::file_descriptor_sink sink(handle, boost::iostreams::file_descriptor_flags::close_handle);
   os.push(boost::iostreams::zstd_compressor());
   os.push(sink);

   os << "This is a test!\n";
   os.flush();
   os.strict_sync();
   return 0;
}
