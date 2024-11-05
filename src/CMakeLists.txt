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

#############################################################################
#### VERSION FILE                                                        ####
#############################################################################

CONFIGURE_FILE (
   "${CMAKE_CURRENT_SOURCE_DIR}/package-version.h.in"
   "${CMAKE_CURRENT_BINARY_DIR}/package-version.h"
)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})


#############################################################################
#### LIBRARIES                                                           ####
#############################################################################

# ====== libhipercontracer ==================================================
IF (WITH_HIPERCONTRACER)
   LIST(APPEND libhipercontracer_headers
      check.h
      destinationinfo.h
      iomodule-base.h
      iomodule-icmp.h
      iomodule-udp.h
      # jitter.h
      logger.h
      ping.h
      resultentry.h
      resultswriter.h
      service.h
      tools.h
      traceroute.h
   )
   LIST(APPEND libhipercontracer_sources
      assure.cc
      check.cc
      destinationinfo.cc
      internet16.cc
      iomodule-base.cc
      iomodule-icmp.cc
      iomodule-udp.cc
      # jitter.cc
      # jitter-rfc3550.cc
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

   IF (WITH_STATIC_LIBRARIES)
      ADD_LIBRARY(libhipercontracer-static STATIC ${libhipercontracer_sources})
   ENDIF()
   IF (WITH_SHARED_LIBRARIES)
      ADD_LIBRARY(libhipercontracer-shared SHARED ${libhipercontracer_sources})
   ENDIF()
   FOREACH(TYPE shared;static)
      IF ( (("${TYPE}" STREQUAL "static") AND (WITH_STATIC_LIBRARIES)) OR
         (("${TYPE}" STREQUAL "shared") AND (WITH_SHARED_LIBRARIES)) )
         SET_TARGET_PROPERTIES(libhipercontracer-${TYPE} PROPERTIES OUTPUT_NAME hipercontracer CLEAN_DIRECT_OUTPUT 1)
         SET_TARGET_PROPERTIES(libhipercontracer-${TYPE} PROPERTIES
            VERSION   ${BUILD_VERSION}
            SOVERSION ${BUILD_MAJOR}
         )
         TARGET_LINK_LIBRARIES (libhipercontracer-${TYPE} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LZMA_LIBRARY} ${BZ2_LIBRARY} ${Z_LIBRARY})
         INSTALL(TARGETS libhipercontracer-${TYPE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
      ENDIF()
   ENDFOREACH()
ENDIF()

# ====== libuniversalimporter ==================================================
IF (WITH_IMPORTER)
   LIST(APPEND libuniversalimporter_headers
      logger.h
      tools.h
      database-configuration.h
      database-statement.h
      databaseclient-base.h
      importer-configuration.h
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
      importer-configuration.cc
      reader-base.cc
      universal-importer.cc
      worker.cc
   )

   IF (ENABLE_BACKEND_DEBUG)
      LIST(APPEND libuniversalimporter_headers databaseclient-debug.h)
      LIST(APPEND libuniversalimporter_sources databaseclient-debug.cc)
      SET_PROPERTY(SOURCE database-configuration.cc APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_DEBUG)
   ENDIF()
   IF (ENABLE_BACKEND_MARIADB)
      LIST(APPEND libuniversalimporter_headers databaseclient-mariadb.h)
      LIST(APPEND libuniversalimporter_sources databaseclient-mariadb.cc)
      SET_PROPERTY(SOURCE database-configuration.cc APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_MARIADB)
   ENDIF()
   IF (ENABLE_BACKEND_POSTGRESQL)
      LIST(APPEND libuniversalimporter_headers databaseclient-postgresql.h)
      LIST(APPEND libuniversalimporter_sources databaseclient-postgresql.cc)
      SET_PROPERTY(SOURCE database-configuration.cc APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_POSTGRESQL)
   ENDIF()
   IF (ENABLE_BACKEND_MONGODB)
      LIST(APPEND libuniversalimporter_headers databaseclient-mongodb.h)
      LIST(APPEND libuniversalimporter_sources databaseclient-mongodb.cc)
      SET_PROPERTY(SOURCE database-configuration.cc APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_MONGODB)
   ENDIF()


   INSTALL(FILES ${libuniversalimporter_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/universalimporter)

   IF (WITH_STATIC_LIBRARIES)
      ADD_LIBRARY(libuniversalimporter-static STATIC ${libuniversalimporter_sources})
   ENDIF()
   IF (WITH_SHARED_LIBRARIES)
      ADD_LIBRARY(libuniversalimporter-shared SHARED ${libuniversalimporter_sources})
   ENDIF()
   FOREACH(TYPE shared;static)
      IF ( (("${TYPE}" STREQUAL "static") AND (WITH_STATIC_LIBRARIES)) OR
           (("${TYPE}" STREQUAL "shared") AND (WITH_SHARED_LIBRARIES)) )
         SET_TARGET_PROPERTIES(libuniversalimporter-${TYPE} PROPERTIES OUTPUT_NAME universalimporter CLEAN_DIRECT_OUTPUT 1)
         SET_TARGET_PROPERTIES(libuniversalimporter-${TYPE} PROPERTIES
            VERSION   ${BUILD_VERSION}
            SOVERSION ${BUILD_MAJOR}
         )
         TARGET_INCLUDE_DIRECTORIES(libuniversalimporter-${TYPE} PUBLIC ${LIBMARIADB_INCLUDE_DIR} ${LIBMONGOC_INCLUDE_DIR} ${LIBBSON_INCLUDE_DIR} ${INotify_INCLUDE_DIRS})
         TARGET_LINK_LIBRARIES(libuniversalimporter-${TYPE} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LZMA_LIBRARY} ${BZ2_LIBRARY} ${Z_LIBRARY} ${LIBMARIADB_LIBRARY} ${LIBPQXX_LIBRARY} ${LIBMONGOC_LIBRARY} ${LIBBSON_LIBRARY} ${INotify_LIBRARIES})
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
IF (WITH_HIPERCONTRACER)
   ADD_EXECUTABLE(hipercontracer hipercontracer.cc)
   TARGET_LINK_LIBRARIES(hipercontracer libhipercontracer-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS     hipercontracer     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES       hipercontracer.1   DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hipercontracer.xml DESTINATION         ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
   INSTALL(FILES       hipercontracer.bib DESTINATION         ${CMAKE_INSTALL_DATADIR}/hipercontracer)

   INSTALL(FILES       hipercontracer.bash-completion
         DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
         RENAME      hipercontracer)
   IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      INSTALL(FILES hipercontracer.service       DESTINATION /lib/systemd/system)
      INSTALL(FILES hipercontracer@.service      DESTINATION /lib/systemd/system)
      INSTALL(FILES hipercontracer-12345678.conf DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()

# ====== HiPerConTracer Trigger =============================================
IF (WITH_TRIGGER)
   ADD_EXECUTABLE(hpct-trigger hpct-trigger.cc)
   TARGET_LINK_LIBRARIES(hpct-trigger libhipercontracer-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-trigger RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-trigger.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-trigger.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-trigger)
   IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      INSTALL(FILES hpct-trigger.service       DESTINATION /lib/systemd/system)
      INSTALL(FILES hpct-trigger@.service      DESTINATION /lib/systemd/system)
      INSTALL(FILES hpct-trigger-87654321.conf DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()


# ====== HiPerConTracer Sync Tool ===========================================
IF (WITH_SYNC)
   INSTALL(PROGRAMS hpct-sync DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-sync.1  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-sync.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-sync)
   IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      INSTALL(FILES hpct-sync.service DESTINATION /lib/systemd/system)
      INSTALL(FILES hpct-sync.timer   DESTINATION /lib/systemd/system)
      INSTALL(FILES hpct-sync.conf    DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()


# ====== HiPerConTracer Importer Tool =======================================
IF (WITH_IMPORTER)
   ADD_EXECUTABLE(hpct-importer hpct-importer.cc
      conversions.cc
      # reader-jitter.cc
      reader-ping.cc
      reader-traceroute.cc
   )
   TARGET_LINK_LIBRARIES(hpct-importer libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-importer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-importer.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-importer.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-importer)
   IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      INSTALL(FILES hpct-importer.service DESTINATION /lib/systemd/system)
      INSTALL(FILES hpct-importer.conf    DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()

# ====== HiPerConTracer Query Tool ==========================================
IF (WITH_QUERY)
   ADD_EXECUTABLE(hpct-query hpct-query.cc)
   TARGET_LINK_LIBRARIES(hpct-query libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-query RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-query.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-query.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-query)
ENDIF()

# ====== HiPerConTracer Results Tool ========================================
IF (WITH_RESULTS)
   ADD_EXECUTABLE(hpct-results hpct-results.cc conversions.cc)
   TARGET_LINK_LIBRARIES(hpct-results libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-results RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-results.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-results.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-results)
ENDIF()

# ====== HiPerConTracer Viewer Tool =========================================
IF (WITH_VIEWER)
   INSTALL(PROGRAMS    hpct-viewer         DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES       hpct-viewer.1       DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-viewer.desktop DESTINATION         ${CMAKE_INSTALL_DATAROOTDIR}/applications)
   INSTALL(FILES       hpct-viewer.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-viewer)
ENDIF()

# ====== HiPerConTracer Database Shell ======================================
IF (WITH_DBSHELL)
   INSTALL(PROGRAMS    dbshell   DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES       dbshell.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       dbshell.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      dbshell)
ENDIF()

# ====== HiPerConTracer DBeaver Tools =======================================
IF (WITH_DBEAVER_TOOLS)
   INSTALL(PROGRAMS make-dbeaver-configuration      DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES    make-dbeaver-configuration.1    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(PROGRAMS decrypt-dbeaver-configuration   DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES    decrypt-dbeaver-configuration.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(PROGRAMS encrypt-dbeaver-configuration   DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES    encrypt-dbeaver-configuration.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== Pipe Checksum Tool =================================================
IF (WITH_PIPE_CHECKSUM)
   ADD_EXECUTABLE(pipe-checksum pipe-checksum.cc)
   TARGET_LINK_LIBRARIES(pipe-checksum ${CRYPTO_LIBRARY})
   INSTALL(TARGETS pipe-checksum   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES   pipe-checksum.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       pipe-checksum.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      pipe-checksum)
ENDIF()

# ====== UDP Echo Server ====================================================
IF (WITH_UDP_ECHO_SERVER)
   ADD_EXECUTABLE(udp-echo-server udp-echo-server.cc logger.cc tools.cc)
   TARGET_LINK_LIBRARIES(udp-echo-server ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS udp-echo-server   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES   udp-echo-server.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES   udp-echo-server.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      udp-echo-server)
   IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      INSTALL(FILES udp-echo-server.service DESTINATION /lib/systemd/system)
      INSTALL(FILES udp-echo-server.conf    DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()

# ====== Scripts ============================================================
INSTALL(PROGRAMS get-default-ips
        DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES get-default-ips.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


# ====== TEST ONLY ==========================================================
# ADD_EXECUTABLE(t1 t1.cc)
# TARGET_LINK_LIBRARIES(t1 ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(dbtest dbtest.cc)
# TARGET_LINK_LIBRARIES(dbtest libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES hipercontracer-importer.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer)
INSTALL(FILES hipercontracer-database.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer)


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(logo)
ADD_SUBDIRECTORY(results-examples)
IF (WITH_IMPORTER OR WITH_QUERY)
   ADD_SUBDIRECTORY(SQL)
   ADD_SUBDIRECTORY(NoSQL)
   ADD_SUBDIRECTORY(TestDB)
ENDIF()
