#################################################
# EXAMPLES
#################################################

INSTALL(FILES install-database-and-users login-as-importer login-as-researcher
              database.sql procedures.sql README schema.sql users.sql
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples)
