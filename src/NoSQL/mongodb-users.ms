use admin
db.createUser({ user: "root", pwd: "$ROOT_PASSWORD}",
                roles: [ "userAdminAnyDatabase",
                         "dbAdminAnyDatabase",
                         "readWriteAnyDatabase",
                         "clusterAdmin" ],
                mechanisms: [ 'SCRAM-SHA-256' ] })

use ${DATABASE}

db.dropUser("maintainer")
db.dropUser("importer")
db.dropUser("researcher")
db.createUser({ user: "maintainer", pwd: "${MAINTAINER_PASSWORD}",
                roles: [ "dbAdmin", "readWrite" ],
                mechanisms: [ 'SCRAM-SHA-256' ]})
db.createUser({ user: "importer",   pwd: "${IMPORTER_PASSWORD}",
                roles: [ "readWrite" ],
                mechanisms: [ 'SCRAM-SHA-256' ]})
db.createUser({ user: "researcher", pwd: "${RESEARCHER_PASSWORD}",
                roles: [ "read" ],
                mechanisms: [ 'SCRAM-SHA-256' ]})

quit()
