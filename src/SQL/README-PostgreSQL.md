# PostgreSQL Setup for a HiPerConTracer Database

NOTE: This is a very brief overview of the steps to install, configure and prepare a PostgreSQL server to import HiPerConTracer results. Take a look at more detailed, general PostgreSQL documentation for more details!


## Basic Installation

### Ubuntu:
```
sudo apt install postgresql-all postgresql-contrib
```
### Fedora:
```
sudo dnf install postgresql-server postgresql-contrib
sudo systemctl enable postgresql.service
sudo systemctl start postgresql.service
```
### FreeBSD:
```
sudo pkg install -y postgresql16-server postgresql-libpqxx
sudo sysrc postgresql_enable="YES"
sudo /usr/local/etc/rc.d/postgresql initdb
sudo service postgresql start
```


## Basic Configuration

### Enable Network Access

In /etc/postgresql/*/main/pg_hba.conf (**EXAMPLE ONLY** for 10.0.0.0/8, adapt to your setup!):
```
hostssl        all        all        10.0.0.0/8        scram-sha-256
```

In /etc/postgresql/*/main/postgresql.conf:
```
listen_addresses = '*'
```

### Tuning

In /etc/postgresql/*/main/postgresql.conf (**EXAMPLE ONLY**, adapt to your setup!):
```
shared_buffers       = 2048MB   # 1/4 of RAM
effective_cache_size = 4096MB   # 1/2 of RAM
checkpoint_timeout   = 15min
autovacuum           = on

work_mem             = 32MB
maintenance_work_mem = 64MB
```

### Enable TLS

Change in /etc/postgresql/*/main/postgresql.conf (**EXAMPLE ONLY**, adapt to your setup!):
```
ssl = on
ssl_ca_file = '/etc/ssl/TestCA/certs/TestCA-chain.pem'
ssl_cert_file = '/etc/ssl/postgresql.domain.example/postgresql.domain.example.crt'
ssl_key_file = '/etc/ssl/postgresql.domain.example/postgresql.domain.example.key'
ssl_prefer_server_ciphers = on
ssl_min_protocol_version = 'TLSv1.3'
```

Make sure TLS is working properly, particularly when connecting to the database over the Internet!


## HiPerConTracer Database

### Create Database for Ping and Traceroute Measurements

See [postgresql-database.sql](postgresql-database.sql). NOTE: Replace the placeholder ${DATABASE} by your database name, e.g. "testdb", first!


### Create Database Tables for Ping and Traceroute Measurements

See [postgresql-schema.sql](postgresql-schema.sql). Apply the schema definition in the database created above!

### Create Users and Roles

Suggested setup with 3 users:
- importer: write access to import results
- researcher: read-only query access
- maintainer: full access

See [postgresql-users.sql](postgresql-users.sql). NOTE: Replace the placeholders first!

HINT: Create secure passwords, for example using pwgen:
```
pwgen -s 128
```

### Test Connectivity
```
PGPASSWORD="<PASSWORD>" \
     PGSSLMODE="verify-full" \
     PGSSLROOTCERT="/etc/ssl/RootCA/RootCA.crt" \
     psql \
        --host=<SERVER> --port=<PORT> \
        --username=<USER> \
        --dbname="<DATABASE>"
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
