#!/bin/bash -e

rm -f CMakeCache.txt

# ------ Obtain number of cores ---------------------------------------------
# Try Linux
cores=`getconf _NPROCESSORS_ONLN 2>/dev/null || true`
if [ "$cores" == "" ] ; then
   # Try FreeBSD
   cores=`sysctl -a | grep 'hw.ncpu' | cut -d ':' -f2 | tr -d ' '`
fi
if [ "$cores" == "" ] ; then
   cores="1"
fi

# ------ Configure and build ------------------------------------------------
cmake -DCMAKE_INSTALL_PREFIX=/usr $@ .
make -j$cores
