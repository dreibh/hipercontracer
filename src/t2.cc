#include <fcntl.h>

#include <filesystem>
#include <iostream>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/program_options.hpp>


enum FileCompressorType {
   FromExtension = 0,
   None          = 1,
   GZip          = 2,
   BZip2         = 3,
   XZ            = 4,
   ZSTD          = 5,
   ZLIB          = 6
};


// ###### Obtain compressor from file name extension ########################
FileCompressorType obtainCompressorFromExtension(const std::filesystem::path& fileName)
{
   FileCompressorType compressor;
   std::string extension(fileName.extension());
   boost::algorithm::to_lower(extension);
   if(extension == ".xz") {
      compressor = XZ;
   }
   else if(extension == ".bz2") {
      compressor = BZip2;
   }
   else if(extension == ".gz") {
      compressor = GZip;
   }
   else if(extension == ".zst") {
      compressor = ZSTD;
   }
   else if(extension == ".zz") {
      compressor = ZLIB;
   }
   else {
      compressor = None;
   }
   return compressor;
}


class InputStream : public boost::iostreams::filtering_istream
{

   public:
   InputStream();
   ~InputStream();

   bool openStream(std::istream& os);
   bool openStream(const std::filesystem::path& fileName,
                   const FileCompressorType     compressor = FromExtension);
   void closeStream();

   private:
   std::filesystem::path                                                      FileName;
   boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>* StreamBuffer;
   FileCompressorType                                                         Compressor;
};


// ###### Constructor #######################################################
InputStream::InputStream()
{
   StreamBuffer = nullptr;
   Compressor   = None;
}


// ###### Destructor ########################################################
InputStream::~InputStream()
{
   closeStream();
}


// ###### Initialise input stream to std::istream ###########################
bool InputStream::openStream(std::istream& os)
{
   closeStream();
   push(os);
   return true;
}


// ###### Initialise input stream to input file #############################
bool InputStream::openStream(const std::filesystem::path& fileName,
                              const FileCompressorType     compressor)
{
   // ====== Reset ==========================================================
   closeStream();

   // ====== Initialise input steam to file =================================
   Compressor = compressor;
   FileName   = fileName;
   if(FileName != std::filesystem::path()) {
      // ------ Open temporary input file -----------------------------------
      int handle = ::open(FileName.c_str(), O_RDONLY);
      if(handle < 0) {
         throw std::runtime_error(std::string(strerror(errno)));
      }
#ifdef POSIX_FADV_SEQUENTIAL
      posix_fadvise(handle, 0, 0, POSIX_FADV_SEQUENTIAL|POSIX_FADV_WILLNEED|POSIX_FADV_NOREUSE);
#endif
      StreamBuffer = new boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>(
                        handle, boost::iostreams::file_descriptor_flags::close_handle);
      assert(StreamBuffer != nullptr);

      // ------ Configure the compressor ------------------------------------
      if(Compressor == FromExtension) {
         Compressor = obtainCompressorFromExtension(FileName);
      }
      switch(Compressor) {
         case XZ: {
            const boost::iostreams::lzma_params params(
               boost::iostreams::lzma::default_compression,
               std::thread::hardware_concurrency());
            push(boost::iostreams::lzma_decompressor(params));
           }
          break;
         case BZip2:
            push(boost::iostreams::bzip2_decompressor());
          break;
         case GZip:
            push(boost::iostreams::gzip_decompressor());
          break;
         case ZSTD:
            push(boost::iostreams::zstd_decompressor());
          break;
         case ZLIB:
            push(boost::iostreams::zlib_decompressor());
          break;
         default:
          break;
      }
      push(*StreamBuffer);

      return true;
   }
   return false;
}


// ###### Close input stream ###############################################
void InputStream::closeStream()
{
   reset();

   // ====== Clean up =======================================================
   if(StreamBuffer) {
      delete StreamBuffer;
      StreamBuffer = nullptr;
   }
   FileName = std::filesystem::path();
}



void test(const char* name)
{
   unsigned int n = 0;
   InputStream is;
   try {
      is.openStream(name);
      std::string line;
      while(std::getline(is, line)) {
         n++;
      }
      is.closeStream();
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: " << e.what() << "\n";
   }
   std::cerr << "OK " << name << "\t" << n << "\n";
}


int main(int argc, char** argv)
{
   // InputStream is;
   //
   // is.openStream(std::cout);
   // is << "COUT-TEST\n";
   // is.closeStream();

   test("test.txt");
   test("test.txt.gz");
   test("test.txt.bz2");
   test("test.txt.xz");
   test("test.txt.zst");
   test("test.txt.zz");
   return 0;
}
