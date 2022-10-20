#include <filesystem>
#include <iostream>
#include <regex>
#include <set>

// g++ t5.cc -o t5 -std=c++17


class AbstractReader
{
   public:
   virtual const std::regex& getFileNameRegExp() const = 0;
   virtual void addFile(const std::filesystem::path& dataFile,
                        const std::smatch            match) = 0;
};


class HiPerConTracerPingReader : public virtual AbstractReader
{
   public:
   HiPerConTracerPingReader();
   ~HiPerConTracerPingReader();

   virtual const std::regex& getFileNameRegExp() const;
   virtual void addFile(const std::filesystem::path& dataFile,
                        const std::smatch            match);

   private:
   struct InputFileEntry {
      std::string           Source;
      std::string           TimeStamp;
      unsigned int          SeqNumber;
      std::filesystem::path DataFile;

      inline bool operator<(const InputFileEntry& cmp) {
         if(Source < cmp.Source) {
            return true;
         }
         if(TimeStamp < cmp.TimeStamp) {
            return true;
         }
         if(SeqNumber < cmp.SeqNumber) {
            return true;
         }
         return false;
      }
   };
   friend bool operator<(const HiPerConTracerPingReader::InputFileEntry& a,
                         const HiPerConTracerPingReader::InputFileEntry& b);

   std::set<InputFileEntry> inputFileSet;
   static const std::regex  PingFileNameRegExp;
};


const std::regex HiPerConTracerPingReader::PingFileNameRegExp = std::regex(
   // Format: Ping-<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Ping-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


bool operator<(const HiPerConTracerPingReader::InputFileEntry& a,
               const HiPerConTracerPingReader::InputFileEntry& b) {
   if(a.Source < b.Source) {
      return true;
   }
   if(a.TimeStamp < b.TimeStamp) {
      return true;
   }
   if(a.SeqNumber < b.SeqNumber) {
      return true;
   }
   return false;
}


HiPerConTracerPingReader::HiPerConTracerPingReader()
{
}


HiPerConTracerPingReader::~HiPerConTracerPingReader()
{
}


const std::regex& HiPerConTracerPingReader::getFileNameRegExp() const
{
   return(PingFileNameRegExp);
}


void HiPerConTracerPingReader::addFile(const std::filesystem::path& dataFile,
                                       const std::smatch            match)
{
   std::cout << dataFile << " s=" << match.size() << std::endl;
   if(match.size() == 5) {
      InputFileEntry inputFileEntry;
      inputFileEntry.Source    = match[2];
      inputFileEntry.TimeStamp = match[3];
      inputFileEntry.SeqNumber = atol(match[4].str().c_str());

      std::cout << "  s=" << inputFileEntry.Source << std::endl;
      std::cout << "  t=" << inputFileEntry.TimeStamp << std::endl;
      std::cout << "  q=" << inputFileEntry.SeqNumber << std::endl;
      inputFileEntry.DataFile  = dataFile;

      inputFileSet.insert(inputFileEntry);
   }
}



class HiPerConTracerTracerouteReader : public virtual AbstractReader,
                                       public HiPerConTracerPingReader
{
   public:
   HiPerConTracerTracerouteReader();
   ~HiPerConTracerTracerouteReader();

   virtual const std::regex& getFileNameRegExp() const;

   private:
   static const std::regex TracerouteFileNameRegExp;
};


const std::regex HiPerConTracerTracerouteReader::TracerouteFileNameRegExp = std::regex(
   // Format: Traceroute-<ProcessID>-<Source>-<YYYYMMDD>T<Seconds.Microseconds>-<Sequence>.results.bz2
   "^Traceroute-P([0-9]+)-([0-9a-f:\\.]+)-([0-9]{8}T[0-9]+\\.[0-9]{6})-([0-9]*)\\.results.*$"
);


HiPerConTracerTracerouteReader::HiPerConTracerTracerouteReader()
{
}


HiPerConTracerTracerouteReader::~HiPerConTracerTracerouteReader()
{
}


const std::regex& HiPerConTracerTracerouteReader::getFileNameRegExp() const
{
   return(TracerouteFileNameRegExp);
}



class Collector
{
   public:
   Collector(const std::filesystem::path& dataDirectory,
             const unsigned int           maxDepth = 5);
   ~Collector();

   void addReader(AbstractReader* reader);
   void lookForFiles();

   private:
   void lookForFiles(const std::filesystem::path& dataDirectory,
                     const unsigned int           maxDepth);
   void addFile(const std::filesystem::path& dataFile);

   std::set<AbstractReader*>   ReaderSet;
   const std::filesystem::path DataDirectory;
   const unsigned int          MaxDepth;
};


Collector::Collector(const std::filesystem::path& dataDirectory,
                     const unsigned int           maxDepth)
 : DataDirectory(dataDirectory),
   MaxDepth(maxDepth)
{
}


Collector::~Collector()
{
}


void Collector::addReader(AbstractReader* reader)
{
   ReaderSet.insert(reader);
}


void Collector::lookForFiles()
{
   lookForFiles(DataDirectory, MaxDepth);
}


void Collector::lookForFiles(const std::filesystem::path& dataDirectory,
                             const unsigned int           maxDepth)
{
   for(const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(dataDirectory)) {
      if(dirEntry.is_regular_file()) {
         addFile(dirEntry.path());
      }
      else if(dirEntry.is_directory()) {
         if(maxDepth > 1) {
            lookForFiles(dirEntry, maxDepth - 1);
         }
      }
   }
}


void Collector::addFile(const std::filesystem::path& dataFile)
{
   // const std::filesystem::path& subdir = std::filesystem::relative(dataFile, DataDirectory).parent_path();
   // const std::string& filename = dataFile.filename();

   std::smatch        match;
   const std::string& filename = dataFile.filename().string();
   for(AbstractReader* reader : ReaderSet) {
      if(std::regex_match(filename, match, reader->getFileNameRegExp())) {
         reader->addFile(dataFile, match);
      }
   }
}



int main(int argc, char** argv)
{
   Collector collector(".", 5);

   HiPerConTracerPingReader pingReader;
   collector.addReader(&pingReader);
   HiPerConTracerTracerouteReader tracerouteReader;
   collector.addReader(&tracerouteReader);
   collector.lookForFiles();
}
