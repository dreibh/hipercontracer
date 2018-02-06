#################################################
# EXAMPLES
#################################################

INSTALL(FILES install-database-and-users admin.ms database.ms Makefile
              README R-query-example.R schema.ms users.ms
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples/NoSQL)
