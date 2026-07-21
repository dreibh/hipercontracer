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

# ====== libhpctio ==========================================================
IF (WITH_LIBHPCTIO)
   LIST(APPEND libhpctio_headers
      compressortype.h
      inputstream.h
      logger.h
      outputstream.h
      tools.h
   )
   LIST(APPEND libhpctio_sources
      compressortype.cc
      inputstream.cc
      logger.cc
      outputstream.cc
      tools.cc
   )

   INSTALL(FILES       ${libhpctio_headers}
         DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hipercontracer)

   IF (WITH_STATIC_LIBRARIES)
      ADD_LIBRARY(libhpctio-static STATIC ${libhpctio_sources})
   ENDIF()
   IF (WITH_SHARED_LIBRARIES)
      ADD_LIBRARY(libhpctio-shared SHARED ${libhpctio_sources})
   ENDIF()
   FOREACH(TYPE shared;static)
      IF ( (("${TYPE}" STREQUAL "static") AND (WITH_STATIC_LIBRARIES)) OR
         (("${TYPE}" STREQUAL "shared") AND (WITH_SHARED_LIBRARIES)) )
         SET_TARGET_PROPERTIES(libhpctio-${TYPE} PROPERTIES
            OUTPUT_NAME         hpctio
            CLEAN_DIRECT_OUTPUT 1
            VERSION             ${BUILD_VERSION}
            SOVERSION           ${BUILD_MAJOR}
         )
         TARGET_INCLUDE_DIRECTORIES(libhpctio-${TYPE} PRIVATE ${Boost_INCLUDE_DIRS})
         TARGET_LINK_LIBRARIES(libhpctio-${TYPE}
                               ${Boost_LIBRARIES}
                               ${ZSTD_LIBRARY}
                               ${LZMA_LIBRARY}
                               ${BZ2_LIBRARY}
                               ${Z_LIBRARY}
                               ${SOCKET_LIBRARY}
                               ${CMAKE_THREAD_LIBS_INIT})
         INSTALL(TARGETS     libhpctio-${TYPE}
               DESTINATION ${CMAKE_INSTALL_LIBDIR})
      ENDIF()
   ENDFOREACH()
ENDIF()

# ====== libhipercontracer ==================================================
IF (WITH_LIBHIPERCONTRACER)
   LIST(APPEND libhipercontracer_headers
      check.h
      destinationinfo.h
      iomodule-base.h
      iomodule-icmp.h
      iomodule-udp.h
      # jitter.h
      ping.h
      resultentry.h
      resultswriter.h
      service.h
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
      ping.cc
      resultentry.cc
      resultswriter.cc
      service.cc
      traceroute.cc
      traceserviceheader.cc
   )

   INSTALL(FILES       ${libhipercontracer_headers}
           DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hipercontracer)

   IF (WITH_STATIC_LIBRARIES)
      ADD_LIBRARY(libhipercontracer-static STATIC ${libhipercontracer_sources})
   ENDIF()
   IF (WITH_SHARED_LIBRARIES)
      ADD_LIBRARY(libhipercontracer-shared SHARED ${libhipercontracer_sources})
   ENDIF()
   FOREACH(TYPE shared;static)
      IF ( (("${TYPE}" STREQUAL "static") AND (WITH_STATIC_LIBRARIES)) OR
         (("${TYPE}" STREQUAL "shared") AND (WITH_SHARED_LIBRARIES)) )
         SET_TARGET_PROPERTIES(libhipercontracer-${TYPE} PROPERTIES
            OUTPUT_NAME         hipercontracer
            CLEAN_DIRECT_OUTPUT 1
            VERSION             ${BUILD_VERSION}
            SOVERSION           ${BUILD_MAJOR}
         )
         TARGET_INCLUDE_DIRECTORIES(libhipercontracer-${TYPE} PRIVATE ${Boost_INCLUDE_DIRS})
         TARGET_LINK_LIBRARIES(libhipercontracer-${TYPE}
                               libhpctio-${TYPE}
                               ${Boost_LIBRARIES}
                               ${CMAKE_THREAD_LIBS_INIT})
         INSTALL(TARGETS     libhipercontracer-${TYPE}
                 DESTINATION ${CMAKE_INSTALL_LIBDIR})
      ENDIF()
   ENDFOREACH()
