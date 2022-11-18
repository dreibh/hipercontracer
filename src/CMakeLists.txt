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

# ====== libhipercontracer ==================================================
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

# ====== libuniversalimporter ==================================================
LIST(APPEND libuniversalimporter_headers
   logger.h
   tools.h
   database-configuration.h
   database-statement.h
   databaseclient-base.h
   databaseclient-debug.h
   databaseclient-mariadb.h
   databaseclient-mongodb.h
   databaseclient-postgresql.h
   reader-base.h
   reader-ping.h
   reader-traceroute.h
   universal-importer.h
   worker.h
)
LIST(APPEND libuniversalimporter_sources
   logger.cc
   tools.cc
   database-configuration.cc
   database-statement.cc
   databaseclient-base.cc
   databaseclient-debug.cc
   databaseclient-mariadb.cc
   databaseclient-mongodb.cc
   databaseclient-postgresql.cc
   reader-base.cc
   reader-ping.cc
   reader-ping.cc
   reader-traceroute.cc
   universal-importer.cc
   worker.cc
)

INSTALL(FILES ${libuniversalimporter_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/universalimporter)

ADD_LIBRARY(libuniversalimporter-shared SHARED ${libuniversalimporter_sources})
ADD_LIBRARY(libuniversalimporter-static STATIC ${libuniversalimporter_sources})
FOREACH(TYPE shared;static)
   SET_TARGET_PROPERTIES(libuniversalimporter-${TYPE} PROPERTIES OUTPUT_NAME universalimporter CLEAN_DIRECT_OUTPUT 1)
   SET_TARGET_PROPERTIES(libuniversalimporter-${TYPE} PROPERTIES
      VERSION   ${BUILD_VERSION}
      SOVERSION ${BUILD_MAJOR}
   )
   TARGET_INCLUDE_DIRECTORIES(libuniversalimporter-${TYPE} PUBLIC ${LIBMONGOC_INCLUDE_DIR} ${LIBBSON_INCLUDE_DIR})
   TARGET_LINK_LIBRARIES(libuniversalimporter-${TYPE} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${MYSQLCPPCONN_LIBRARY} ${LIBPQXX_LIBRARY} ${LIBMONGOC_LIBRARY})
   INSTALL(TARGETS libuniversalimporter-${TYPE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
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

ADD_EXECUTABLE(trace-importer trace-importer.cc
   reader-ping.cc
   reader-traceroute.cc
)
TARGET_LINK_LIBRARIES(trace-importer libuniversalimporter-shared)
INSTALL(TARGETS trace-importer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# INSTALL(FILES trace-importer.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


INSTALL(PROGRAMS tracedataimporter get-default-ips addressinfogenerator
        DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES tracedataimporter.1 get-default-ips.1 addressinfogenerator.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


# Test only:
# ADD_EXECUTABLE(t1 t1.cc)
# TARGET_LINK_LIBRARIES(t1 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(t2 t2.cc)
# TARGET_LINK_LIBRARIES(t2 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CARES_LIBRARY})
# ADD_EXECUTABLE(t3 t3.cc)
# TARGET_LINK_LIBRARIES(t3 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(t7 t7.cc)
# TARGET_LINK_LIBRARIES(t7 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

ADD_EXECUTABLE(nne-importer nne-importer.cc
   reader-nne-metadata.cc
   reader-nne-ping.cc
   reader-nne-speedtest.cc
)
TARGET_LINK_LIBRARIES(nne-importer libuniversalimporter-shared)


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
