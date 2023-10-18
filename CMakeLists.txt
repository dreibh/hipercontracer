#  =================================================================
#           #     #                 #     #
#           ##    #   ####   #####  ##    #  ######   #####
#           # #   #  #    #  #    # # #   #  #          #
#           #  #  #  #    #  #    # #  #  #  #####      #
#           #   # #  #    #  #####  #   # #  #          #
#           #    ##  #    #  #   #  #    ##  #          #
#           #     #   ####   #    # #     #  ######     #
#
#        ---   The NorNet Testbed for Multi-Homed Systems  ---
#                        https://www.nntb.no
#  =================================================================
#
#  High-Performance Connectivity Tracer (HiPerConTracer)
#  Copyright (C) 2015-2023 by Thomas Dreibholz
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#  Contact: dreibh@simula.no


CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(hipercontracer LANGUAGES C CXX)

SET(BUILD_MAJOR "2")
SET(BUILD_MINOR "0")
SET(BUILD_PATCH "0~beta1")
SET(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH})

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)


#############################################################################
#### INSTALLATION_DIRECTORIES                                            ####
#############################################################################

# See: https://cmake.org/cmake/help/v3.0/module/GNUInstallDirs.html
INCLUDE(GNUInstallDirs)


#############################################################################
#### PACKAGING                                                           ####
#############################################################################

SET(CPACK_SOURCE_GENERATOR "TXZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME
   "${CMAKE_PROJECT_NAME}-${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH}")
SET(CPACK_SOURCE_IGNORE_FILES
   "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}-*;${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}_*;/build/;/.git/;/config.log;/CMakeFiles/;CMakeCache.txt$;.cmake$;.tar.gz$;/_CPack_Packages;/Makefile$;~$;/build-deb;/clean-deb;/filter-empty-entries;/make-symbols;/make-ppa;/make-deb;/debian.conf;/make-rpm;/rpm.conf;${CPACK_SOURCE_IGNORE_FILES}")
INCLUDE(CPack)

ADD_CUSTOM_TARGET(dist COMMAND ${CMAKE_MAKE_PROGRAM} clean package_source)


INCLUDE(CheckIncludeFile)
INCLUDE(CheckStructHasMember)


#############################################################################
#### UNINSTALL                                                           ####
#############################################################################

IF(NOT TARGET uninstall)
   configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
      IMMEDIATE @ONLY)

   ADD_CUSTOM_TARGET(uninstall
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
ENDIF()


#############################################################################
#### OS DEPENDENT                                                        ####
#############################################################################

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/local/include")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include" "/usr/local/opt/openssl/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/local/include" "/usr/local/opt/openssl/include")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/pkg/include" "/usr/include" "/usr/local/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/pkg/include" "/usr/local/include")

ELSE()
    MESSAGE(FATAL_ERROR ${CMAKE_SYSTEM_NAME} " not supported (yet?)")

ENDIF()


#############################################################################
#### OPTIONS                                                             ####
#############################################################################

OPTION(STATIC_BUILD "Build statically-linked executables" 0)
IF (STATIC_BUILD)
   MESSAGE("Trying to build static executables")
   LIST(REMOVE_ITEM CMAKE_FIND_LIBRARY_SUFFIXES   ".a" )
   LIST(INSERT      CMAKE_FIND_LIBRARY_SUFFIXES 0 ".a" )
   SET(Boost_USE_STATIC_LIBS ON)
ENDIF()

OPTION(WITH_TRIGGER         "Build HiPerConTracer Trigger"      1)
OPTION(WITH_IMPORTER        "Build HiPerConTracer Importer"     1)
OPTION(WITH_QUERY           "Build HiPerConTracer Query Tool"   1)
OPTION(WITH_RESULTS         "Build HiPerConTracer Results Tool" 1)
OPTION(WITH_PIPE_CHECKSUM   "Build Pipe Checksum Tool"          1)
OPTION(WITH_UDP_ECHO_SERVER "Build UDP Echo Server"             1)


#############################################################################
#### COMPILER FLAGS                                                      ####
#############################################################################

