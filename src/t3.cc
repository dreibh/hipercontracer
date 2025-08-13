#include <iostream>
#include <filesystem>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>


// ###### Set file modification time ########################################
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
   struct statx file_info;
   if(statx(AT_FDCWD, "test.txt", 0, STATX_CTIME|STATX_MTIME|STATX_ATIME, &file_info) == -1) {
      perror("Error getting file status");
      return 1;
   }
   printf("C=%lld\n", file_info.stx_ctime.tv_sec);
   printf("A=%lld\n", file_info.stx_atime.tv_sec);
   printf("M=%lld\n", file_info.stx_mtime.tv_sec);

   unsigned long long t = 212847000 * 1000000000ULL + 1234560022;
   if(!set_last_write_time(std::filesystem::path("test.txt"), t)) {
      perror("Error setting file status");
      return 1;
   }

   if(statx(AT_FDCWD, "test.txt", 0, STATX_CTIME|STATX_MTIME|STATX_ATIME, &file_info) == -1) {
      perror("Error getting file status");
      return 1;
   }
   printf("C=%lld\n", file_info.stx_ctime.tv_sec);
   printf("A=%lld\n", file_info.stx_atime.tv_sec);
   printf("M=%lld\n", file_info.stx_mtime.tv_sec);

   return 0;
}
