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


class OutputStream : public boost::iostreams::filtering_ostream
{

   public:
   OutputStream();
   ~OutputStream();

   bool openStream(std::ostream& os);
   bool openStream(const std::filesystem::path& fileName,
                   const FileCompressorType     compressor = FromExtension);
   bool closeStream(const bool sync = true);

   private:
   std::filesystem::path                                                    FileName;
   std::filesystem::path                                                    TmpFileName;
   boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_sink>* StreamBuffer;
   FileCompressorType                                                       Compressor;
};


// ###### Constructor #######################################################
OutputStream::OutputStream()
{
   StreamBuffer = nullptr;
   Compressor   = None;
}


// ###### Destructor ########################################################
OutputStream::~OutputStream()
{
   closeStream(false);
}


// ###### Initialise output stream to std::ostream ##########################
bool OutputStream::openStream(std::ostream& os)
{
   closeStream(false);
   push(os);
   return true;
}


// ###### Initialise output stream to output file ###########################
bool OutputStream::openStream(const std::filesystem::path& fileName,
                              const FileCompressorType     compressor)
{
   // ====== Reset ==========================================================
   closeStream();

   // ====== Initialise output steam to file ================================
   Compressor = compressor;
   FileName   = fileName;
   if(FileName != std::filesystem::path()) {
      TmpFileName = FileName.string() + ".tmp";

      // ------ Remove output file, if it is existing -----------------------
      std::filesystem::remove(FileName);

      // ------ Open temporary output file ----------------------------------
      int handle = ::open(TmpFileName.c_str(),
                          O_CREAT|O_TRUNC|O_WRONLY,
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if(handle < 0) {
         throw std::runtime_error(std::string(strerror(errno)));
      }
#ifdef POSIX_FADV_SEQUENTIAL
      posix_fadvise(handle, 0, 0, POSIX_FADV_SEQUENTIAL|POSIX_FADV_NOREUSE);
#endif
      StreamBuffer = new boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_sink>(
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
            push(boost::iostreams::lzma_compressor(params));
           }
          break;
         case BZip2:
            push(boost::iostreams::bzip2_compressor());
          break;
         case GZip:
            push(boost::iostreams::gzip_compressor());
          break;
         case ZSTD:
            push(boost::iostreams::zstd_compressor());
          break;
         case ZLIB:
            push(boost::iostreams::zlib_compressor());
          break;
         default:
          break;
      }
      push(*StreamBuffer);

      return true;
   }
   return false;
}


// ###### Close output stream ###############################################
bool OutputStream::closeStream(const bool sync)
{
   // ====== Synchronise and close file =====================================
   bool success = false;
   if(sync) {
      if(FileName != std::filesystem::path()) {
         // ------ Synchronise, then rename temporary output file --------------
         boost::iostreams::filtering_ostream::strict_sync();
         success = good();
         if(success) {
            std::filesystem::rename(TmpFileName, FileName);
            success = true;
         }
         else {
            std::filesystem::remove(TmpFileName);
         }
      }
   }
   reset();

   // ====== Clean up =======================================================
   if(StreamBuffer) {
      delete StreamBuffer;
      StreamBuffer = nullptr;
   }
   FileName    = std::filesystem::path();
   TmpFileName = std::filesystem::path();

   return success;
}



void test(const char* name)
{
   OutputStream of;
   try {
      of.openStream(name);
      for(unsigned int i = 0; i < 1000; i++) {
         of << "Test! " << name << "\n";
      }
      of.flush();
      if(!of.closeStream()) {
         std::cerr << "SYNC FAILED " << name << "\n";
      }
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: " << e.what() << "\n";
   }
   std::cerr << "OK " << name << "\n";
}

int main(int argc, char** argv)
{
   OutputStream of;

   of.openStream(std::cout);
   of << "COUT-TEST\n";
   of.closeStream();

   test("test.txt");
   test("test.txt.gz");
   test("test.txt.bz2");
   test("test.txt.xz");
   test("test.txt.zst");
   test("test.txt.zz");
   return 0;
}
