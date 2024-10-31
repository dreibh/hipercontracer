#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>

#include "tools.h"


using namespace std::chrono_literals;

typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemDuration;

typedef std::chrono::file_clock              FileClock;
typedef FileClock::time_point                FileTimePoint;
typedef FileClock::duration                  FileDuration;



class GarbageDirectoryCollector
{
   public:
   GarbageDirectoryCollector();
   ~GarbageDirectoryCollector();

   void addDirectory(const std::filesystem::path directory);
   void removeDirectory(const std::filesystem::path directory);
   void performGarbageCollection(const SystemDuration& maxAge);

   private:
   std::map<const std::filesystem::path, SystemTimePoint> DirectoryMap;
};


GarbageDirectoryCollector::GarbageDirectoryCollector()
{

}

GarbageDirectoryCollector::~GarbageDirectoryCollector()
{

}

void GarbageDirectoryCollector::addDirectory(const std::filesystem::path directory)
{
   std::error_code ec;
   const FileTimePoint lastWriteFileTime = std::filesystem::last_write_time(directory, ec);
   if(!ec) {
      std::cout << std::format("File write time is {}\n", lastWriteFileTime);
      const SystemTimePoint lastWriteSystemTime = FileClock::to_sys(lastWriteFileTime);
      std::map<const std::filesystem::path, SystemTimePoint>::iterator found =
         DirectoryMap.find(directory);
      if(found != DirectoryMap.end()) {
         found->second = lastWriteSystemTime;
      }
      else {
         std::cout << "UPDATE\n";
         DirectoryMap.insert(std::pair<const std::filesystem::path, SystemTimePoint>(
                                directory, lastWriteSystemTime));
      }
   }
}


void GarbageDirectoryCollector::removeDirectory(const std::filesystem::path directory)
{
   std::map<const std::filesystem::path, SystemTimePoint>::iterator found =
      DirectoryMap.find(directory);
   if(found != DirectoryMap.end()) {
      DirectoryMap.erase(found);
   }
}


void GarbageDirectoryCollector::performGarbageCollection(const SystemDuration& maxAge)
{
   const SystemTimePoint threshold = SystemClock::now() - maxAge;
   const size_t          n1        = DirectoryMap.size();
   std::cout << "Starting garbage directory clean-up of directories older than "
             << timePointToString<SystemTimePoint>(threshold) << "\n";

   std::map<const std::filesystem::path, SystemTimePoint>::iterator iterator =
      DirectoryMap.begin();
   while(iterator != DirectoryMap.end()) {
      if(iterator->second < threshold) {
         const std::filesystem::path& directory = iterator->first;

         // ====== Update last write time ===================================
         std::error_code ec;
         const FileTimePoint lastWriteFileTime =
            std::filesystem::last_write_time(directory, ec);
         if(!ec) {
            const SystemTimePoint lastWriteSystemTime = FileClock::to_sys(lastWriteFileTime);
            iterator->second = lastWriteSystemTime;
            puts("U!");
         }
         // ====== Out of date -> remove directory ==========================
         if(iterator->second <= threshold) {
             std::filesystem::remove(directory, ec);
             if(!ec) {
                 std::cout << "Deleted empty directory " << directory << "\n";
                                 // << relativeTo(dirEntry.path(), ImporterConfig.getImportFilePath());
             }
             iterator = DirectoryMap.erase(iterator);
             continue;
         }
      }
      iterator++;
   }

   const size_t n2 = DirectoryMap.size();
   std::cout << "Clean-up finished: " << n1 << " -> " << n2 << "\n";
}


int main()
{
    GarbageDirectoryCollector gdc;

    gdc.addDirectory("/tmp/test0");
    gdc.addDirectory("/tmp/test0");
    gdc.addDirectory("/tmp/test1");

    gdc.performGarbageCollection(10s);

    gdc.removeDirectory("/tmp/test0");
    gdc.removeDirectory("/tmp/test0");


    // auto p = std::filesystem::temp_directory_path() / "test0";
    //
    // std::filesystem::file_time_type ftime = std::filesystem::last_write_time(p);
    // std::cout << std::format("File write time is {}\n", ftime);
 }
