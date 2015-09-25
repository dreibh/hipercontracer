AC_INIT([HiPerConTracer], [0.0.0~alpha0.0], [dreib@simula.no])

AM_INIT_AUTOMAKE
AC_PREREQ(2.60)

AC_CONFIG_HEADERS([config.h:config.in])
AC_CONFIG_SRCDIR([src/hipercontracer.cc])

LT_INIT
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CXX


# ###### Compiler settings ##################################################
CXXFLAGS="$CXXFLAGS -std=c++11"


# ###### colorgcc ###########################################################
AC_ARG_ENABLE([colorgcc],
              [  --enable-colorgcc       enable colorgcc support ][default=no]],
              enable_colorgcc=$enableval, enable_colorgcc=no)
if test "${enable_colorgcc}" = "yes"; then
   AC_CHECK_PROG([COLORGCC], [colorgcc], [yes], [no])
   if test "$COLORGCC" == "yes"
   then
      AC_MSG_NOTICE([Compiling using colorgcc])
      CC=colorgcc
      CXX="colorgcc -lstdc++"
   fi
fi


# ###### Operating system checks ############################################
case $host_os in
freebsd*)
    ;;
linux*)
    ;;
openbsd*)
    ;;
solaris*)
    LDFLAGS="$LDFLAGS -lsocket -lnsl -lrt -lresolv"
    ;;
darwin*)
    ;;
esac


# ###### Makefiles ##########################################################
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
