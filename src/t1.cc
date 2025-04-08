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
#include <boost/program_options.hpp>


enum FileCompressorType {
   FromExtension = 0,
   None          = 1,
   GZip          = 2,
   BZip2         = 3,
   XZ            = 4
};


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
   std::filesystem::path                                                      FileName;
   std::filesystem::path                                                      TmpFileName;
   boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>* StreamBuffer;
   std::ostream*                                                              OutputFileStream;
   FileCompressorType                                                         Compressor;
};


// ###### Constructor #######################################################
OutputStream::OutputStream()
{
   StreamBuffer     = nullptr;
   OutputFileStream = nullptr;
   Compressor       = None;
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
      std::cout << "tmp=" << TmpFileName << "\n";
      int handle = ::open(TmpFileName.c_str(),
                          O_CREAT|O_TRUNC|O_WRONLY,
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if(handle < 0) {
         throw std::runtime_error(std::string(strerror(errno)));
      }
#ifdef POSIX_FADV_SEQUENTIAL
      posix_fadvise(handle, 0, 0, POSIX_FADV_SEQUENTIAL|POSIX_FADV_NOREUSE);
#endif
      ::write(handle, "TEst\n",5);

      StreamBuffer = new boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>(
                        handle, boost::iostreams::file_descriptor_flags::close_handle);
      assert(StreamBuffer != nullptr);
      OutputFileStream = new std::ostream(StreamBuffer);

      // ------ Configure the compressor ------------------------------------
      if(Compressor == FromExtension) {
         std::string extension(FileName.extension());
         boost::algorithm::to_lower(extension);
         if(extension == ".xz") {
            Compressor = XZ;
         }
         else if(extension == ".bz2") {
            Compressor = BZip2;
         }
         else if(extension == ".gz") {
            Compressor = GZip;
         }
         else {
            Compressor = None;
         }
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
         default:
          break;
      }
      push(*OutputFileStream);

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
   puts("CLOSE!");
         // ------ Synchronise, then rename temporary output file --------------
         puts("a-1");
         boost::iostreams::filtering_ostream::strict_sync();
         success = good();
         puts("a-2");
         if(success) {
            puts("a-4");
            // std::filesystem::rename(TmpFileName, FileName);
            success = true;
         }
         else {
            std::error_code ec;
            // std::filesystem::remove(TmpFileName, ec);
         }
      }
   }
   // if(!empty()) {
   //    pop();
   // }
   // pop();
   // pop();
   reset();

   // ====== Clean up =======================================================
   if(OutputFileStream) {
      delete OutputFileStream;
      OutputFileStream = nullptr;
   }
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
   // test("test.txt.gz");
   // test("test.txt.bz2");
   // test("test.txt.xz");
   return 0;
}
