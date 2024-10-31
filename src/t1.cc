#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <boost/filesystem/operations.hpp>

#include "tools.h"


using namespace std::chrono_literals;

typedef std::chrono::system_clock            SystemClock;
typedef std::chrono::time_point<SystemClock> SystemTimePoint;
typedef SystemClock::duration                SystemDuration;

// typedef std::chrono::file_clock              FileClock;
// typedef FileClock::time_point                FileTimePoint;
// typedef FileClock::duration                  FileDuration;



class GarbageDirectoryCollector
{
   public:
   GarbageDirectoryCollector();
   ~GarbageDirectoryCollector();

   void addGarbageDirectory(const std::filesystem::path directory);
   void removeGarbageDirectory(const std::filesystem::path directory);
   void performGarbageDirectoryCleanUp(const SystemDuration& maxAge);

   private:
   std::map<const std::filesystem::path, SystemTimePoint> GarbageDirectoryMap;
};


GarbageDirectoryCollector::GarbageDirectoryCollector()
{

}

GarbageDirectoryCollector::~GarbageDirectoryCollector()
{

}


bool getLastWriteTimePoint(const std::filesystem::path path,
                           SystemTimePoint&            lastWriteTimePoint)
{
    try {
       const time_t lastWriteTimeT = boost::filesystem::last_write_time(boost::filesystem::path(path));
       lastWriteTimePoint = SystemClock::from_time_t(lastWriteTimeT);
       return true;
    }
    catch(...) { }
    return false;
}



void GarbageDirectoryCollector::addGarbageDirectory(const std::filesystem::path directory)
{
   SystemTimePoint lastWriteTimePoint;
   if(getLastWriteTimePoint(directory, lastWriteTimePoint)) {
      std::map<const std::filesystem::path, SystemTimePoint>::iterator found =
         GarbageDirectoryMap.find(directory);
      if(found != GarbageDirectoryMap.end()) {
         found->second = lastWriteTimePoint;
      }
      else {
         GarbageDirectoryMap.insert(std::pair<const std::filesystem::path, SystemTimePoint>(
                                directory, lastWriteTimePoint));
      }
   }
}


void GarbageDirectoryCollector::removeGarbageDirectory(const std::filesystem::path directory)
{
   std::map<const std::filesystem::path, SystemTimePoint>::iterator found =
      GarbageDirectoryMap.find(directory);
   if(found != GarbageDirectoryMap.end()) {
      GarbageDirectoryMap.erase(found);
   }
}


void GarbageDirectoryCollector::performGarbageDirectoryCleanUp(const SystemDuration& maxAge)
{
   const SystemTimePoint threshold = SystemClock::now() - maxAge;
   const size_t          n1        = GarbageDirectoryMap.size();
   std::cout << "Starting garbage directory clean-up of directories older than "
                   << timePointToString<SystemTimePoint>(threshold)<< "\n";

   std::map<const std::filesystem::path, SystemTimePoint>::iterator iterator =
      GarbageDirectoryMap.begin();
   while(iterator != GarbageDirectoryMap.end()) {
      if(iterator->second < threshold) {
         const std::filesystem::path& directory = iterator->first;

         // ====== Update last write time ===================================
         SystemTimePoint lastWriteTimePoint;
         if(getLastWriteTimePoint(directory, lastWriteTimePoint)) {
            iterator->second = lastWriteTimePoint;
         }
         // ====== Out of date -> remove directory ==========================
         if(iterator->second <= threshold) {
             std::error_code ec;
             std::filesystem::remove(directory, ec);
             if(!ec) {
                 std::cout << "Deleted empty directory "  << directory << "\n";
                                 // << relativeTo(directory, ImporterConfig.getImportFilePath());
             }
             iterator = GarbageDirectoryMap.erase(iterator);
             continue;
         }
      }
      iterator++;
   }

   const size_t n2 = GarbageDirectoryMap.size();
   std::cout << "Finished garbage directory clean-up: " << n1 << " -> " << n2 << "\n";
}


int main()
{
    GarbageDirectoryCollector gdc;

    gdc.addGarbageDirectory("/tmp/test0");
    gdc.addGarbageDirectory("/tmp/test0");
    gdc.addGarbageDirectory("/tmp/test1");

    gdc.performGarbageDirectoryCleanUp(10s);

    gdc.removeGarbageDirectory("/tmp/test0");
    gdc.removeGarbageDirectory("/tmp/test0");


    // auto p = std::filesystem::temp_directory_path() / "test0";
    //
    // std::filesystem::file_time_type ftime = std::filesystem::last_write_time(p);
    // std::cout << std::format("File write time is {}\n", ftime);
 }
