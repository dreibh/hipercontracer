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
# Copyright (C) 2015-2026 by Thomas Dreibholz
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
SET(BUILD_MINOR "2")
SET(BUILD_PATCH "7")
SET(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH})

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

IF(NOT "${CMAKE_CXX_STANDARD}")
   # HiPerConTracer also builds with C++17, but libpqxx 8.x expects C++20.
   # Assuming C++20 here. If libpqxx < 8.0 is detected, the standard will
   # be lowered to C++17 later.
   SET(CMAKE_CXX_STANDARD 20)
ENDIF()


#############################################################################
#### CMAKE INCLUDES                                                      ####
#############################################################################

INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckStructHasMember)
INCLUDE(GNUInstallDirs)


#############################################################################
#### PACKAGING                                                           ####
#############################################################################

SET(CPACK_SOURCE_GENERATOR "TXZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH}")
SET(CPACK_SOURCE_IGNORE_FILES "\\\\.git;\\\\.swp$;~$;\\\\.\\\\#;/\\\\#")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "^${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}[_-]")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "\\\\.make$|\\\\.log$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/CMakeCache\\\\.txt$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/(CMakeFiles|CMakeScripts|_CPack_Packages)/")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/package-version\\\\.h$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "/packaging\\\\.conf$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "^${PROJECT_SOURCE_DIR}/(po.*/|src.*/|)Makefile$")
LIST(APPEND CPACK_SOURCE_IGNORE_FILES "^${PROJECT_SOURCE_DIR}/(po.*|src.*|[^/]*)\\\\.cmake$")
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
#### OS-DEPENDENT                                                        ####
#############################################################################

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "NetBSD")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
   IF(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
      SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
   ENDIF()
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "SunOS")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
   ADD_COMPILE_OPTIONS(-pthreads)
   ADD_COMPILE_DEFINITIONS(_REENTRANT)
   # Necessary for correctly detecting BOOST libraries:
   IF (CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86_64|amd64|AMD64")
      SET(CMAKE_LIBRARY_ARCHITECTURE "amd64")
   ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
      SET(CMAKE_LIBRARY_ARCHITECTURE "sparcv9")
   ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
      SET(CMAKE_LIBRARY_ARCHITECTURE "aarch64")
   ENDIF()
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
ELSE()
   MESSAGE(FATAL_ERROR "${CMAKE_SYSTEM_NAME} not supported (yet?)")
ENDIF()


#############################################################################
#### OPTIONS                                                             ####
#############################################################################

OPTION(WITH_STATIC_LIBRARIES "Build static libraries"              OFF)
OPTION(WITH_SHARED_LIBRARIES "Build shared libraries"              ON)
OPTION(STATIC_BUILD          "Build statically-linked executables" OFF)

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

OPTION(WITH_COLLECTOR            "Build HiPerConTracer Collector Tools"                        ON)
OPTION(WITH_DBEAVER_TOOLS        "Build HiPerConTracer DBeaver Tools"                          ON)
OPTION(WITH_DBSHELL              "Build HiPerConTracer Database Shell"                         ON)
OPTION(WITH_EXAMPLE_RESULTS      "Include example results"                                     ON)
OPTION(WITH_EXAMPLE_SCRIPTS      "Include example scripts"                                     ON)
OPTION(WITH_HIPERCONTRACER       "Build HiPerConTracer"                                        ON)
OPTION(WITH_ICONS                "Build HiPerConTracer icons and logo files"                   ON)
OPTION(WITH_IMPORTER             "Build HiPerConTracer Importer"                               ON)
OPTION(WITH_NODE                 "Build HiPerConTracer Node Tools"                             ON)
OPTION(WITH_PIPE_CHECKSUM        "Build Pipe Checksum Tool"                                    ON)
OPTION(WITH_QUERY                "Build HiPerConTracer Query Tool"                             ON)
OPTION(WITH_RESULTS              "Build HiPerConTracer Results Tool"                           ON)
OPTION(WITH_RTUNNEL              "Build HiPerConTracer Reverse Tunnel Tool"                    ON)
OPTION(WITH_SYNC                 "Build HiPerConTracer Synchronisation Tool"                   ON)
OPTION(WITH_TRIGGER              "Build HiPerConTracer Trigger"                                ON)
OPTION(WITH_UDP_ECHO_SERVER      "Build UDP Echo Server"                                       ON)
OPTION(WITH_VIEWER               "Build HiPerConTracer Viewer Tool"                            ON)

OPTION(WITH_LIBHIPERCONTRACER    "Build HiPerConTracer Library (libhipercontracer)"            OFF)
OPTION(WITH_LIBHPCTDB            "Build HiPerConTracer Database Backend Library (libhpctdb)"   OFF)
OPTION(WITH_LIBHPCTIO            "Build HiPerConTracer I/O Library (libhpctio)"                OFF)
OPTION(WITH_LIBUNIVERSALIMPORTER "Build Universal Importer Library (libuniversalimporter)"     OFF)

IF ((WITH_HIPERCONTRACER) OR (WITH_TRIGGER))
   SET(WITH_LIBHIPERCONTRACER ON)
ENDIF()

IF (WITH_IMPORTER)
   SET(WITH_LIBUNIVERSALIMPORTER ON)
ENDIF()

IF ((WITH_LIBHIPERCONTRACER) OR (WITH_QUERY) OR (WITH_RESULTS))
   SET(WITH_LIBHPCTIO ON)
ENDIF()

IF ((WITH_LIBUNIVERSALIMPORTER) OR (WITH_QUERY) OR (WITH_RESULTS))
   SET(WITH_LIBHPCTDB ON)
ENDIF()



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

# ====== Solaris: libsocket and libnsl ======================================
IF (CMAKE_SYSTEM_NAME MATCHES "SunOS")
   FIND_LIBRARY(SOCKET_LIBRARY socket REQUIRED)
   FIND_LIBRARY(NSL_LIBRARY    nsl    REQUIRED)
ENDIF()

# ====== Boost ==============================================================
IF(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
   # Use BoostConfig.cmake to find BOOST libraries, if possible.
   CMAKE_POLICY(SET CMP0167 NEW)
ENDIF()
FIND_PACKAGE(Boost COMPONENTS log date_time thread iostreams filesystem program_options)
IF (Boost_FOUND)
   MESSAGE(STATUS "BOOST libraries found:")
   MESSAGE(STATUS " Boost_VERSION:      ${Boost_VERSION}")
   MESSAGE(STATUS " Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
   MESSAGE(STATUS " Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
   MESSAGE(STATUS " Boost_LIBRARIES:    ${Boost_LIBRARIES}")
   FIND_PACKAGE(Boost COMPONENTS log date_time thread iostreams filesystem program_options REQUIRED)
ELSE()
   MESSAGE(FATAL_ERROR "Cannot find BOOST libraries! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y libboost-all-dev\n"
           " * Fedora:        sudo dnf install -y boost-devel\n"
           " * OpenSUSE:      sudo zypper install -y boost-devel\n"
           " * Alpine:        sudo apk add boost-dev\n"
           " * FreeBSD:       sudo pkg install -y boost\n"
           " * NetBSD:        sudo pkgin -y install boost\n"
           " * OpenBSD:       sudo pkg_add boost\n"
           " * Solaris:       sudo pkg install boost\n"
           " * Apple:         brew install boost")
ENDIF()

# ====== INotify ============================================================
IF (WITH_LIBUNIVERSALIMPORTER)
   IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
      # Linux provides INotify without need for any library!
      SET(INotify_FOUND TRUE)
      SET(INotify_LIBRARIES "")
      SET(INotify_INCLUDE_DIRS "")
   ELSE()
      FIND_LIBRARY(INotify_LIBRARIES NAMES inotify       HINTS /usr/local/lib/inotify)
      FIND_PATH(INotify_INCLUDE_DIRS NAMES sys/inotify.h HINTS /usr/local/include/inotify)
      IF ((NOT INotify_LIBRARIES) OR (NOT INotify_INCLUDE_DIRS))
         MESSAGE(FATAL_ERROR
                 "Cannot find INotify library! Try:\n"
                 " * Ubuntu/Debian: (always included as part of libc)\n"
                 " * Fedora:        (always included as part of libc)\n"
                 " * SuSE:          (always included as part of libc)\n"
                 " * Alpine:        (always included as part of libc)\n"
                 " * FreeBSD:       sudo pkg install -y libinotify\n"
                 " * NetBSD:        sudo pkgin -y install libinotify\n"
                 " * OpenBSD:       sudo pkg_add libinotify\n"
                 " * Solaris:       --- N/A ---\n"
                 " * Apple:         --- N/A ---\n"
                 "If the Importer is not required, it can be disabled by setting WITH_IMPORTER=OFF (add -DWITH_IMPORTER=OFF to CMake options)!")
      ENDIF()
   ENDIF()
ENDIF()

# ====== libz ===============================================================
FIND_LIBRARY(Z_LIBRARY NAMES z)
FIND_PATH(Z_INCLUDE_DIR NAMES zlib.h)
IF (Z_LIBRARY AND Z_INCLUDE_DIR)
   MESSAGE(STATUS "GZip library found:")
   MESSAGE(STATUS " Z_LIBRARY:     ${Z_LIBRARY}")
   MESSAGE(STATUS " Z_INCLUDE_DIR: ${Z_INCLUDE_DIR}")
ELSE()
   MESSAGE(FATAL_ERROR
           "Cannot find Z library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y zlib1g-dev\n"
           " * Fedora:        sudo dnf install -y zlib-ng-compat-devel\n"
           " * SuSE:          sudo zypper install -y zlib-devel\n"
           " * Alpine:        sudo apk add zlib-ng-dev\n"
           " * FreeBSD:       (should be installed by default)\n"
           " * NetBSD:        (should be installed by default)\n"
           " * OpenBSD:       (should be installed by default)\n"
           " * Solaris:       sudo pkg install zlib\n"
           " * Apple:         brew install zlib")
ENDIF()

# ====== libbz2 =============================================================
FIND_LIBRARY(BZ2_LIBRARY NAMES bz2)
FIND_PATH(BZ2_INCLUDE_DIR NAMES bzlib.h)
IF (BZ2_LIBRARY AND BZ2_INCLUDE_DIR)
   MESSAGE(STATUS "BZip2 library found:")
   MESSAGE(STATUS " BZ2_LIBRARY:     ${BZ2_LIBRARY}")
   MESSAGE(STATUS " BZ2_INCLUDE_DIR: ${BZ2_INCLUDE_DIR}")
ELSE()
   MESSAGE(FATAL_ERROR
           "Cannot find BZip2 library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y libbz2-dev\n"
           " * Fedora:        sudo dnf install -y bzip2-devel\n"
           " * SuSE:          sudo zypper install -y libbz2-devel\n"
           " * Alpine:        sudo apk add bzip2-dev\n"
           " * FreeBSD:       sudo pkg install -y bzip2\n"
           " * NetBSD:        (should be installed by default)\n"
           " * OpenBSD:       (should be installed by default)\n"
           " * Solaris    :   sudo pkg install bzip2\n"
           " * Apple:         brew install bzip2")
ENDIF()

# ====== liblzma ============================================================
FIND_LIBRARY(LZMA_LIBRARY NAMES lzma)
FIND_PATH(LZMA_INCLUDE_DIR NAMES lzma.h)
IF (LZMA_LIBRARY AND LZMA_INCLUDE_DIR)
   MESSAGE(STATUS "LZMA library found:")
   MESSAGE(STATUS " LZMA_LIBRARY:     ${LZMA_LIBRARY}")
   MESSAGE(STATUS " LZMA_INCLUDE_DIR: ${LZMA_INCLUDE_DIR}")
ELSE()
   MESSAGE(FATAL_ERROR
           "Cannot find LZMA library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y liblzma-dev\n"
           " * Fedora:        sudo dnf install -y xz-devel\n"
           " * SuSE:          sudo zypper install -y xz-devel\n"
           " * Alpine:        sudo apk add xz-dev\n"
           " * FreeBSD:       sudo pkg install -y lzmalib\n"
           " * NetBSD:        sudo pkgin -y install lzmalib\n"
           " * OpenBSD:       sudo pkg_add xz\n"
           " * Solaris    :   sudo pkg install liblzma\n"
           " * Apple:         brew install xz")
ENDIF()

# ====== libzstd ============================================================
FIND_LIBRARY(ZSTD_LIBRARY NAMES zstd)
FIND_PATH(ZSTD_INCLUDE_DIR NAMES zstd.h)
IF (ZSTD_LIBRARY AND ZSTD_INCLUDE_DIR)
   MESSAGE(STATUS "ZSTD library found:")
   MESSAGE(STATUS " ZSTD_LIBRARY:     ${ZSTD_LIBRARY}")
   MESSAGE(STATUS " ZSTD_INCLUDE_DIR: ${ZSTD_INCLUDE_DIR}")
ELSE()
   MESSAGE(FATAL_ERROR
           "Cannot find ZSTD library! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y libzstd-dev\n"
           " * Fedora:        sudo dnf install -y libzstd-devel\n"
           " * SuSE:          sudo zypper install -y libzstd-devel\n"
           " * Alpine:        sudo apk add libzstd-devel\n"
           " * FreeBSD:       sudo pkg install -y zstd\n"
           " * NetBSD:        sudo pkgin -y install zstd\n"
           " * OpenBSD:       sudo pkg_add zstd\n"
           " * Solaris    :   sudo pkg install zstd\n"
           " * Apple:         brew install zsdt")
ENDIF()

# ====== C-ARES =============================================================
# IF (WITH_HIPERCONTRACER)
#    FIND_LIBRARY(CARES_LIBRARY NAMES cares libcares)
#    FIND_PATH(CARES_INCLUDE_DIR NAMES ares.h)
#    IF (CARES_LIBRARY AND CARES_INCLUDE_DIR)
#       MESSAGE(STATUS "C-ARES library found:")
#       MESSAGE(STATUS " CARES_LIBRARY:     ${CARES_LIBRARY}")
#       MESSAGE(STATUS " CARES_INCLUDE_DIR: ${CARES_INCLUDE_DIR}")
#    ELSE()
#       MESSAGE(FATAL_ERROR
#             "Cannot find C-ARES DNS resolver library! Try:\n"
#             " * Ubuntu/Debian: sudo apt install -y libc-ares-dev\n"
#             " * Fedora:        sudo dnf install -y c-ares-devel\n"
#             " * SuSE:          sudo zypper install -y c-ares-devel\n"
#             " * Alpine:        sudo apk add c-ares-dev\n"
#             " * FreeBSD:       sudo pkg install -y c-ares\n"
#             " * NetBSD:        sudo pkgin -y install libcares\n"
#             " * OpenBSD:       sudo pkg_add libcares\n"
#             " * Solaris:       sudo pkg install ?????\n"
#             " * Apple:         brew install c-ares")
#    ENDIF()
# ENDIF()

# ====== libcrypto ==========================================================
IF (WITH_PIPE_CHECKSUM)
   FIND_LIBRARY(CRYPTO_LIBRARY crypto libcrypto)
   FIND_PATH(CRYPTO_INCLUDE_DIR NAMES openssl/sha.h)
   IF (CRYPTO_LIBRARY AND CRYPTO_INCLUDE_DIR)
      MESSAGE(STATUS "libcrypto library found:")
      MESSAGE(STATUS " CRYPTO_LIBRARY:     ${CRYPTO_LIBRARY}")
      MESSAGE(STATUS " CRYPTO_INCLUDE_DIR: ${CRYPTO_INCLUDE_DIR}")
   ELSE()
      MESSAGE(FATAL_ERROR
              "Cannot find libcrypto library! Try:\n"
              " * Ubuntu/Debian: sudo apt install -y libssl-dev\n"
              " * Fedora:        sudo dnf install -y openssl-devel\n"
              " * SuSE:          sudo zypper install -y openssl-devel\n"
              " * Alpine:        sudo apk add openssl-dev\n"
              " * FreeBSD:       sudo pkg install -y openssl\n"
              " * NetBSD:        sudo pkgin -y install openssl\n"
              " * OpenBSD:       sudo pkg_add openssl\n"
              " * Solaris:       sudo pkg install openssl-3\n"
              " * Apple:         brew install openssl")
   ENDIF()
ENDIF()

# ====== Database client libraries ==========================================
IF (WITH_LIBHPCTDB)

   # ------ Debug backend ---------------------------------------------------
   OPTION(ENABLE_BACKEND_DEBUG "Enable SQL/NoSQL Debug dummy backed" ON)
   IF (ENABLE_BACKEND_DEBUG)
      MESSAGE("Building with SQL/NoSQL Debug dummy backend (ENABLE_BACKEND_DEBUG=${ENABLE_BACKEND_DEBUG})")
   ELSE()
      MESSAGE("Building without SQL/NoSQL Debug backend (ENABLE_BACKEND_DEBUG=${ENABLE_BACKEND_DEBUG})")
      MESSAGE("SQL/NoSQL Debug dummy backend disabled!")
   ENDIF()

   # ------ MariaDB/MySQL ---------------------------------------------------
   OPTION(ENABLE_BACKEND_MARIADB "Enable MySQL/MariaDB backed" ON)
   IF (ENABLE_BACKEND_MARIADB)
      MESSAGE("Building with MySQL/MariaDB backend (ENABLE_BACKEND_MARIADB=${ENABLE_BACKEND_MARIADB})")

      MESSAGE("Looking for MySQL/MariaDB C Client library:")
      FIND_LIBRARY(LIBMARIADB_LIBRARY NAMES mariadb mysqlclient
                   PATH_SUFFIXES            mariadb mysql
                   HINTS                    /opt/homebrew/opt/mariadb-connector-c/lib
                                            /usr/local/opt/mariadb-connector-c/lib
                                            /usr/mariadb/11.4/lib/amd64
                                            /usr/mariadb/10.6/lib/amd64)
      MESSAGE(" * Library: ${LIBMARIADB_LIBRARY}")
      FIND_PATH(LIBMARIADB_INCLUDE_DIR NAMES mysql.h
                PATH_SUFFIXES mariadb mysql
                HINTS /opt/homebrew/opt/mariadb-connector-c/include
                      /usr/local/opt/mariadb-connector-c/include
                      /usr/mariadb/11.4/include
                      /usr/mariadb/10.6/include)
      MESSAGE(" * Includes: ${LIBMARIADB_INCLUDE_DIR}")
      IF ((NOT LIBMARIADB_INCLUDE_DIR) OR (NOT LIBMARIADB_LIBRARY))
         MESSAGE(FATAL_ERROR
                 "Cannot find MySQL/MariaDB C Client Library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libmariadb-dev\n"
                 " * Fedora:        sudo dnf install -y mariadb-connector-c-devel\n"
                 " * SuSE:          sudo zypper install -y libmariadb-devel\n"
                 " * Alpine:        sudo apk add mariadb-connector-c-dev\n"
                 " * FreeBSD:       sudo pkg install -y mariadb-connector-c\n"
                 " * NetBSD:        sudo pkgin -y install mariadb-client\n"
                 " * OpenBSD:       sudo pkg_add mariadb-client\n"
                 " * Solaris:       sudo pkg install mariadb-114\n"
                 " * Apple:         brew install mariadb-connector-c\n"
                 "If MySQL/MariaDB support is not required, it can be disabled by setting ENABLE_BACKEND_MARIADB=OFF (add -DENABLE_BACKEND_MARIADB=OFF to CMake options)!")
      ENDIF()
   ELSE()
      MESSAGE("Building without MySQL/MariaDB backend (ENABLE_BACKEND_MARIADB=${ENABLE_BACKEND_MARIADB})")
      MESSAGE("MySQL/MariaDB backend disabled!")
   ENDIF()

   # ------ PostgreSQL ------------------------------------------------------
   OPTION(ENABLE_BACKEND_POSTGRESQL "Enable PostgreSQL backed" ON)
   IF (ENABLE_BACKEND_POSTGRESQL)
      MESSAGE("Building with PostgreSQL backend (ENABLE_BACKEND_POSTGRESQL=${ENABLE_BACKEND_POSTGRESQL})")

      MESSAGE("Looking for PostgreSQL C++ Client library:")
      LIST(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/libpqxx"
                                    "/usr/local/opt/libpqxx")
      FIND_PACKAGE(PQXX)
      MESSAGE(" * Version:  ${PQXX_VERSION}")
      MESSAGE(" * Library:  ${PQXX_LIBRARIES}")
      MESSAGE(" * Includes: ${PQXX_INCLUDE_DIRS}")
      IF ((NOT PQXX_INCLUDE_DIRS) OR (NOT PQXX_LIBRARIES))
         MESSAGE(FATAL_ERROR
                 "Cannot find PostgreSQL C++ Client Library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libpqxx-dev\n"
                 " * Fedora:        sudo dnf install -y libpqxx-devel\n"
                 " * SuSE:          sudo zypper install -y libpqxx-devel\n"
                 " * Alpine:        (needs PPA or installation from sources!)\n"
                 " * FreeBSD:       sudo pkg install -y postgresql-libpqxx\n"
                 " * NetBSD:        sudo pkgin -y install libpqxx\n"
                 " * OpenBSD:       sudo pkg_add libpqxx\n"
                 " * Solaris:       sudo pkg install libpqxx\n"
                 " * Apple:         brew install libpqxx\n"
                 "If PostgreSQL support is not required, it can be disabled by setting ENABLE_BACKEND_POSTGRESQL=OFF (add -DENABLE_BACKEND_POSTGRESQL=OFF to CMake options)!")
      ENDIF()

      IF (PQXX_VERSION VERSION_GREATER_EQUAL 8.0)
         SET(CMAKE_CXX_STANDARD 20)
      ELSE()
         SET(CMAKE_CXX_STANDARD 17)
      ENDIF()
   ELSE()
      MESSAGE("Building without PostgreSQL backend (ENABLE_BACKEND_POSTGRESQL=${ENABLE_BACKEND_POSTGRESQL})")
      MESSAGE("PostgreSQL backend disabled!")
   ENDIF()

   # ------ MongoDB ---------------------------------------------------------
   OPTION(ENABLE_BACKEND_MONGODB "Enable MongoDB backed" ON)
   IF (ENABLE_BACKEND_MONGODB)
      MESSAGE("Building with MongoDB backend (ENABLE_BACKEND_MONGODB=${ENABLE_BACKEND_MONGODB})")

      MESSAGE("Looking for MongoDB C Client library:")
      FILE(GLOB mongoSearchPaths
         "/usr/include/mongoc-*"            "/usr/include/libmongoc*"
         "/usr/local/include/mongoc-*"      "/usr/local/include/libmongoc*"
         "/usr/pkg/include/mongoc-*"        "/usr/pkg/include/libmongoc-*"
         "/opt/homebrew/include/mongoc-*"   "/opt/homebrew/include/libmongoc*"
      )
      FIND_LIBRARY(LIBMONGOC_LIBRARY NAMES mongoc-1.0 mongoc2 HINTS ${mongoSearchPaths})
      MESSAGE(" * Library: ${LIBMONGOC_LIBRARY}")
      FIND_PATH(LIBMONGOC_INCLUDE_DIR NAMES mongoc/mongoc.h HINTS ${mongoSearchPaths})
      MESSAGE(" * Includes: ${LIBMONGOC_INCLUDE_DIR}")
      IF ((NOT LIBMONGOC_INCLUDE_DIR) OR (NOT LIBMONGOC_LIBRARY))
         MESSAGE(FATAL_ERROR
                 "Cannot find MongoDB C Client Library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libmongoc-dev\n"
                 " * Fedora:        sudo dnf install -y mongo-c-driver-devel\n"
                 " * SuSE:          sudo zypper install -y mongo-c-driver-devel\n"
                 " * Alpine:        sudo apk add mongo-c-driver-dev\n"
                 " * FreeBSD:       sudo pkg install -y mongo-c-driver\n"
                 " * NetBSD:        sudo pkgin -y install mongo-c-driver\n"
                 " * OpenBSD:       (needs PPA or installation from sources!)\n"
                 " * Solaris:       (needs PPA or installation from sources!)\n"
                 " * Apple:         brew install mongo-c-driver\n"
                 "If MongoDB support is not required, it can be disabled by setting ENABLE_BACKEND_MONGODB=OFF (add -DENABLE_BACKEND_MONGODB=OFF to CMake options)!")
      ENDIF()

      MESSAGE("Looking for MongoDB BSON library:")
      FILE(GLOB bsonSearchPaths
         "/usr/include/bson-*"            "/usr/include/libbson-*"
         "/usr/local/include/bson-*"      "/usr/local/include/libbson-*"
         "/usr/pkg/include/bson-*"        "/usr/pkg/include/libbson-*"
         "/opt/homebrew/include/bson-*"   "/opt/homebrew/include/libbson*"
      )
      FIND_LIBRARY(LIBBSON_LIBRARY NAMES bson-1.0 bson2 HINTS ${bsonSearchPaths})
      MESSAGE(" * Library: ${LIBBSON_LIBRARY}")
      FIND_PATH(LIBBSON_INCLUDE_DIR NAMES bson/bson.h HINTS ${bsonSearchPaths})
      MESSAGE(" * Includes: ${LIBBSON_INCLUDE_DIR}")
      IF ((NOT LIBBSON_INCLUDE_DIR) OR (NOT LIBBSON_LIBRARY))
         MESSAGE(FATAL_ERROR
                 "Cannot find MongoDB BSON library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libbson-dev\n"
                 " * Fedora:        sudo dnf install -y libbson-devel\n"
                 " * SuSE:          sudo zypper install -y bson-devel\n"
                 " * Alpine:        sudo apk add libbson-dev\n"
                 " * FreeBSD:       sudo pkg install -y libbson\n"
                 " * NetBSD:        sudo pkgin -y install mongo-c-driver\n"
                 " * OpenBSD:       (needs PPA or installation from sources!)\n"
                 " * Solaris:       (needs PPA or installation from sources!)\n"
                 " * Apple:         brew install mongo-c-driver\n"
                 "If MongoDB support is not required, it can be disabled by setting ENABLE_BACKEND_MONGODB=OFF (add -DENABLE_BACKEND_MONGODB=OFF to CMake options)!")
      ENDIF()
   ELSE()
      MESSAGE("Building without MongoDB backend (ENABLE_BACKEND_MONGODB=${ENABLE_BACKEND_MONGODB})")
      MESSAGE("MongoDB backend disabled!")
   ENDIF()

ENDIF()

# ====== Graphics tools for icons and logo ==================================
IF (WITH_ICONS)
   MESSAGE("Building with icons (WITH_ICONS=${WITH_ICONS})")

   # ------ Ghostscript -----------------------------------------------------
   FIND_PROGRAM(GS gs HINTS /bin /usr/bin /usr/local/bin /usr/pkg/bin)
   IF(GS)
      EXECUTE_PROCESS(COMMAND ${GS} -version
                      OUTPUT_VARIABLE GS_OUTPUT
                      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
      STRING(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+)" GS_VERSION "${GS_OUTPUT}")
      MESSAGE(STATUS "Ghostscript found: ${GS} (version ${GS_VERSION})")
   ELSE()
      MESSAGE(FATAL_ERROR
              "Cannot find Ghostscript! Try:\n"
              " * Ubuntu/Debian: sudo apt install -y ghostscript\n"
              " * Fedora:        sudo dnf install -y ghostscript\n"
              " * OpenSUSE:      sudo zypper install -y ghostscript\n"
              " * Alpine:        sudo apk add ghostscript\n"
              " * FreeBSD:       sudo pkg install -y ghostscript10\n"
              " * NetBSD:        sudo pkgin -y install ghostscript\n"
              " * OpenBSD:       sudo pkg_add ghostscript\n"
              " * Solaris:       sudo pkg install ghostscript\n"
              " * Apple:         brew install ghostscript\n"
              "If desktop icons are not required, they can be disabled by setting WITH_ICONS=OFF (add -DWITH_ICONS=OFF to CMake options)!")
   ENDIF()

   # ------ GraphicsMagick or ImageMagick -----------------------------------
   FIND_PROGRAM(GM gm magick HINTS /bin /usr/bin /usr/local/bin /usr/pkg/bin)
   IF(GM)
      EXECUTE_PROCESS(COMMAND ${GM} -version | head -n1
                      OUTPUT_VARIABLE GM_OUTPUT
                      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
      STRING(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+)" GM_VERSION "${GM_OUTPUT}")
      MESSAGE(STATUS "GraphicsMagick found: ${GM} (version ${GM_VERSION})")
   ELSE()
      MESSAGE(FATAL_ERROR
              "Cannot find GraphicsMagick! Try:\n"
              " * Ubuntu/Debian: sudo apt install -y graphicsmagick\n"
              " * Fedora:        sudo dnf install -y GraphicsMagick\n"
              " * OpenSUSE:      sudo zypper install -y GraphicsMagick\n"
              " * Alpine:        sudo apk add graphicsmagick\n"
              " * FreeBSD:       sudo pkg install -y GraphicsMagick\n"
              " * NetBSD:        sudo pkgin -y install GraphicsMagick\n"
              " * OpenBSD:       sudo pkg_add GraphicsMagick\n"
              " * Solaris:       sudo pkg install graphicsmagick\n"
              " * Apple:         brew install graphicsmagick\n"
              "If desktop icons are not required, they can be disabled by setting WITH_ICONS=OFF (add -DWITH_ICONS=OFF to CMake options)!")
   ENDIF()

   # ------ MuPDF mutool ----------------------------------------------------
   FIND_PROGRAM(MUTOOL mutool HINTS /bin /usr/bin /usr/local/bin /usr/pkg/bin)
   IF(MUTOOL)
      EXECUTE_PROCESS(COMMAND ${MUTOOL} -v
                      ERROR_VARIABLE  MUTOOL_OUTPUT
                      OUTPUT_VARIABLE MUTOOL_OUTPUT
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
      STRING(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+)" MUTOOL_VERSION "${MUTOOL_OUTPUT}")
      MESSAGE(STATUS "MuPDF mutool found: ${MUTOOL} (version ${MUTOOL_VERSION})")
   ELSE()
      MESSAGE(FATAL_ERROR
              "Cannot find mupdf! Try:\n"
              " * Ubuntu/Debian: sudo apt install -y mupdf-tools\n"
              " * Fedora:        sudo dnf install -y mupdf\n"
              " * OpenSUSE:      sudo zypper install -y mupdf\n"
              " * Alpine:        sudo apk add mupdf-tools\n"
              " * FreeBSD:       sudo pkg install -y mupdf\n"
              " * NetBSD:        sudo pkgin -y install mupdf\n"
              " * OpenBSD:       sudo pkg_add mupdf\n"
              " * Solaris:       (needs PPA or installation from sources!)\n"
              " * Apple:         brew install mupdf-tools\n"
              "If desktop icons are not required, they can be disabled by setting WITH_ICONS=OFF (add -DWITH_ICONS=OFF to CMake options)!")
   ENDIF()

ELSE()
   MESSAGE("Building without icons (WITH_ICONS=${WITH_ICONS})")
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(src)
