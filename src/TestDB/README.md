# HiPerConTracer Database Test Scripts

This directory contains scripts to install, configure and prepare a test database, as well as to run a full import and query test run with this test database.
- **[run-full-test](run-full-test)**
- [0-make-configurations](0-make-configurations)
- [1-install-database](1-install-database)
- [2-initialise-database](2-initialise-database)
- [3-test-database](3-test-database)
- [4-clean-database](4-clean-database)
- [5-perform-hpct-importer-test](5-perform-hpct-importer-test)
- [6-perform-hpct-query-test](6-perform-hpct-query-test)
- [9-uninstall-database](9-uninstall-database)

These scripts use some of the [X.509-Tools](https://www.nntb.no/~dreibh/system-tools/#x.509-tools), which are distributed as part of the [System-Tools](https://www.nntb.no/~dreibh/system-tools/) package, for creating a CA and test certificates, as well as checking the TLS configuration of the database.
