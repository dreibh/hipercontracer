# PostgreSQL Seup for HiPerConTracer Database

NOTE: This is a very brief overview of the setups to install, configure and prepare
a PostgreSQL server to import HiPerConTracer results. Take a look at more detailed
general PostgreSQL documentation for more details!


## Basic Installation

### Ubuntu:
```
sudo apt install postgresql-all postgresql-contrib
```
### Fedora:
```
sudo dnf install postgresql-server postgresql-contrib
```


## Basic Configuration

### Enable Network Access

In /etc/postgresql/*/main/pg_hba.conf (EXAMPLE ONLY for 10.0.0.0/8, adapt to your setup!):
```
hostssl        all        all        10.0.0.0/8        scram-sha-256
```

In /etc/postgresql/*/main/postgresql.conf:
```
listen_addresses = '*'
```

### Tuning

In /etc/postgresql/*/main/postgresql.conf (EXAMPLE ONLY, adapt to your setup!):
```
shared_buffers       = 2048MB   # 1/4 of RAM
effective_cache_size = 4096MB   # 1/2 of RAM
checkpoint_timeout   = 15min
autovacuum           = on

work_mem             = 32MB
maintenance_work_mem = 64MB
```

### Enable TLS

Change in /etc/postgresql/*/main/postgresql.conf (EXAMPLE ONLY, adapt to your setup!):
```
# ====== NorNet =============================================================
ssl                       = on
ssl_ciphers               = 'ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256'
ssl_prefer_server_ciphers = on
password_encryption       = scram-sha-256
ssl_ca_file               = '/etc/ssl/server-chain.pem'
ssl_cert_file             = '/etc/ssl/server.crt'
ssl_key_file              = '/etc/ssl/server.key'
```

Make sure TLS is working properly, particularly when connecting to the database over the Internet!


## HiPerConTracer Database

### Create Database for Ping and Traceroute Measurements

See [postgresql-database.sql]. NOTE: Replace the placeholder ${DATABASE} by your database name, e.g. "testdb", first!


### Create Database Tables for Ping and Traceroute Measurements

See [postgresql-schema.sql]. Apply the schema definition in the database created above!

### Create Users and Roles

Suggested setup with 3 users:
- importer: write access to import results
- researcher: read-only query access
- maintainer: full access

See [postgresql-users.sql]. NOTE: Replace the placeholders ${DATABASE} and ${<USER>_PASSWORD} first!

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

See [TestDB](../TestDB) for example scripts to install, configure, prepare and test a test databse:
- **[run-full-test](../TestDB/run-full-test)**
- [0-make-users.conf](../TestDB/0-make-users.conf)
- [1-install-database](../TestDB/1-install-database)
- [2-initialise-database](../TestDB/2-initialise-database)
- [3-test-database](../TestDB/3-test-database)
- [4-clean-database](../TestDB/4-clean-database)
- [5-perform-importer-test](../TestDB/5-perform-importer-test)
- [6-perform-query-test](../TestDB/6-perform-query-test)
- [9-uninstall-database](../TestDB/9-uninstall-database)
