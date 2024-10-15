#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

FILE(GLOB EXAMPLE_RESULTS "*.results.*")
INSTALL(FILES HiPerConTracer.R
              ${EXAMPLE_RESULTS}
              README.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/results-examples)
INSTALL(PROGRAMS r-ping-example
                 r-traceroute-example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/results-examples)
