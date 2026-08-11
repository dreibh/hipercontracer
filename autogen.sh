#!/usr/bin/env bash
#
# Build Scripts
# Copyright (C) 2002-2026 by Thomas Dreibholz
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
# Contact: thomas.dreibholz@gmail.com

# Bash options:
set -euo pipefail


cmakeOptions=""
makePrefix=""
cores=0
while [ $# -gt 0 ] ; do
   if [[ "$1" =~ ^(-|--)use-clang$ ]] ; then
      # Use these settings for CLang:
      export CXX=clang++
      export CC=clang
   elif [[ "$1" =~ ^(-|--)use-clang-scan-build$ ]] ; then
      # Use these settings for CLang:
      export CXX=clang++
      export CC=clang
      # Ensure build with CLang Static Analyzer
      mkdir -p scan-build-reports
      makePrefix="scan-build -o scan-build-reports"
   elif [[ "$1" =~ ^(-|--)use-gcc$ ]] ; then
      # Use these settings for GCC:
      export CXX=g++
      export CC=gcc
   elif [[ "$1" =~ ^(-|--)use-gcc-analyzer$ ]] ; then
      # Use these settings for GCC:
      export CXX=g++
      export CC=gcc
      export CFLAGS=-fanalyzer
      export CXXFLAGS=-fanalyzer
      cmakeOptions="${cmakeOptions} -DCMAKE_VERBOSE_MAKEFILE=ON"
   elif [[ "$1" =~ ^(-|--)debug$ ]] ; then
      # Enable debugging build:
      cmakeOptions="${cmakeOptions} -DCMAKE_BUILD_TYPE=Debug"
   elif [[ "$1" =~ ^(-|--)release$ ]] ; then
      # Enable debugging build:
      cmakeOptions="${cmakeOptions} -DCMAKE_BUILD_TYPE=Release"
   elif [[ "$1" =~ ^(-|--)release-with-debinfo$ ]] ; then
      # Enable debugging build:
      cmakeOptions="${cmakeOptions} -DCMAKE_BUILD_TYPE=RelWithDebInfo"
   elif [[ "$1" =~ ^(-|--)verbose$ ]] ; then
      # Enable verbose Makefile:
      cmakeOptions="${cmakeOptions} -DCMAKE_VERBOSE_MAKEFILE=ON"
   elif [[ "$1" =~ ^(-|--)cores ]] ; then
      if [[ ! "$2" =~ ^[0-9]+$ ]] ; then
         echo >&2 "ERROR: Number of cores must be an integer number!"
         exit 1
      fi
      cores="$2"
      shift
   elif [ "$1" == "--" ] ; then
      shift
      break
   else
      echo >&2 "Usage: autogen.sh [--use-clang|--use-clang-scan-build|--use-gcc|--use-gcc-analyzer] [--debug|--release|--release-with-debinfo] [--cores N] [--verbose] -- (further CMake/Configure options)"
      exit 1
   fi
   shift
done


# ====== Obtain installation path prefix ====================================
uname="$(uname)"
case "${uname}" in
   Linux|SunOS)
      installPrefix="/usr"
      ;;
   NetBSD)
      installPrefix="/usr/pkg"
      ;;
   *)
      installPrefix="/usr/local"
      ;;
esac


# ====== Obtain number of cores =============================================
if [ "${cores}" -lt 1 ] ; then
   uname="$(uname)"
   case "${uname}" in
      Linux)
         cores="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo "1")"
         ;;
      FreeBSD)
         cores="$(sysctl -n hw.ncpu 2>/dev/null || echo "1")"
         ;;
      NetBSD|OpenBSD)
         cores="$(/sbin/sysctl -n hw.ncpuonline 2>/dev/null || echo "1")"
         ;;
      SunOS)
         cores="$(getconf NPROCESSORS_ONLN 2>/dev/null || echo "1")"
         ;;
      Darwin)
         cores="$(sysctl -n hw.logicalcpu 2>/dev/null || echo "1")"
         ;;
      *)
         cores=1
         ;;
   esac
fi
if [ "${cores}" -lt 1 ] ; then
   cores="1"
fi


# ====== Configure with CMake ===============================================
if [ -e CMakeLists.txt ] ; then
   rm -f CMakeCache.txt
   if [ $# -gt 0 ] ; then
      cmakeOptions="${cmakeOptions} $*"
   fi
   echo "CMake options:${cmakeOptions} . -DCMAKE_INSTALL_PREFIX=\"${installPrefix}\""
   # shellcheck disable=SC2048,SC2086
   ${makePrefix} cmake ${cmakeOptions} . -DCMAKE_INSTALL_PREFIX="${installPrefix}"

# ====== Configure with AutoConf/AutoMake ===================================
elif [ -e bootstrap ] ; then
   ./bootstrap
   # shellcheck disable=SC2068
   ./configure $@

else
   echo >&2 "ERROR: Failed to configure with CMake or AutoMake/AutoConf!"
   exit 1
fi


# ====== Build ==============================================================
echo "Starting build using up to ${cores} cores ..."
${makePrefix} make -j "${cores}"