ENDIF()

# ====== libhpctdb ==========================================================
IF (WITH_LIBHPCTDB)
   LIST(APPEND libhpctdb_headers
      database-configuration.h
      database-statement.h
      databaseclient-base.h
   )
   LIST(APPEND libhpctdb_sources
      database-configuration.cc
      database-statement.cc
      databaseclient-base.cc
   )

   IF (ENABLE_BACKEND_DEBUG)
      LIST(APPEND libhpctdb_headers databaseclient-debug.h)
      LIST(APPEND libhpctdb_sources databaseclient-debug.cc)
      SET_PROPERTY(SOURCE database-configuration.cc
                   APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_DEBUG)
   ENDIF()
   IF (ENABLE_BACKEND_MARIADB)
      LIST(APPEND libhpctdb_headers databaseclient-mariadb.h)
      LIST(APPEND libhpctdb_sources databaseclient-mariadb.cc)
      SET_PROPERTY(SOURCE database-configuration.cc
                   APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_MARIADB)
   ENDIF()
   IF (ENABLE_BACKEND_POSTGRESQL)
      LIST(APPEND libhpctdb_headers databaseclient-postgresql.h)
      LIST(APPEND libhpctdb_sources databaseclient-postgresql.cc)
      SET_PROPERTY(SOURCE database-configuration.cc
                   APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_POSTGRESQL)
   ENDIF()
   IF (ENABLE_BACKEND_MONGODB)
      LIST(APPEND libhpctdb_headers databaseclient-mongodb.h)
      LIST(APPEND libhpctdb_sources databaseclient-mongodb.cc)
      SET_PROPERTY(SOURCE database-configuration.cc
                   APPEND PROPERTY COMPILE_DEFINITIONS ENABLE_BACKEND_MONGODB)
   ENDIF()

   INSTALL(FILES       ${libhpctdb_headers}
           DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hipercontracer)

   IF (WITH_STATIC_LIBRARIES)
      ADD_LIBRARY(libhpctdb-static STATIC ${libhpctdb_sources})
   ENDIF()
   IF (WITH_SHARED_LIBRARIES)
      ADD_LIBRARY(libhpctdb-shared SHARED ${libhpctdb_sources})
   ENDIF()
   FOREACH(TYPE shared;static)
      IF ( (("${TYPE}" STREQUAL "static") AND (WITH_STATIC_LIBRARIES)) OR
         (("${TYPE}" STREQUAL "shared") AND (WITH_SHARED_LIBRARIES)) )
         SET_TARGET_PROPERTIES(libhpctdb-${TYPE} PROPERTIES
            OUTPUT_NAME         hpctdb
            CLEAN_DIRECT_OUTPUT 1
            VERSION             ${BUILD_VERSION}
            SOVERSION           ${BUILD_MAJOR}
         )
         TARGET_INCLUDE_DIRECTORIES(libhpctdb-${TYPE} PRIVATE
                                    ${LIBMARIADB_INCLUDE_DIR}
                                    ${PQXX_INCLUDE_DIRS}
                                    ${LIBMONGOC_INCLUDE_DIR}
                                    ${LIBBSON_INCLUDE_DIR}
                                    ${Boost_INCLUDE_DIRS})
         TARGET_LINK_LIBRARIES(libhpctdb-${TYPE}
                               libhpctio-${TYPE}
                               ${LIBMARIADB_LIBRARY}
                               ${PQXX_LIBRARIES}
                               ${LIBMONGOC_LIBRARY}
                               ${LIBBSON_LIBRARY})
         INSTALL(TARGETS     libhpctdb-${TYPE}
                 DESTINATION ${CMAKE_INSTALL_LIBDIR})
      ENDIF()
   ENDFOREACH()
ENDIF()

