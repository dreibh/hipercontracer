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
#  Copyright (C) 2015-2018 by Thomas Dreibholz
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


CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(hipercontracer)

SET(BUILD_MAJOR "1")
SET(BUILD_MINOR "1")
SET(BUILD_PATCH "3")
SET(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH})


#################################################
# INSTALLATION_DIRECTORIES
#################################################

SET(CMAKE_INSTALL_BINDIR_DOC        "user executables [PREFIX/bin]")
SET(CMAKE_INSTALL_SBINDIR_DOC       "system admin executables [EPREFIX/sbin]")
SET(CMAKE_INSTALL_DATADIR_DOC       "read-only architecture-independent data [PREFIX/share]")
SET(CMAKE_INSTALL_SYSCONFDIR_DOC    "read-only single-machine data [PREFIX/etc]")
SET(CMAKE_INSTALL_LOCALSTATEDIR_DOC "modifiable single-machine data [PREFIX/var]")
SET(CMAKE_INSTALL_LIBDIR_DOC        "object code libraries [PREFIX/lib]")
SET(CMAKE_INSTALL_INCLUDEDIR_DOC    "read-only includes [PREFIX/include]")
SET(CMAKE_INSTALL_MANDIR_DOC        "man documentation [PREFIX/share/man]")

SET(CMAKE_INSTALL_BINDIR "bin"
    CACHE PATH ${CMAKE_INSTALL_BINDIR_DOC})
SET(CMAKE_INSTALL_SBINDIR "sbin"
    CACHE PATH ${CMAKE_INSTALL_SBINDIR_DOC})
SET(CMAKE_INSTALL_DATADIR "share"
    CACHE PATH ${CMAKE_INSTALL_DATADIR_DOC})
SET(CMAKE_INSTALL_SYSCONFDIR "etc"
    CACHE PATH ${CMAKE_INSTALL_SYSCONFDIR_DOC})
SET(CMAKE_INSTALL_LOCALSTATEDIR "var"
    CACHE PATH ${CMAKE_INSTALL_LOCALSTATEDIR_DOC})
SET(CMAKE_INSTALL_LIBDIR "lib"
    CACHE PATH ${CMAKE_INSTALL_LIBDIR_DOC})
SET(CMAKE_INSTALL_INCLUDEDIR "include"
    CACHE PATH ${CMAKE_INSTALL_INCLUDEDIR_DOC})
SET(CMAKE_INSTALL_MANDIR "share/man"
    CACHE PATH ${CMAKE_INSTALL_MANDIR_DOC})


#################################################
# PACKAGING
#################################################

SET(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME
   "${CMAKE_PROJECT_NAME}-${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_PATCH}")
SET(CPACK_SOURCE_IGNORE_FILES
   "${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}-*;${PROJECT_SOURCE_DIR}/${CMAKE_PROJECT_NAME}_*;/build/;/.git/;/config.log;/CMakeFiles/;CMakeCache.txt$;.cmake$;.tar.gz$;/_CPack_Packages;/Makefile$;~$;/build-deb;/clean-deb;/filter-empty-entries;/make-symbols;/make-ppa;/make-deb;/debian.conf;/make-rpm;/rpm.conf;${CPACK_SOURCE_IGNORE_FILES}")
INCLUDE(CPack)

ADD_CUSTOM_TARGET(dist COMMAND ${CMAKE_MAKE_PROGRAM} clean package_source)


INCLUDE(CheckIncludeFile)
INCLUDE(CheckStructHasMember)


#################################################
# OS DEPENDENT
#################################################

SET(CMAKE_MACOSX_RPATH 1)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    ADD_DEFINITIONS("-D_DEFAULT_SOURCE -DLINUX")
    FIND_LIBRARY(SCTP_LIB sctp)

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/local/include")
    ADD_DEFINITIONS("-DFreeBSD")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include" "/usr/local/opt/openssl/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/local/include" "/usr/local/opt/openssl/include")
    ADD_DEFINITIONS("-DDARWIN")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    LIST(APPEND neat_headers neat_bsd.h)
    LIST(APPEND neat_sources neat_bsd.c)
    SET(CMAKE_REQUIRED_INCLUDES "/usr/pkg/include" "/usr/include" "/usr/local/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/pkg/include" "/usr/local/include")
    ADD_DEFINITIONS("-DNetBSD")

ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    MESSAGE(STATUS ${CMAKE_SYSTEM_NAME} " supported")
    ADD_DEFINITIONS("-DSOLARIS")

ELSE()
    MESSAGE(FATAL_ERROR ${CMAKE_SYSTEM_NAME} " not supported (yet?)")

ENDIF()

FIND_PACKAGE(Threads REQUIRED)


#################################################
# CHECK STRUCT MEMBERS
#################################################

CHECK_STRUCT_HAS_MEMBER("struct sockaddr" "sa_len" "sys/types.h;sys/socket.h" HAVE_SA_LEN)
IF (HAVE_SA_LEN)
    MESSAGE(STATUS "HAVE_SA_LEN")
    ADD_DEFINITIONS(-DHAVE_SA_LEN)
ENDIF()

CHECK_STRUCT_HAS_MEMBER("struct sockaddr_in" "sin_len" "sys/types.h;netinet/in.h" HAVE_SIN_LEN)
IF (HAVE_SIN_LEN)
    MESSAGE(STATUS "HAVE_SIN_LEN")
    ADD_DEFINITIONS(-DHAVE_SIN_LEN)
ENDIF()

CHECK_STRUCT_HAS_MEMBER("struct sockaddr_in6" "sin6_len" "sys/types.h;netinet/in.h" HAVE_SIN6_LEN)
IF (HAVE_SIN6_LEN)
    MESSAGE(STATUS "HAVE_SIN6_LEN")
    ADD_DEFINITIONS(-DHAVE_SIN6_LEN)
ENDIF()

CHECK_STRUCT_HAS_MEMBER("struct sockaddr_storage" "ss_len" "sys/types.h;sys/socket.h" HAVE_SS_LEN)
IF (HAVE_SS_LEN)
    MESSAGE(STATUS "HAVE_SS_LEN")
    ADD_DEFINITIONS(-DHAVE_SS_LEN)
ENDIF()


#################################################
# CHECK OPTIONS
#################################################

# Debug build type as default
IF (NOT CMAKE_BUILD_TYPE)
    MESSAGE(STATUS "No build type selected, using DEBUG")
    SET(CMAKE_BUILD_TYPE "DEBUG")
    # _FORTIFY_SOURCE requires compiling with optimization
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O")
ENDIF()


#################################################
# REQUIREMENTS
#################################################

# ###### Boost ##############################################################
FIND_PACKAGE(Boost REQUIRED COMPONENTS date_time system thread iostreams filesystem)
IF (NOT Boost_FOUND)
   MESSAGE(FATAL_ERROR "Cannot find Boost libraries!")
ELSE()
  INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
ENDIF()
ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)   # <<-- Required to handle BOOST Log link problem!


#################################################
# COMPILER FLAGS
#################################################

SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -D_FORTIFY_SOURCE=2 -O2 -Wformat -Wformat-security")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -D_FORTIFY_SOURCE=2 -O2 -Wformat -Wformat-security")

IF (CMAKE_BUILD_TYPE MATCHES DEBUG)
    MESSAGE(STATUS "Building in DEBUG mode")
    # _FORTIFY_SOURCE requires compiling with optimization
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O")
ENDIF()


#################################################
# SUBDIRECTORIES
#################################################

ADD_SUBDIRECTORY(src)
