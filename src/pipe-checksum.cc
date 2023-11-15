// =================================================================
//          #     #                 #     #
//          ##    #   ####   #####  ##    #  ######   #####
//          # #   #  #    #  #    # # #   #  #          #
//          #  #  #  #    #  #    # #  #  #  #####      #
//          #   # #  #    #  #####  #   # #  #          #
//          #    ##  #    #  #   #  #    ##  #          #
//          #     #   ####   #    # #     #  ######     #
//
//       ---   The NorNet Testbed for Multi-Homed Systems  ---
//                       https://www.nntb.no
// =================================================================
//
// Pipe into file and write checksum file as well
// Copyright (C) 2022-2024 by Thomas Dreibholz
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact: dreibh@simula.no


#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>

#include <chrono>
#include <string>
#include <cstring>
#include <iostream>


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   // ====== Handle arguments ===============================================
   if(argc < 2) {
      std::cerr << "Usage: " << argv[0] << " file [--digest=SHA256|...]\n";
      exit(1);
   }

   bool        success    = false;
   const char* digestName = "SHA256";
   for(int i = 2; i < argc; i++) {
      if(!strncmp(argv[i], "--digest=", 9)) {
         digestName = (const char*)&argv[i][9];
      }
      else {
         std::cerr << "ERROR: Invalid option " << argv[i] << "!\n";
         exit(1);
      }
   }
   const EVP_MD* md = EVP_get_digestbyname(digestName);
   if(md == nullptr) {
      std::cerr << "ERROR: Unknown message digest " << digestName << "!\n";
      exit(1);
   }
   const std::string outputFileName      = argv[1];
   const std::string checksumFileName    = outputFileName + ".checksum";
   const std::string tmpOutputFileName   = outputFileName + ".tmp";
   const std::string tmpChecksumFileName = checksumFileName + ".tmp";
   unsigned long long                          totalBytesWritten = 0;
   const std::chrono::system_clock::time_point t1                 = std::chrono::system_clock::now();


   // ====== Create files ===================================================
   remove(outputFileName.c_str());
   remove(checksumFileName.c_str());
   FILE* outputFile = fopen(tmpOutputFileName.c_str(), "w");
   if(outputFile != nullptr) {
      FILE* checksumFile = fopen(tmpChecksumFileName.c_str(), "wt");
      if(checksumFile != nullptr) {
         EVP_MD_CTX* digestNameContext;
         digestNameContext = EVP_MD_CTX_create();
         if(digestNameContext != nullptr) {
            EVP_DigestInit_ex(digestNameContext, md, NULL);

            // ====== I/O loop ==============================================
            char              buffer[16384];
            ssize_t           length;
            while( (length = fread(&buffer, 1, sizeof(buffer), stdin)) > 0 ) {
               EVP_DigestUpdate(digestNameContext, buffer, length);
               if(fwrite(&buffer, length, 1, outputFile) != 1) {
                  std::cerr << "ERROR: Writing to " << tmpOutputFileName << " failed: " << strerror(errno) << "!\n";
                  goto finish;
               }
               totalBytesWritten += (unsigned long long)length;
            }

            // ====== Write checksum ========================================
            unsigned char mdValue[EVP_MAX_MD_SIZE];
            unsigned int  mdLength;
            EVP_DigestFinal_ex(digestNameContext, mdValue, &mdLength);
            fprintf(checksumFile, "%s (%s) = ", digestName, outputFileName.c_str());
            for(unsigned int i = 0; i < mdLength; i++) {
               fprintf(checksumFile, "%02x", mdValue[i]);
            }
            fputs("\n", checksumFile);

            success = true;

finish:
            EVP_MD_CTX_destroy(digestNameContext);
         }
         else {
            std::cerr << "ERROR: EVP_MD_CTX_create() failed!\n";
         }
         EVP_cleanup();

         // ====== Close files ==============================================
         if(fclose(checksumFile)) {
            std::cerr << "ERROR: Unable to close checksum file " << tmpChecksumFileName << "!\n";
            success = false;
         }
      }
      if(fclose(outputFile)) {
         std::cerr << "ERROR: Unable to close output file " << tmpOutputFileName << "!\n";
         success = false;
      }
   }
   else {
      std::cerr << "ERROR: Unable to write output file " << tmpOutputFileName << "!\n";
   }


   // ====== Rename files ===================================================
   if(success) {
      if(rename(tmpOutputFileName.c_str(), outputFileName.c_str())) {
         std::cerr << "ERROR: Unable to rename " << tmpOutputFileName << " to " << outputFileName << "!\n";
         success = false;
      }
      if(rename(tmpChecksumFileName.c_str(), checksumFileName.c_str())) {
         std::cerr << "ERROR: Unable to rename " << tmpChecksumFileName << " to " << checksumFileName << "!\n";
         success = false;
      }
   }
   if(success) {
      const std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();

      // ====== Write statistics ============================================
      std::cerr << "Wrote " << totalBytesWritten << " B in "
                << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms => "
                << (totalBytesWritten / 1048576.0) / (std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000000.0) << " MiB/s\n";
   }
   else {
      remove(tmpOutputFileName.c_str());
      remove(tmpChecksumFileName.c_str());
   }

   return (success == false);
}
