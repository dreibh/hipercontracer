#include <filesystem>
#include <fstream>
#include <iostream>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/stream.hpp>


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

   bool open(const std::filesystem::path& fileName,
             const FileCompressorType     compressor = FromExtension);
   bool close(const bool sync = true);

   private:
   std::filesystem::path                                                      FileName;
   std::filesystem::path                                                      TmpFileName;
   boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source>* StreamBuffer;
};


OutputStream::OutputStream()
{
   StreamBuffer = nullptr;
}


OutputStream::~OutputStream()
{
   close(false);
}


bool OutputStream::open(const std::filesystem::path& fileName,
                        const FileCompressorType     compressor)
{
   // ====== Reset ==========================================================
   close();

   // ======
   if(fileName != std::filesystem::path()) {
      FileName    = fileName;
      TmpFileName = fileName + ".tmp";

   }
}


bool OutputStream::close(const bool sync)
{
   bool success;
   if(sync) {
      bool success = outputStream.sync();
      if(success) {
         std::filesystem::rename(TmpFileName, TmpFileName);
      }
   }
   else {
      success = false;
   }
   outputStream.reset();
   if(StreamBuffer) {
      delete StreamBuffer;
      StreamBuffer = nullptr;
   }
   FileName    = std::filesystem::path();
   TmpFileName = std::filesystem::path();
   return success;
}


int main(int argc, char** argv)
{
   std::ofstream of("test.txt");
   // int fh = of.native_handle();
   of << "Test!\n";
   return 0;
}


