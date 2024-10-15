#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES
           README-MySQL+MariaDB.md
           README-PostgreSQL.md
           mariadb-database.sql
           mariadb-delete-all-rows.sql
           mariadb-schema.sql
           mariadb-test.sql
           mariadb-users.sql
           postgresql-database.sql
           postgresql-delete-all-rows.sql
           postgresql-schema.sql
           postgresql-test.sql
           postgresql-users.sql
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/SQL)
