#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES
           README-MySQL+MariaDB.md
           README-PostgreSQL.md
           mysql-database.sql
           mysql-schema.sql
           mysql-test.sql
           mysql-users.sql
           postgresql-database.sql
           postgresql-schema.sql
           postgresql-test.sql
           postgresql-users.sql
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples/SQL)
