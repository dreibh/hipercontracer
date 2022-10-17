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
#  Copyright (C) 2015-2022 by Thomas Dreibholz
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


#############################################################################
# LIBRARIES
#############################################################################

# ====== liblibhipercontracer ==============================================
LIST(APPEND libhipercontracer_headers
   destinationinfo.h
   logger.h
   ping.h
   resultentry.h
   resultswriter.h
   service.h
   tools.h
   traceroute.h
)
LIST(APPEND libhipercontracer_sources
   destinationinfo.cc
   logger.cc
   ping.cc
   resultentry.cc
   resultswriter.cc
   service.cc
   traceroute.cc
   tools.cc
)

INSTALL(FILES ${libhipercontracer_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hipercontracer)

ADD_LIBRARY(libhipercontracer-shared SHARED ${libhipercontracer_sources})
ADD_LIBRARY(libhipercontracer-static STATIC ${libhipercontracer_sources})
FOREACH(TYPE shared;static)
   SET_TARGET_PROPERTIES(libhipercontracer-${TYPE} PROPERTIES OUTPUT_NAME hipercontracer CLEAN_DIRECT_OUTPUT 1)
   SET_TARGET_PROPERTIES(libhipercontracer-${TYPE} PROPERTIES
      VERSION   ${BUILD_VERSION}
      SOVERSION ${BUILD_MAJOR}
   )
   TARGET_LINK_LIBRARIES (libhipercontracer-${TYPE} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS libhipercontracer-${TYPE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
ENDFOREACH()


#############################################################################
# PROGRAMS
#############################################################################

ADD_EXECUTABLE(hipercontracer hipercontracer.cc)
TARGET_LINK_LIBRARIES(hipercontracer libhipercontracer-shared ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
INSTALL(TARGETS hipercontracer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES hipercontracer.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

ADD_EXECUTABLE(hpcttrigger hpcttrigger.cc)
TARGET_LINK_LIBRARIES(hpcttrigger libhipercontracer-shared ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
INSTALL(TARGETS hpcttrigger RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES hpcttrigger.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

INSTALL(PROGRAMS tracedataimporter get-default-ips addressinfogenerator
        DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES tracedataimporter.1 get-default-ips.1 addressinfogenerator.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


# Test only:
# ADD_EXECUTABLE(t1 t1.cc)
# TARGET_LINK_LIBRARIES(t1 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ADD_EXECUTABLE(t2 t2.cc)
TARGET_LINK_LIBRARIES(t2 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CARES_LIBRARY})
ADD_EXECUTABLE(t3 t3.cc)
TARGET_LINK_LIBRARIES(t3 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


#############################################################################
# EXAMPLES
#############################################################################

INSTALL(FILES hipercontracer-database-configuration
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples)


#############################################################################
# SUBDIRECTORIES
#############################################################################

ADD_SUBDIRECTORY(SQL)
ADD_SUBDIRECTORY(NoSQL)
ADD_SUBDIRECTORY(examples)
