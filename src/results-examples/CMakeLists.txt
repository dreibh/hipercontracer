#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

FILE(GLOB EXAMPLE_RESULTS "*.results.*")
INSTALL(FILES HiPerConTracer.R
              ${EXAMPLE_RESULTS}
              README.md
              r-ping-example
              r-traceroute-example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples/results-examples)
