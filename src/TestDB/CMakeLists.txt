#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES
           README.md
           0-make-users.conf
           1-install-database
           2-initialise-database
           3-test-database
           4-clean-database
           5-perform-importer-test
           6-perform-query-test
           9-uninstall-database
           generate-test-certificates
           run-full-test
           test-tls-connection
           users.conf.example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples/TestDB)
