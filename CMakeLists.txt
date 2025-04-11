# ==========================================================================
#     _   _ _ ____            ____          _____
#    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
#    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
#    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
#    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
#
#       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
#                 https://www.nntb.no/~dreibh/hipercontracer/
# ==========================================================================
#
# High-Performance Connectivity Tracer (HiPerConTracer)
# Copyright (C) 2015-2025 by Thomas Dreibholz
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact: dreibh@simula.no

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(hipercontracer LANGUAGES C CXX)

SET(BUILD_MAJOR "2")
SET(BUILD_MINOR "0")
SET(BUILD_PATCH "11")
SET(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH})

# HiPerConTracer needs at least C++17!
SET(CMAKE_CXX_STANDARD 17)


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
SET(CPACK_SOURCE_IGNORE_FILES "\\\\.git;\\\\.swp$;~$;\\\\.\\\\#;/\\\\#")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "^${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}[_-]")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "\\\\.cmake$|\\\\.make$|\\\\.log$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/CMakeCache\\\\.txt$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/(CMakeFiles|CMakeScripts|_CPack_Packages)/")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/package-version\\\\.h$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/packaging\\\\.conf$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "^${PROJECT_SOURCE_DIR}/(po.*/|src.*/|)Makefile$")
INCLUDE(CPack)

ADD_CUSTOM_TARGET(dist COMMAND ${CMAKE_MAKE_PROGRAM} clean package_source)


#############################################################################
#### UNINSTALL                                                           ####
#############################################################################

