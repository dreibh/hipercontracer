# MongoDB Setup for a HiPerConTracer Database

NOTE: This is a very brief overview of the steps to install, configure and prepare a MongoDB server to import HiPerConTracer results. Take a look at more detailed, general MongoDB documentation for more details!


## Basic Installation

### Ubuntu:
NOTE: The MongoDB PPA has to be added first!
```
sudo apt install mongodb-org
```
### Fedora:
NOTE: The MongoDB PPA has to be added first!
```
sudo dnf install (FIXME: TBD!)
```


## Basic Configuration


### Basic Engine Configuration

In /etc/mongod.conf:
```
storage:
  ...
  directoryPerDB: true
  wiredTiger:
    engineConfig:
      directoryForIndexes: true
```


### Create Root User
Choose a secure root password. Then:

```
   mongosh --quiet <<EOF
use admin
db.dropUser("root")
db.createUser({ user: "root",
                pwd: "<ROOT_PASSWORD>",
                roles: [
                   "userAdminAnyDatabase",
                   "dbAdminAnyDatabase",
                   "readWriteAnyDatabase",
                   "clusterAdmin"
                ] })
quit()
EOF
```

HINT: Create secure passwords, for example using pwgen:
```
pwgen -s 128
```


## Enable Authorization

This step enables security.authorization.

WARNING: Without enabling authorization, there would be full read/write access for everybody being able to connect to the server!

In /etc/mongod.conf:
```
security:
  authorization: enabled
```

After the change, the database is only accessible for user root with the password set above.


### Enable Network Access

WARNING: Do not perform this step without enabling authorization (see above) first!

In /etc/mongod.conf:
```
net:
  port: 27017
  ipv6: true
  bindIpAll: true
...
security:
  authorization: enabled
```


### Enable TLS

In /etc/mongod.conf (**EXAMPLE ONLY**, adapt to your setup!):
```
net:
  ...
  tls:
    mode: requireTLS
    certificateKeyFile: /etc/ssl/mongodb.domain.test/mongodb.domain.test.crt+key+chain
    disabledProtocols: TLS1_0,TLS1_1,TLS1_2
```

Make sure TLS is working properly, particularly when connecting to the database over the Internet!


## HiPerConTracer Database

### Create Database for Ping and Traceroute Measurements

See [mongodb-database.ms](mongodb-database.ms). NOTE: Replace the placeholder ${DATABASE} by your database name, e.g. "testdb", first!


### Create Database Tables for Ping and Traceroute Measurements

See [mongodb-schema.ms](mongodb-schema.ms). Apply the schema definition in the database created above!

### Create Users and Roles

Suggested setup with 3 users:
- importer: write access to import results
- researcher: read-only query access
- maintainer: full access

See [mongodb-users.ms](mongodb-users.ms). NOTE: Replace the placeholders first!

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


## GNU R Query Example

See [R-query-example.R](R-query-example.R) for an example of how to query the results from a MongoDB in GNU R.
