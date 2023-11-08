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

#############################################################################
#### VERSION FILE                                                        ####
#############################################################################

CONFIGURE_FILE (
   "${CMAKE_CURRENT_SOURCE_DIR}/package-version.h.in"
   "${CMAKE_CURRENT_SOURCE_DIR}/package-version.h"
)


#############################################################################
#### LIBRARIES                                                           ####
#############################################################################

# ====== libhipercontracer ==================================================
LIST(APPEND libhipercontracer_headers
   destinationinfo.h
   iomodule-base.h
   iomodule-icmp.h
   iomodule-udp.h
   jitter.h
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
   internet16.cc
   iomodule-base.cc
   iomodule-icmp.cc
   iomodule-udp.cc
   jitter.cc
   jitter-rfc3550.cc
   logger.cc
   ping.cc
   resultentry.cc
   resultswriter.cc
   service.cc
   tools.cc
   traceroute.cc
   traceserviceheader.cc
)

INSTALL(FILES ${libhipercontracer_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hipercontracer)

IF (NOT STATIC_BUILD)
   ADD_LIBRARY(libhipercontracer-shared SHARED ${libhipercontracer_sources})
ENDIF()
ADD_LIBRARY(libhipercontracer-static STATIC ${libhipercontracer_sources})
FOREACH(TYPE shared;static)
   IF (("${TYPE}" STREQUAL "static") OR (NOT STATIC_BUILD))
      SET_TARGET_PROPERTIES(libhipercontracer-${TYPE} PROPERTIES OUTPUT_NAME hipercontracer CLEAN_DIRECT_OUTPUT 1)
      SET_TARGET_PROPERTIES(libhipercontracer-${TYPE} PROPERTIES
         VERSION   ${BUILD_VERSION}
         SOVERSION ${BUILD_MAJOR}
      )
      TARGET_LINK_LIBRARIES (libhipercontracer-${TYPE} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
      INSTALL(TARGETS libhipercontracer-${TYPE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
   ENDIF()
ENDFOREACH()

# ====== libuniversalimporter ==================================================
IF (WITH_IMPORTER)
   LIST(APPEND libuniversalimporter_headers
      logger.h
      tools.h
      database-configuration.h
      database-statement.h
      databaseclient-base.h
      databaseclient-debug.h
      reader-base.h
      results-exception.h
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
      reader-base.cc
      universal-importer.cc
      worker.cc
   )

   IF (ENABLE_BACKEND_MARIADB)
      LIST(APPEND libuniversalimporter_headers databaseclient-mariadb.h)
      LIST(APPEND libuniversalimporter_sources databaseclient-mariadb.cc)
   ENDIF()
   IF (ENABLE_BACKEND_POSTGRESQL)
      LIST(APPEND libuniversalimporter_headers databaseclient-postgresql.h)
      LIST(APPEND libuniversalimporter_sources databaseclient-postgresql.cc)
   ENDIF()
   IF (ENABLE_BACKEND_MONGODB)
      LIST(APPEND libuniversalimporter_headers databaseclient-mongodb.h)
      LIST(APPEND libuniversalimporter_sources databaseclient-mongodb.cc)
   ENDIF()


   INSTALL(FILES ${libuniversalimporter_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/universalimporter)

   IF (NOT STATIC_BUILD)
      ADD_LIBRARY(libuniversalimporter-shared SHARED ${libuniversalimporter_sources})
   ENDIF()
   ADD_LIBRARY(libuniversalimporter-static STATIC ${libuniversalimporter_sources})
   FOREACH(TYPE shared;static)
      IF (("${TYPE}" STREQUAL "static") OR (NOT STATIC_BUILD))
         SET_TARGET_PROPERTIES(libuniversalimporter-${TYPE} PROPERTIES OUTPUT_NAME universalimporter CLEAN_DIRECT_OUTPUT 1)
         SET_TARGET_PROPERTIES(libuniversalimporter-${TYPE} PROPERTIES
            VERSION   ${BUILD_VERSION}
            SOVERSION ${BUILD_MAJOR}
         )
         TARGET_INCLUDE_DIRECTORIES(libuniversalimporter-${TYPE} PUBLIC ${MYSQLCPPCONN_INCLUDE_DIR} ${LIBMONGOC_INCLUDE_DIR} ${LIBBSON_INCLUDE_DIR} ${INotify_INCLUDE_DIRS})
         TARGET_LINK_LIBRARIES(libuniversalimporter-${TYPE} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${MYSQLCPPCONN_LIBRARY} ${LIBPQXX_LIBRARY} ${LIBMONGOC_LIBRARY} ${LIBBSON_LIBRARY} ${INotify_LIBRARIES})
         INSTALL(TARGETS libuniversalimporter-${TYPE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
      ENDIF()
   ENDFOREACH()
ENDIF()


#############################################################################
#### PROGRAMS                                                            ####
#############################################################################

IF (STATIC_BUILD)
   SET(libraryType static)
ELSE()
   SET(libraryType shared)
ENDIF()

# ====== HiPerConTracer =====================================================
ADD_EXECUTABLE(hipercontracer hipercontracer.cc)
TARGET_LINK_LIBRARIES(hipercontracer libhipercontracer-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -llzma -lbz2 -lz)
INSTALL(TARGETS hipercontracer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES hipercontracer.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# ====== HiPerConTracer Trigger =============================================
IF (WITH_TRIGGER)
   ADD_EXECUTABLE(hpct-trigger hpct-trigger.cc)
   TARGET_LINK_LIBRARIES(hpct-trigger libhipercontracer-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -llzma -lbz2 -lz)
   INSTALL(TARGETS hpct-trigger RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-trigger.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== HiPerConTracer Importer ============================================
IF (WITH_IMPORTER)
   ADD_EXECUTABLE(hpct-importer hpct-importer.cc
      conversions.cc
      reader-ping.cc
      reader-traceroute.cc
   )
   TARGET_LINK_LIBRARIES(hpct-importer libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -llzma -lbz2 -lz)
   INSTALL(TARGETS hpct-importer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-importer.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES hpct-importer.service DESTINATION /lib/systemd/system)
   INSTALL(FILES hpct-importer.conf    DESTINATION /etc/hipercontracer)
ENDIF()

# ====== HiPerConTracer Query Tool ==========================================
IF (WITH_QUERY)
   ADD_EXECUTABLE(hpct-query hpct-query.cc)
   TARGET_LINK_LIBRARIES(hpct-query libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -llzma -lbz2 -lz)
   INSTALL(TARGETS hpct-query
           DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-query.1
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== HiPerConTracer Results Tool ========================================
IF (WITH_RESULTS)
   ADD_EXECUTABLE(hpct-results hpct-results.cc conversions.cc)
   TARGET_LINK_LIBRARIES(hpct-results libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -llzma -lbz2 -lz)
   INSTALL(TARGETS hpct-results RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-results.1
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== Pipe Checksum Tool =================================================
IF (WITH_PIPE_CHECKSUM)
   ADD_EXECUTABLE(pipe-checksum pipe-checksum.cc)
   TARGET_LINK_LIBRARIES(pipe-checksum ${CRYPTO_LIBRARY})
   INSTALL(TARGETS pipe-checksum RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES pipe-checksum.1
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== UDP Echo Server ====================================================
IF (WITH_UDP_ECHO_SERVER)
   ADD_EXECUTABLE(udp-echo-server udp-echo-server.cc logger.cc tools.cc)
   TARGET_LINK_LIBRARIES(udp-echo-server ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -llzma -lbz2 -lz)
   INSTALL(TARGETS udp-echo-server
           DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES udp-echo-server.1
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES udp-echo-server.service DESTINATION /lib/systemd/system)
   INSTALL(FILES udp-echo-server.conf    DESTINATION /etc/hipercontracer)
ENDIF()

# ====== Scripts ============================================================
INSTALL(PROGRAMS get-default-ips
        DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES get-default-ips.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


# ====== TEST ONLY ==========================================================
# ADD_EXECUTABLE(t1 t1.cc)
# TARGET_LINK_LIBRARIES(t1 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(t2 t2.cc)
# TARGET_LINK_LIBRARIES(t2 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CARES_LIBRARY})
# ADD_EXECUTABLE(t3 t3.cc)
# ADD_EXECUTABLE(t4 t4.cc)
# TARGET_LINK_LIBRARIES(t4 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(t9 t9.cc)
# TARGET_LINK_LIBRARIES(t9 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(t9b t9b.cc)
# TARGET_LINK_LIBRARIES(t9b ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# TARGET_LINK_LIBRARIES(t3 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(t7 t7.cc)
# TARGET_LINK_LIBRARIES(t7 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES hipercontracer-database.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples)


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

IF (WITH_IMPORTER)
   ADD_SUBDIRECTORY(SQL)
   ADD_SUBDIRECTORY(NoSQL)
   ADD_SUBDIRECTORY(TestDB)
   ADD_SUBDIRECTORY(examples)
ENDIF()
