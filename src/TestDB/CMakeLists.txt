#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES
           CertificateHelper.py
           README.md
           hpct-users.conf.example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/TestDB)
INSTALL(PROGRAMS
           0-make-configurations
           1-install-database
           2-initialise-database
           3-test-database
           4-clean-database
           5-perform-hpct-importer-test
           6-perform-hpct-query-test
           9-uninstall-database
           generate-test-certificates
           name-in-etc-hosts
           run-full-test
           test-tls-connection
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/TestDB)
