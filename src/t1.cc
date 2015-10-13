#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <fstream>
#include <set>
#include <algorithm>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>


class SQLWriter
{
   public:
   SQLWriter(const std::string& directory,
             const std::string& uniqueID,
             const std::string& tableName);
   virtual ~SQLWriter();

   bool prepare();
   bool changeFile(const bool createNewFile = true);
   void insert(const std::string& tuple);

   protected:
   const boost::filesystem::path Directory;
   const std::string             UniqueID;
   const std::string             TableName;
   boost::filesystem::path       TempFileName;
   boost::filesystem::path       TargetFileName;
   std::ofstream                 OutputFile;
   size_t                        Inserts;
   unsigned long long            SeqNumber;
};


SQLWriter::SQLWriter(const std::string& directory,
                     const std::string& uniqueID,
                     const std::string& tableName)
   : Directory(directory),
     UniqueID(uniqueID),
     TableName(tableName)
{
   Inserts   = 0;
   SeqNumber = 0;
}


SQLWriter::~SQLWriter()
{
   changeFile(false);
}


bool SQLWriter::prepare()
{
   try {
      boost::filesystem::create_directory(Directory);
      boost::filesystem::create_directory(Directory / "tmp");
   }
   catch(std::exception const& e) {
      std::cerr << "ERROR: Unable to prepare directories - " << e.what() << std::endl;
      return(false);
   }
   return(changeFile());
}


bool SQLWriter::changeFile(const bool createNewFile)
{
   if(OutputFile.is_open()) {
      OutputFile << "COMMIT TRANSACTION" << std::endl;
      OutputFile.close();
      try {
         if(Inserts == 0) {
            // empty file -> just remove it!
            boost::filesystem::remove(TempFileName);
         }
         else {
            // file has contents -> move it!
            boost::filesystem::rename(TempFileName, TargetFileName);
         }
      }
      catch(std::exception const& e) {
         std::cerr << "ERROR: SQLWriter::changeFile() - " << e.what() << std::endl;
      }
   }
   else puts("nothing");

   Inserts = 0;
   SeqNumber++;
   if(createNewFile) {
      try {
         const std::string name = UniqueID + str(boost::format("-%09d.sql") % SeqNumber);
         TempFileName   = Directory / "tmp" / name;
         TargetFileName = Directory / name;
         std::cout << "t=" << TempFileName << std::endl;


         OutputFile.open(TempFileName.c_str());
         OutputFile << "START TRANSACTION" << std::endl;
         return(OutputFile.good());
      }
      catch(std::exception const& e) {
         std::cerr << "ERROR: SQLWriter::changeFile() - " << e.what() << std::endl;
         return(false);
      }
   }
   return(true);
}


void SQLWriter::insert(const std::string& tuple)
{
   OutputFile << "INSERT INTO " << TableName << " VALUES ("
              << tuple
              << ");" << std::endl;
   Inserts++;
   printf("i=%d\n",(int)Inserts);
}


int main(int argc, char** argv)
{
   std::string sqlDirectory = "/storage/xy";

   // ====== Initialize =====================================================
   SQLWriter w(sqlDirectory, "UniqueID", "TracerouteTable");
   if(!w.prepare()) {
      return 1;
   }

   w.insert("1,2,3,4");
   w.changeFile();
   w.insert("5,6,7,8");
   w.changeFile();
   w.insert("8,8,8,8");
   w.changeFile();
}