IF(NOT TARGET uninstall)
   CONFIGURE_FILE(
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
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib" "/usr/local/lib/mariadb")
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

INCLUDE(CheckIncludeFile)
INCLUDE(CheckStructHasMember)
INCLUDE(CheckCCompilerFlag)

OPTION(WITH_STATIC_LIBRARIES "Build static libraries"              0)
OPTION(WITH_SHARED_LIBRARIES "Build shared libraries"              1)
OPTION(STATIC_BUILD          "Build statically-linked executables" 0)
IF ((NOT WITH_STATIC_LIBRARIES) AND (NOT WITH_SHARED_LIBRARIES))
   MESSAGE(FATAL_ERROR "Static (WITH_STATIC_LIBRARIES) or shared (WITH_SHARED_LIBRARIES) libraries must be enabled!")
ENDIF()

IF (STATIC_BUILD)
   IF (WITH_SHARED_LIBRARIES)
      MESSAGE(FATAL_ERROR "WITH_STATIC_LIBRARIES=ON cannot be used with WITH_SHARED_LIBRARIES=ON!")
   ENDIF()
   MESSAGE("Building static executables")
   SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
   SET(Boost_USE_STATIC_LIBS       ON)
   SET(CMAKE_EXE_LINKER_FLAGS      "-static")
ELSE()
   IF (NOT WITH_SHARED_LIBRARIES)
      MESSAGE(FATAL_ERROR "STATIC_BUILD=OFF cannot be used with WITH_SHARED_LIBRARIES=OFF!")
   ENDIF()
ENDIF()

OPTION(WITH_HIPERCONTRACER  "Build HiPerConTracer"                      1)
OPTION(WITH_TRIGGER         "Build HiPerConTracer Trigger"              1)
OPTION(WITH_IMPORTER        "Build HiPerConTracer Importer"             1)
OPTION(WITH_SYNC            "Build HiPerConTracer Synchronisation Tool" 1)
OPTION(WITH_RTUNNEL         "Build HiPerConTracer Reverse Tunnel Tool"  1)
OPTION(WITH_NODE            "Build HiPerConTracer Node Tools"           1)
OPTION(WITH_COLLECTOR       "Build HiPerConTracer Collector Tools"      1)
OPTION(WITH_QUERY           "Build HiPerConTracer Query Tool"           1)
OPTION(WITH_RESULTS         "Build HiPerConTracer Results Tool"         1)
OPTION(WITH_VIEWER          "Build HiPerConTracer Viewer Tool"          1)
OPTION(WITH_DBSHELL         "Build HiPerConTracer Database Shell"       1)
OPTION(WITH_DBEAVER_TOOLS   "Build HiPerConTracer DBeaver Tools"        1)
OPTION(WITH_PIPE_CHECKSUM   "Build Pipe Checksum Tool"                  1)
OPTION(WITH_UDP_ECHO_SERVER "Build UDP Echo Server"                     1)
OPTION(WITH_ICONS           "Build HiPerConTracer icons and logo files" 1)


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

# ====== Threads ============================================================
FIND_PACKAGE(Threads REQUIRED)

# ====== Boost ==============================================================
IF(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
   # Use BoostConfig.cmake to find BOOST libraries, if possible.
   CMAKE_POLICY(SET CMP0167 NEW)
ENDIF()
FIND_PACKAGE(Boost REQUIRED COMPONENTS log date_time system thread iostreams filesystem program_options)
IF (NOT Boost_FOUND)
   MESSAGE(FATAL_ERROR "Cannot find Boost libraries!")
ELSE()
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
ENDIF()
IF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
   ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)   # <<-- Required to handle BOOST Log link problem!
ENDIF()

# ====== INotify ============================================================
IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
   SET(INotify_FOUND TRUE)
   SET(INotify_LIBRARIES "")
   SET(INotify_INCLUDE_DIRS "")
ELSE()
   FIND_LIBRARY(INotify_LIBRARIES NAMES inotify)
   FIND_PATH(INotify_INCLUDE_DIRS NAMES sys/inotify.h)
ENDIF()

# ====== libz ===============================================================
FIND_LIBRARY(Z_LIBRARY NAMES z)
IF (NOT Z_LIBRARY)
   MESSAGE(FATAL_ERROR
           " Cannot find Z library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y zlib1g-dev\n"
           " * Fedora:        sudo dnf install -y zlib-ng-compat-devel\n"
           " * SuSE:          sudo zypper install -y zlib-devel\n"
           " * Alpine:        sudo apk add zlib-ng-dev\n"
           " * FreeBSD:       sudo pkg install -y ???")
ENDIF()

# ====== libbz2 =============================================================
FIND_LIBRARY(BZ2_LIBRARY NAMES bz2)
IF (NOT BZ2_LIBRARY)
   MESSAGE(FATAL_ERROR
           " Cannot find BZ2 library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y libbz2-dev\n"
           " * Fedora:        sudo dnf install -y bzip2-devel\n"
           " * SuSE:          sudo zypper install -y libbz2-devel\n"
           " * Alpine:        sudo apk add bzip2-dev\n"
           " * FreeBSD:       sudo pkg install -y bzip2")
ENDIF()

# ====== liblzma ============================================================
FIND_LIBRARY(LZMA_LIBRARY NAMES lzma)
IF (NOT LZMA_LIBRARY)
   MESSAGE(FATAL_ERROR
           " Cannot find LZMA library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y liblzma-dev\n"
           " * Fedora:        sudo dnf install -y xz-devel\n"
           " * SuSE:          sudo zypper install -y xz-devel\n"
           " * Alpine:        sudo apk add xz-dev\n"
           " * FreeBSD:       sudo pkg install -y lzmalib")
ENDIF()

# ====== C-ARES =============================================================
# IF (WITH_HIPERCONTRACER)
#    FIND_LIBRARY(CARES_LIBRARY NAMES cares libcares)
#    IF (NOT CARES_LIBRARY)
#       MESSAGE(FATAL_ERROR
#             " Cannot find C-ARES DNS resolver library! Try:\n"
#             " * Ubuntu/Debian: sudo apt install -y libc-ares-dev\n"
#             " * Fedora:        sudo dnf install -y c-ares-devel\n"
#             " * SuSE:          sudo zypper install -y c-ares-devel\n"
#             " * Alpine:        sudo apk add c-ares-dev\n"
#             " * FreeBSD:       sudo pkg install -y c-ares")
#    ENDIF()
# ENDIF()

# ====== libcrypto ==========================================================
IF (WITH_PIPE_CHECKSUM)
   FIND_LIBRARY(CRYPTO_LIBRARY crypto libcrypto)
   IF (NOT CRYPTO_LIBRARY)
      MESSAGE(FATAL_ERROR
              " Cannot find libcrypto library! Try:\n"
              " * Ubuntu/Debian: sudo apt install -y libssl-dev\n"
              " * Fedora:        sudo dnf install -y openssl-devel\n"
              " * SuSE:          sudo zypper install -y openssl-devel\n"
              " * Alpine:        sudo apk add openssl-dev\n"
              " * FreeBSD:       sudo pkg install -y openssl")
   ENDIF()
ENDIF()

# ====== Database client libraries ==========================================
IF (WITH_IMPORTER)

   # ------ Debug backend ---------------------------------------------------
   OPTION(ENABLE_BACKEND_DEBUG "Enable SQL/NoSQL Debug dummy backed" 1)
   IF (ENABLE_BACKEND_DEBUG)
      MESSAGE("Building with SQL/NoSQL Debug dummy backend (ENABLE_BACKEND_DEBUG=${ENABLE_BACKEND_DEBUG})")
   ELSE()
      MESSAGE("SQL/NoSQL Debug dummy backend disabled!")
   ENDIF()

   # ------ MariaDB/MySQL ---------------------------------------------------
   OPTION(ENABLE_BACKEND_MARIADB "Enable MySQL/MariaDB backed" 1)
   IF (ENABLE_BACKEND_MARIADB)
      MESSAGE("Building with MySQL/MariaDB backend (ENABLE_BACKEND_MARIADB=${ENABLE_BACKEND_MARIADB})")

      MESSAGE("Looking for MySQL/MariaDB C Client library:")
      FIND_LIBRARY(LIBMARIADB_LIBRARY NAMES mariadb  PATH_SUFFIXES mariadb mysql)
      MESSAGE(" * Library: ${LIBMARIADB_LIBRARY}")
      FIND_PATH(LIBMARIADB_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mariadb mysql)
      MESSAGE(" * Includes: ${LIBMARIADB_INCLUDE_DIR}")
      IF ((NOT LIBMARIADB_INCLUDE_DIR) OR (NOT LIBMARIADB_LIBRARY))
         MESSAGE(FATAL_ERROR
                 " Cannot find MySQL/MariaDB C Client Library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libmariadb-dev\n"
                 " * Fedora:        sudo dnf install -y mariadb-connector-c-devel\n"
                 " * SuSE:          sudo zypper install -y libmariadb-devel\n"
                 " * Alpine:        sudo apk add mariadb-connector-c-dev\n"
                 " * FreeBSD:       sudo pkg install -y mariadb-connector-c")
      ENDIF()
   ELSE()
      MESSAGE("MySQL/MariaDB backend disabled!")
   ENDIF()

   # ------ PostgreSQL ------------------------------------------------------
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
                 " Cannot find PostgreSQL C++ Client Library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libpqxx-dev\n"
                 " * Fedora:        sudo dnf install -y libpqxx-devel\n"
                 " * SuSE:          sudo zypper install -y libpqxx-devel\n"
                 " * Alpine:        sudo apk add mariadb-connector-c\n"
                 " * FreeBSD:       sudo pkg install -y postgresql-libpqxx")
      ENDIF()
   ELSE()
      MESSAGE("PostgreSQL backend disabled!")
   ENDIF()

   # ------ MongoDB ---------------------------------------------------------
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
                 " Cannot find MongoDB C Client Library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libmongoc-dev\n"
                 " * Fedora:        sudo dnf install -y mongo-c-driver-devel\n"
                 " * SuSE:          (needs PPA!)\n"
                 " * Alpine:        sudo apk add mongo-c-driver-dev\n"
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
                 " * SuSE:          (needs PPA!)\n"
                 " * Alpine:        sudo apk add libbson-dev\n"
                 " * FreeBSD:       sudo pkg install -y libbson")
      ENDIF()
   ELSE()
      MESSAGE("MongoDB backend disabled!")
   ENDIF()

