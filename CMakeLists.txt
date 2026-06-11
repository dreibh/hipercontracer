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
SET(BUILD_PATCH "3~rc0")
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
#### OS DEPENDENT                                                        ####
#############################################################################

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "NetBSD")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
   MESSAGE(STATUS "${CMAKE_SYSTEM_NAME} supported")
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
ELSE()
   MESSAGE(FATAL_ERROR "Cannot find BOOST libraries! Try:\n"
           " * Ubuntu/Debian: sudo apt install -y libboost-all-dev\n"
           " * Fedora:        sudo dnf install -y boost-devel\n"
           " * SuSE:          sudo zypper install -y boost-devel\n"
           " * Alpine:        sudo apk add boost-dev\n"
           " * FreeBSD:       sudo pkg install -y boost\n"
           " * NetBSD:        sudo pkgin -y install boost\n"
           " * OpenBSD:       sudo pkg_add boost\n"
           " * Apple:         brew install boost")
ENDIF()
IF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
   ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)   # <<-- Required to handle BOOST Log link problem!
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
               " * Apple:         --- N/A ---")
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
      FIND_LIBRARY(LIBMARIADB_LIBRARY NAMES mariadb  PATH_SUFFIXES mariadb mysql)
      MESSAGE(" * Library: ${LIBMARIADB_LIBRARY}")
      FIND_PATH(LIBMARIADB_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mariadb mysql)
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
                 " * Apple:         brew install mariadb-connector-c")
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
                 " * Apple:         brew install libpqxx")
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
      FIND_LIBRARY(LIBMONGOC_LIBRARY NAMES mongoc-1.0 mongoc2)
      MESSAGE(" * Library: ${LIBMONGOC_LIBRARY}")
      FILE(GLOB mongoSearchPaths
         "/usr/include/mongoc-*"       "/usr/include/libmongoc*"
         "/usr/local/include/mongoc-*" "/usr/local/include/libmongoc*"
      )
      FIND_PATH(LIBMONGOC_INCLUDE_DIR NAMES mongoc/mongoc.h HINTS ${mongoSearchPaths})
      MESSAGE(" * Includes: ${LIBMONGOC_INCLUDE_DIR}")
      IF ((NOT LIBMONGOC_INCLUDE_DIR) OR (NOT LIBMONGOC_LIBRARY))
         MESSAGE(FATAL_ERROR
                 "Cannot find MongoDB C Client Library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libmongoc-dev\n"
                 " * Fedora:        sudo dnf install -y mongo-c-driver-devel\n"
                 " * SuSE:          (needs PPA or installation from sources!)\n"
                 " * Alpine:        sudo apk add mongo-c-driver-dev\n"
                 " * FreeBSD:       sudo pkg install -y mongo-c-driver\n"
                 " * NetBSD:        sudo pkgin -y install mongo-c-driver\n"
                 " * OpenBSD:       sudo pkg_add ????????\n"
                 " * Apple:         brew install mongo-c-driver")
      ENDIF()

      MESSAGE("Looking for MongoDB BSON library:")
      FIND_LIBRARY(LIBBSON_LIBRARY NAMES bson-1.0 bson2)
      MESSAGE(" * Library: ${LIBBSON_LIBRARY}")
      FILE(GLOB bsonSearchPaths
         "/usr/include/bson-*"       "/usr/include/libbson-*"
         "/usr/local/include/bson-*" "/usr/local/include/libbson-*"
      )
      FIND_PATH(LIBBSON_INCLUDE_DIR NAMES bson/bson.h HINTS ${bsonSearchPaths})
      MESSAGE(" * Includes: ${LIBBSON_INCLUDE_DIR}")
      IF ((NOT LIBBSON_INCLUDE_DIR) OR (NOT LIBBSON_LIBRARY))
         MESSAGE(FATAL_ERROR
                 "Cannot find MongoDB BSON library! Try:\n"
                 " * Ubuntu/Debian: sudo apt install -y libbson-dev\n"
                 " * Fedora:        sudo dnf install -y libbson-devel\n"
                 " * SuSE:          (needs PPA or installation from sources!)\n"
                 " * Alpine:        sudo apk add libbson-dev\n"
                 " * FreeBSD:       sudo pkg install -y libbson\n"
                 " * NetBSD:        sudo pkgin -y install ????????\n"
                 " * OpenBSD:       sudo pkg_add ????????\n"
                 " * Apple:         brew install mongo-c-driver")
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
   FIND_PROGRAM(GS gs HINTS /usr/pkg/bin)
   IF(GS)
      EXECUTE_PROCESS(COMMAND ${GS} --version
                      OUTPUT_VARIABLE GS_OUTPUT
                      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
      STRING(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+)" GS_VERSION "${GS_OUTPUT}")
      MESSAGE(STATUS "Ghostscript found: ${GS} (version ${GS_VERSION})")
   ELSE()
      MESSAGE(FATAL_ERROR
            "Cannot find Ghostscript! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y ghostscript\n"
            " * Fedora:        sudo dnf install -y ghostscript\n"
            " * SuSE:          sudo zypper install -y ghostscript\n"
            " * Alpine:        sudo apk add ghostscript\n"
            " * FreeBSD:       sudo pkg install -y ghostscript10\n"
            " * NetBSD:        sudo pkgin -y install ghostscript\n"
            " * OpenBSD:       sudo pkg_add ghostscript\n"
            " * Apple:         brew install ghostscript")
   ENDIF()

   # ------ GraphicsMagick --------------------------------------------------
   FIND_PROGRAM(GM gm HINTS /usr/pkg/bin)
   IF(GM)
      EXECUTE_PROCESS(COMMAND ${GM} version | head -n1
                      OUTPUT_VARIABLE GM_OUTPUT
                      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
      STRING(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+)" GM_VERSION "${GM_OUTPUT}")
      MESSAGE(STATUS "GraphicsMagick found: ${GM} (version ${GM_VERSION})")
   ELSE()
      MESSAGE(FATAL_ERROR
            "Cannot find GraphicsMagick! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y graphicsmagick\n"
            " * Fedora:        sudo dnf install -y GraphicsMagick\n"
            " * SuSE:          sudo zypper install -y GraphicsMagick\n"
            " * Alpine:        sudo apk add graphicsmagick\n"
            " * FreeBSD:       sudo pkg install -y GraphicsMagick\n"
            " * NetBSD:        sudo pkgin -y install GraphicsMagick\n"
            " * OpenBSD:       sudo pkg_add GraphicsMagick\n"
            " * Apple:         brew install graphicsmagick")
   ENDIF()

   # ------ pdftocairo ------------------------------------------------------
   FIND_PROGRAM(PDFTOCAIRO pdftocairo HINTS /usr/pkg/bin)
   IF(PDFTOCAIRO)
      EXECUTE_PROCESS(COMMAND ${PDFTOCAIRO} -v 2>&1 | head -n1
                      ERROR_VARIABLE PDFTOCAIRO_OUTPUT
                      OUTPUT_QUIET ERROR_STRIP_TRAILING_WHITESPACE)
      STRING(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+|[0-9]+\\.[0-9]+)" PDFTOCAIRO_VERSION "${PDFTOCAIRO_OUTPUT}")
      MESSAGE(STATUS "pdftocairo found: ${PDFTOCAIRO} (version ${PDFTOCAIRO_VERSION})")
   ELSE()
      MESSAGE(FATAL_ERROR
            "Cannot find pdftocairo! Try:\n"
            " * Ubuntu/Debian: sudo apt install -y poppler-utils\n"
            " * Fedora:        sudo dnf install -y poppler-utils\n"
            " * SuSE:          sudo zypper install -y poppler-utils\n"
            " * Alpine:        sudo apk add ghostscript\n"
            " * FreeBSD:       sudo pkg install -y poppler-utils\n"
            " * NetBSD:        sudo pkgin -y install poppler-utils\n"
            " * OpenBSD:       (needs installation from sources!)\n"
            " * Apple:         brew install poppler")
   ENDIF()

ELSE()
   MESSAGE("Building without icons (WITH_ICONS=${WITH_ICONS})")
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(src)
