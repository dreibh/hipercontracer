# MySQL/MariaDB Setup for a HiPerConTracer Database

NOTE: This is a very brief overview of the steps to install, configure and prepare a MySQL/MariaDB server to import HiPerConTracer results. Take a look at more detailed, general MySQL/MariaDB documentation for more details!


## Basic Installation

### Ubuntu:
```
sudo apt install mariadb-server mariadb-backup mariadb-client libmariadb-dev
```
### Fedora:
```
sudo dnf install mariadb-server mariadb-backup mariadb-connector-c-devel openssl
sudo systemctl enable mariadb.service
sudo systemctl start mariadb.service
```
### FreeBSD:
```
sudo dnf install mariadb106-server mariadb106-client
sudo sysrc mysql_enable=YES
sudo service mysql-server start
```


## Basic Configuration

### Enable Network Access

In /etc/mysql/mariadb.conf.d/50-server.cnf (**EXAMPLE ONLY** for 10.0.0.0/8, adapt to your setup!):
```
hostssl        all        all        10.0.0.0/8        scram-sha-256
```

In /etc/mariadb/*/main/mariadb.conf:
```
bind-address = *
```

### Enable TLS

In /etc/mysql/mariadb.conf.d/50-server.cnf (**EXAMPLE ONLY**, adapt to your setup!):
```
ssl-ca = /etc/ssl/TestCA/certs/TestCA-chain.pem
ssl-cert = /etc/ssl/mariadb.domain.example/mariadb.domain.example.crt
ssl-key = /etc/ssl/mariadb.domain.example/mariadb.domain.example.key
require-secure-transport = on
tls_version = TLSv1.3
```

Make sure TLS is working properly, particularly when connecting to the database over the Internet!


## HiPerConTracer Database

### Create Database for Ping and Traceroute Measurements

See [mariadb-database.sql](mariadb-database.sql). NOTE: Replace the placeholder ${DATABASE} by your database name, e.g. "testdb", first!


### Create Database Tables for Ping and Traceroute Measurements

See [mariadb-schema.sql]([mariadb-schema.sql). Apply the schema definition in the database created above!

### Create Users and Roles

Suggested setup with 3 users:
- importer: write access to import results
- researcher: read-only query access
- maintainer: full access

See [mariadb-users.sql](mariadb-users.sql). NOTE: Replace the placeholders first!

HINT: Create secure passwords, for example using pwgen:
```
pwgen -s 128
```

### Test Connectivity
```
mysql \
 --host=<SERVER> --port <PORT> --protocol=tcp \
 --ssl --ssl-verify-server-cert --ssl-ca=/etc/ssl/RootCA/RootCA.crt \
 --user=<USER> --password="<PASSWORD>" \
 --database="${DATABASE}"
```


## Test Database Example Scripts

See [TestDB](../TestDB) for example scripts to install, configure and prepare a test database, as well as to run a full import and query test run with this test database:

- **[run-full-test](../TestDB/run-full-test)**
- [0-make-configurations](../TestDB/0-make-configurations)
- [1-install-database](../TestDB/1-install-database)
- [2-initialise-database](../TestDB/2-initialise-database)
- [3-test-database](../TestDB/3-test-database)
- [4-clean-database](../TestDB/4-clean-database)
- [5-perform-hpct-importer-test](../TestDB/5-perform-hpct-importer-test)
- [6-perform-hpct-query-test](../TestDB/6-perform-hpct-query-test)
- [9-uninstall-database](../TestDB/9-uninstall-database)