ENDIF()

# ====== Graphics tools for icons and logo ==================================
IF (WITH_ICONS)
   FIND_PROGRAM(GS gs PATHS /bin /usr/bin /usr/local/bin)
   IF(NOT GS)
      MESSAGE(FATAL_ERROR
            " Cannot find GhostScript! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y ghostscript\n"
            " * Fedora:        sudo dnf install -y ghostscript\n"
            " * FreeBSD:       sudo pkg install -y ghostscript10")
   ENDIF()

   FIND_PROGRAM(PDF2SVG pdf2svg PATHS /bin /usr/bin /usr/local/bin)
   IF(NOT PDF2SVG)
      MESSAGE(FATAL_ERROR
            " Cannot find PDF2SVG! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y pdf2svg\n"
            " * Fedora:        sudo dnf install -y pdf2svg\n"
            " * FreeBSD:       sudo pkg install -y pdf2svg")
   ENDIF()

   FIND_PROGRAM(GM gm PATHS /bin /usr/bin /usr/local/bin)
   IF(NOT GM)
      MESSAGE(FATAL_ERROR
            " Cannot find GraphicsMagick! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y graphicsmagick\n"
            " * Fedora:        sudo dnf install -y GraphicsMagick\n"
            " * FreeBSD:       sudo pkg install -y GraphicsMagick")
   ENDIF()
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(src)