# ====== libuniversalimporter ==================================================
IF (WITH_LIBUNIVERSALIMPORTER)
   LIST(APPEND libuniversalimporter_headers
      importer-configuration.h
      reader-base.h
      results-exception.h
      universal-importer.h
      worker.h
   )
   LIST(APPEND libuniversalimporter_sources
      importer-configuration.cc
      reader-base.cc
      universal-importer.cc
      worker.cc
   )

   INSTALL(FILES       ${libuniversalimporter_headers}
           DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/universalimporter)

   IF (WITH_STATIC_LIBRARIES)
      ADD_LIBRARY(libuniversalimporter-static STATIC ${libuniversalimporter_sources})
   ENDIF()
   IF (WITH_SHARED_LIBRARIES)
      ADD_LIBRARY(libuniversalimporter-shared SHARED ${libuniversalimporter_sources})
   ENDIF()
   FOREACH(TYPE shared;static)
      IF ( (("${TYPE}" STREQUAL "static") AND (WITH_STATIC_LIBRARIES)) OR
           (("${TYPE}" STREQUAL "shared") AND (WITH_SHARED_LIBRARIES)) )
         SET_TARGET_PROPERTIES(libuniversalimporter-${TYPE} PROPERTIES
            OUTPUT_NAME         universalimporter
            CLEAN_DIRECT_OUTPUT 1
            VERSION             ${BUILD_VERSION}
            SOVERSION           ${BUILD_MAJOR}
         )
         TARGET_INCLUDE_DIRECTORIES(libuniversalimporter-${TYPE} PRIVATE
                                    ${INotify_INCLUDE_DIRS}
                                    ${Boost_INCLUDE_DIRS})
         TARGET_LINK_LIBRARIES(libuniversalimporter-${TYPE}
                               libhpctdb-${TYPE}
                               ${Boost_LIBRARIES}
                               ${LIBMARIADB_LIBRARY}
                               ${PQXX_LIBRARIES}
                               ${LIBMONGOC_LIBRARY}
                               ${LIBBSON_LIBRARY}
                               ${INotify_LIBRARIES}
                               ${SOCKET_LIBRARY}
                               ${CMAKE_THREAD_LIBS_INIT})
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
   TARGET_INCLUDE_DIRECTORIES(hipercontracer PRIVATE ${Boost_INCLUDE_DIRS})
   TARGET_LINK_LIBRARIES(hipercontracer libhipercontracer-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS     hipercontracer     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES       hipercontracer.1   DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hipercontracer.xml DESTINATION         ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages)
   INSTALL(FILES       hipercontracer.bib DESTINATION         ${CMAKE_INSTALL_DATADIR}/hipercontracer)

   INSTALL(FILES       hipercontracer.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hipercontracer)
   IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
      INSTALL(FILES hipercontracer.service       DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES hipercontracer@.service      DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES hipercontracer-12345678.conf DESTINATION /etc/hipercontracer)
   ENDIF()

   CONFIGURE_FILE(get-default-ips get-default-ips.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES get-default-ips.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/get-default-ips.versioned
         DESTINATION ${CMAKE_INSTALL_BINDIR}
         RENAME      get-default-ips)
   INSTALL(FILES get-default-ips.1
         DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== HiPerConTracer Trigger =============================================
IF (WITH_TRIGGER)
   ADD_EXECUTABLE(hpct-trigger hpct-trigger.cc)
   TARGET_INCLUDE_DIRECTORIES(hpct-trigger PRIVATE ${Boost_INCLUDE_DIRS})
   TARGET_LINK_LIBRARIES(hpct-trigger libhipercontracer-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-trigger RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES       hpct-trigger.1 DESTINATION   ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-trigger.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-trigger)
   IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
      INSTALL(FILES hpct-trigger.service       DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES hpct-trigger@.service      DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES hpct-trigger-87654321.conf DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()


# ====== HiPerConTracer Sync Tool ===========================================
IF (WITH_SYNC)
   CONFIGURE_FILE(hpct-sync hpct-sync.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES hpct-sync.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/hpct-sync.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      hpct-sync)
   INSTALL(FILES hpct-sync.1  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-sync.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-sync)
   IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
      INSTALL(FILES hpct-sync.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES hpct-sync.timer   DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES hpct-sync.conf    DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()


# ====== HiPerConTracer Reverse Tunnel Tool =================================
IF (WITH_RTUNNEL)
   CONFIGURE_FILE(hpct-rtunnel hpct-rtunnel.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES hpct-rtunnel.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/hpct-rtunnel.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      hpct-rtunnel)
   INSTALL(FILES       hpct-rtunnel.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-rtunnel.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-rtunnel)
   IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
      INSTALL(FILES hpct-rtunnel.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
   ENDIF()
ENDIF()


# ====== HiPerConTracer Node Tools ==========================================
IF (WITH_NODE)
   CONFIGURE_FILE(hpct-node-setup hpct-node-setup.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES hpct-node-setup.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/hpct-node-setup.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      hpct-node-setup)
   INSTALL(FILES       hpct-node-setup.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-node-setup.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-node-setup)
ENDIF()


# ====== HiPerConTracer Collector Tools =====================================
IF (WITH_COLLECTOR)
   CONFIGURE_FILE(hpct-node-removal hpct-node-removal.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES hpct-node-removal.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/hpct-node-removal.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      hpct-node-removal)
   INSTALL(FILES       hpct-node-removal.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-node-removal.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-node-removal)

   CONFIGURE_FILE(hpct-nodes-list hpct-nodes-list.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES hpct-nodes-list.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/hpct-nodes-list.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      hpct-nodes-list)
   INSTALL(FILES       hpct-nodes-list.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-nodes-list.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-nodes-list)

   CONFIGURE_FILE(hpct-ssh hpct-ssh.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES hpct-ssh.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/hpct-ssh.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      hpct-ssh)
   INSTALL(FILES       hpct-ssh.1        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-ssh.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-ssh)
ENDIF()


# ====== HiPerConTracer Importer Tool =======================================
IF (WITH_IMPORTER)
   ADD_EXECUTABLE(hpct-importer hpct-importer.cc
      conversions.cc
      # reader-jitter.cc
      reader-ping.cc
      reader-traceroute.cc
   )
   TARGET_INCLUDE_DIRECTORIES(hpct-importer PRIVATE ${Boost_INCLUDE_DIRS})
   TARGET_LINK_LIBRARIES(hpct-importer libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-importer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-importer.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-importer.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-importer)
   IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
      INSTALL(FILES hpct-importer.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES hpct-importer.conf    DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()

# ====== HiPerConTracer Query Tool ==========================================
IF (WITH_QUERY)
   ADD_EXECUTABLE(hpct-query hpct-query.cc)
   TARGET_INCLUDE_DIRECTORIES(hpct-query PRIVATE ${Boost_INCLUDE_DIRS})
   TARGET_LINK_LIBRARIES(hpct-query libhpctdb-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-query RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-query.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-query.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-query)
ENDIF()

# ====== HiPerConTracer Results Tool ========================================
IF (WITH_RESULTS)
   ADD_EXECUTABLE(hpct-results hpct-results.cc conversions.cc)
   TARGET_INCLUDE_DIRECTORIES(hpct-results PRIVATE ${Boost_INCLUDE_DIRS})
   TARGET_LINK_LIBRARIES(hpct-results libhpctio-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS hpct-results RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES hpct-results.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-results.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-results)
ENDIF()

# ====== HiPerConTracer Viewer Tool =========================================
IF (WITH_VIEWER)
   CONFIGURE_FILE(hpct-viewer hpct-viewer.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES hpct-viewer.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/hpct-viewer.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      hpct-viewer)
   INSTALL(FILES       hpct-viewer.1       DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       hpct-viewer.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
   INSTALL(FILES       hpct-viewer.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      hpct-viewer)
   INSTALL(FILES       no.nntb.dreibh.hipercontracer.hpct-viewer.metainfo.xml
           DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
ENDIF()

# ====== HiPerConTracer Database Shell ======================================
IF (WITH_DBSHELL)
   CONFIGURE_FILE(dbshell dbshell.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES dbshell.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/dbshell.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      dbshell)
   INSTALL(FILES       dbshell.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       dbshell.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      dbshell)
ENDIF()

# ====== HiPerConTracer DBeaver Tools =======================================
IF (WITH_DBEAVER_TOOLS)
   CONFIGURE_FILE(make-dbeaver-configuration make-dbeaver-configuration.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES make-dbeaver-configuration.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/make-dbeaver-configuration.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      make-dbeaver-configuration)
   INSTALL(FILES    make-dbeaver-configuration.1    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

   CONFIGURE_FILE(decrypt-dbeaver-configuration decrypt-dbeaver-configuration.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES decrypt-dbeaver-configuration.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/decrypt-dbeaver-configuration.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      decrypt-dbeaver-configuration)
   INSTALL(FILES    decrypt-dbeaver-configuration.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

   CONFIGURE_FILE(encrypt-dbeaver-configuration encrypt-dbeaver-configuration.versioned @ONLY)
   SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES encrypt-dbeaver-configuration.versioned)
   INSTALL(PROGRAMS    ${CMAKE_CURRENT_BINARY_DIR}/encrypt-dbeaver-configuration.versioned
           DESTINATION ${CMAKE_INSTALL_BINDIR}
           RENAME      encrypt-dbeaver-configuration)
   INSTALL(FILES    encrypt-dbeaver-configuration.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
ENDIF()

# ====== Pipe Checksum Tool =================================================
IF (WITH_PIPE_CHECKSUM)
   ADD_EXECUTABLE(pipe-checksum pipe-checksum.cc)
   TARGET_INCLUDE_DIRECTORIES(pipe-checksum PRIVATE ${Boost_INCLUDE_DIRS} ${CRYPTO_INCLUDE_DIR})
   TARGET_LINK_LIBRARIES(pipe-checksum ${Boost_LIBRARIES} ${CRYPTO_LIBRARY})
   INSTALL(TARGETS pipe-checksum   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES   pipe-checksum.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES       pipe-checksum.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      pipe-checksum)
ENDIF()

# ====== UDP Echo Server ====================================================
IF (WITH_UDP_ECHO_SERVER)
   ADD_EXECUTABLE(udp-echo-server udp-echo-server.cc logger.cc tools.cc)
   TARGET_INCLUDE_DIRECTORIES(udp-echo-server PRIVATE ${Boost_INCLUDE_DIRS})
   TARGET_LINK_LIBRARIES(udp-echo-server ${Boost_LIBRARIES} ${SOCKET_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
   INSTALL(TARGETS udp-echo-server   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
   INSTALL(FILES   udp-echo-server.1 DESTINATION         ${CMAKE_INSTALL_MANDIR}/man1)
   INSTALL(FILES   udp-echo-server.bash-completion
           DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
           RENAME      udp-echo-server)
   IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
      INSTALL(FILES udp-echo-server.service DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
      INSTALL(FILES udp-echo-server.conf    DESTINATION /etc/hipercontracer)
   ENDIF()
ENDIF()


# ====== TEST ONLY ==========================================================
# ADD_EXECUTABLE(t1 t1.cc)
# TARGET_LINK_LIBRARIES(t1 ${Boost_LIBRARIES} libhpctio-${libraryType} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(t2 t2.cc)
# TARGET_LINK_LIBRARIES(t2 ${Boost_LIBRARIES} libhpctio-${libraryType} ${CMAKE_THREAD_LIBS_INIT})
# ADD_EXECUTABLE(dbtest dbtest.cc)
# TARGET_LINK_LIBRARIES(dbtest libuniversalimporter-${libraryType} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

IF (WITH_IMPORTER)
   INSTALL(FILES hipercontracer-importer.conf
           DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer)
ENDIF()
IF (WITH_LIBHPCTDB)
   INSTALL(FILES hipercontracer-database.conf
           DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer)
ENDIF()


#############################################################################
#### SUBDIRECTORIES                                                      ####
#############################################################################

ADD_SUBDIRECTORY(logo)
ADD_SUBDIRECTORY(results-examples)
IF (WITH_IMPORTER)
   ADD_SUBDIRECTORY(SQL)
   ADD_SUBDIRECTORY(NoSQL)
   ADD_SUBDIRECTORY(TestDB)
ENDIF()
