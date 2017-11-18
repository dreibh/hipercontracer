include_directories(.)

#################################################
# PROGRAMS
#################################################

ADD_EXECUTABLE(hipercontracer
   hipercontracer.cc
   icmpheader.h
   ipv4header.h
   ipv6header.h
   traceserviceheader.h
   traceroute.cc
   traceroute.h
   service.h
   sqlwriter.cc
   sqlwriter.h
   ping.cc
   ping.h
)
TARGET_LINK_LIBRARIES(hipercontracer ${Boost_LIBRARIES} "${CMAKE_THREAD_LIBS_INIT}")
INSTALL(TARGETS hipercontracer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES hipercontracer.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


INSTALL(FILES tracedataimporter addressinfogenerator
        DESTINATION ${CMAKE_INSTALL_BINDIR})
INSTALL(FILES tracedataimporter.1 addressinfogenerator.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


#################################################
# EXAMPLES
#################################################

INSTALL(FILES hipercontracer-database-configuration
        DESTINATION ${CMAKE_INSTALL_DATADIR_DOC}/doc/hipercontracer/examples)


#################################################
# SUBDIRECTORIES
#################################################

ADD_SUBDIRECTORY(SQL)