IF(CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
  STRING(APPEND CMAKE_C_STANDARD_LIBRARIES   " -latomic")
  STRING(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -latomic")
ENDIF()


#############################################################################
#### REQUIREMENTS                                                        ####
#############################################################################

# ###### Threads ############################################################
FIND_PACKAGE(Threads REQUIRED)

# ###### Boost ##############################################################
FIND_PACKAGE(Boost REQUIRED COMPONENTS log date_time system thread iostreams filesystem program_options)
IF (NOT Boost_FOUND)
   MESSAGE(FATAL_ERROR "Cannot find Boost libraries!")
ELSE()
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
ENDIF()
IF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
   ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)   # <<-- Required to handle BOOST Log link problem!
ENDIF()

# ###### INotify ############################################################
IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
   SET(INotify_FOUND TRUE)
   SET(INotify_LIBRARIES "")
   SET(INotify_INCLUDE_DIRS "")
ELSE()
   FIND_LIBRARY(INotify_LIBRARIES NAMES inotify)
   FIND_PATH(INotify_INCLUDE_DIRS NAMES sys/inotify.h)
ENDIF()

# ###### C-ARES #############################################################
FIND_LIBRARY(CARES_LIBRARY NAMES cares libcares)
IF (NOT CARES_LIBRARY)
   MESSAGE(FATAL_ERROR
           " Cannot find C-ARES DNS resolver library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y libc-ares-dev\n"
           " * Fedora:        sudo dnf install -y c-ares-devel\n"
           " * FreeBSD:       sudo pkg install -y c-ares")
ENDIF()

# ###### libcrypto ##########################################################
IF (WITH_PIPE_CHECKSUM)
   FIND_LIBRARY(CRYPTO_LIBRARY crypto libcrypto)
   IF (NOT CRYPTO_LIBRARY)
      MESSAGE(FATAL_ERROR
            " Cannot find libcrypto library! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y libssl-dev\n"
            " * Fedora:        sudo dnf install -y openssl-devel\n"
            " * FreeBSD:       sudo pkg install -y openssl")
   ENDIF()
ENDIF()

# ###### Database client libraries ##########################################
IF (WITH_IMPORTER)
   # ------ MariaDB/MySQL ---------------------------------------------------
   OPTION(ENABLE_BACKEND_MARIADB "Enable MySQL/MariaDB backed" 1)
   IF (ENABLE_BACKEND_MARIADB)
      MESSAGE("Building with MySQL/MariaDB backend (ENABLE_BACKEND_MARIADB=${ENABLE_BACKEND_MARIADB})")

      MESSAGE("Looking for MySQL/MariaDB C++ Client library:")
      FIND_LIBRARY(MYSQLCPPCONN_LIBRARY NAMES mysqlcppconn)
      MESSAGE(" * Library: ${MYSQLCPPCONN_LIBRARY}")
      FIND_PATH(MYSQLCPPCONN_INCLUDE_DIR NAMES driver.h PATH_SUFFIXES cppconn mysql-cppconn/jdbc/cppconn)
      MESSAGE(" * Includes: ${MYSQLCPPCONN_INCLUDE_DIR}")
      IF ((NOT MYSQLCPPCONN_INCLUDE_DIR) OR (NOT MYSQLCPPCONN_LIBRARY))
         MESSAGE(FATAL_ERROR
                 " Cannot find MySQL/MariaDB C++ client library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libmysqlcppconn-dev\n"
                 " * Fedora:        sudo dnf install -y mysql-connector-c++-devel (needs PPA!)\n"
                 " * FreeBSD:       sudo pkg install -y mysql-connector-c++")
      ENDIF()
   ELSE()
      MESSAGE("MySQL/MariaDB backend disabled!")
   ENDIF()

   # ------ PostgreSQL ---------------------------------------------------------
   OPTION(ENABLE_BACKEND_POSTGRESQL "Enable PostgreSQL backed" 1)
   IF (ENABLE_BACKEND_POSTGRESQL)
      MESSAGE("Building with PostgreSQL backend (ENABLE_BACKEND_POSTGRESQL=${ENABLE_BACKEND_POSTGRESQL})")

      MESSAGE("Looking for PostgreSQL C++ Client library:")
      FIND_LIBRARY(LIBPQXX_LIBRARY NAMES pqxx)
      MESSAGE(" * Library: ${LIBPQXX_LIBRARY}")
      FIND_PATH(LIBPQXX_INCLUDE_DIR NAMES pqxx PATH_SUFFIXES pqxx)
      MESSAGE(" * Includes: ${LIBPQXX_INCLUDE_DIR}")
      IF ((NOT LIBPQXX_INCLUDE_DIR) OR (NOT LIBPQXX_LIBRARY))
         MESSAGE(FATAL_ERROR
                 " Cannot find PostgreSQL C++  client library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libpqxx-dev\n"
                 " * Fedora:        sudo dnf install -y libpqxx-devel\n"
                 " * FreeBSD:       sudo pkg install -y postgresql-libpqxx")
      ENDIF()
   ELSE()
      MESSAGE("PostgreSQL backend disabled!")
   ENDIF()

   # ------ MongoDB ------------------------------------------------------------
   OPTION(ENABLE_BACKEND_MONGODB "Enable MongoDB backed" 1)
   IF (ENABLE_BACKEND_MONGODB)
      MESSAGE("Building with MongoDB backend (ENABLE_BACKEND_MONGODB=${ENABLE_BACKEND_MONGODB})")

      MESSAGE("Looking for MongoDB C Client library:")
      FIND_LIBRARY(LIBMONGOC_LIBRARY  NAMES mongoc-1.0)
      MESSAGE(" * Library: ${LIBMONGOC_LIBRARY}")
      FIND_PATH(LIBMONGOC_INCLUDE_DIR NAMES mongoc.h PATH_SUFFIXES libmongoc-1.0)
      MESSAGE(" * Includes: ${LIBMONGOC_INCLUDE_DIR}")
      IF ((NOT LIBMONGOC_INCLUDE_DIR) OR (NOT LIBMONGOC_LIBRARY))
         MESSAGE(FATAL_ERROR
                 " Cannot find MongoDB C client library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libmongoc-dev\n"
                 " * Fedora:        sudo dnf install -y mongo-c-driver-devel\n"
                 " * FreeBSD:       sudo pkg install -y mongo-c-driver")
      ENDIF()

      MESSAGE("Looking for MongoDB BSON library:")
      FIND_LIBRARY(LIBBSON_LIBRARY  NAMES bson-1.0)
      MESSAGE(" * Library: ${LIBBSON_LIBRARY}")
      FIND_PATH(LIBBSON_INCLUDE_DIR NAMES bson.h PATH_SUFFIXES libbson-1.0)
      MESSAGE(" * Includes: ${LIBBSON_INCLUDE_DIR}")
      IF ((NOT LIBBSON_INCLUDE_DIR) OR (NOT LIBBSON_LIBRARY))
         MESSAGE(FATAL_ERROR
                 " Cannot find MongoDB BSON library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libbson-dev\n"
                 " * Fedora:        sudo dnf install -y libbson-devel\n"
                 " * FreeBSD:       sudo pkg install -y libbson")
      ENDIF()
   ELSE()
      MESSAGE("MongoDB backend disabled!")
   ENDIF()
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(src)
