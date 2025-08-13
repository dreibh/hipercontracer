#include <iostream>
#include <filesystem>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>


// ###### Set file modification time ########################################
// NOTE:
// * std::filesystem::last_write_time() is unusable in C++17, and C++20 is
//   too new (not even in Ubuntu 24.04, etc.).
// * boost::filesystem::last_write_time() has issues with Debian 13 on ARMv7.
// => Using POSIX function in ANSI C instead:
bool set_last_write_time(const std::filesystem::path& path,
                         const unsigned long long     newTime)
{
   timespec ts[2];
   // Access time:
   ts[0].tv_sec  = 0;
   ts[0].tv_nsec = UTIME_OMIT;   // Leave unchanged
   // Modification time:
   ts[1].tv_sec  = newTime / 1000000000ULL;
   ts[1].tv_nsec = newTime % 1000000000ULL;
   return (utimensat(AT_FDCWD, path.c_str(), (const timespec*)&ts, 0) == 0);
}


// Test:
// touch test.txt && make t3 && ./t3 && ls -l test.txt
int main(int argc, char** argv)
{
   struct stat file_info;
   if(stat("test.txt", &file_info) == -1) {
      perror("Error getting file status");
      return 1;
   }
   printf("C=%lu\n", (long)file_info.st_ctime);
   printf("M=%lu\n", (long)file_info.st_mtime);
   printf("A=%lu\n", (long)file_info.st_atime);

   unsigned long long t = 212847000 * 1000000000ULL + 123456888;
   if(!set_last_write_time(std::filesystem::path("test.txt"), t)) {
      perror("Error setting file status");
      return 1;
   }

   // NOTE: *** LINUX ONLY! ***
   // struct statx file_infox;
   // if(statx(AT_FDCWD, "test.txt", 0, STATX_CTIME|STATX_MTIME|STATX_ATIME, &file_infox) == -1) {
   //    perror("Error getting file status");
   //    return 1;
   // }
   // printf("C=%llu.%09u\n", file_infox.stx_ctime.tv_sec, file_infox.stx_ctime.tv_nsec);
   // printf("A=%llu.%09u\n", file_infox.stx_mtime.tv_sec, file_infox.stx_mtime.tv_nsec);
   // printf("M=%llu.%09u\n", file_infox.stx_atime.tv_sec, file_infox.stx_atime.tv_nsec);

   if(stat("test.txt", &file_info) == -1) {
      perror("Error getting file status");
      return 1;
   }
   printf("C=%lu\n", (long)file_info.st_ctime);
   printf("M=%lu\n", (long)file_info.st_mtime);
   printf("A=%lu\n", (long)file_info.st_atime);

   return 0;
}
