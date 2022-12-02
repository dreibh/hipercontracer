use admin
db.dropUser("maintainer")
db.createUser({ user: "maintainer",
                pwd: "${MAINTAINER_PASSWORD}",
                roles: [
                   "userAdminAnyDatabase",
                   "dbAdminAnyDatabase",
                   "readWriteAnyDatabase",
                   "clusterAdmin"
                ] })

use ${DATABASE}
db.dropUser("importer")
db.dropUser("researcher")
db.createUser({ user: "importer",   pwd: "${IMPORTER_PASSWORD}",   roles: [ "readWrite" ] })
db.createUser({ user: "researcher", pwd: "${RESEARCHER_PASSWORD}", roles: [ "dbAdmin", "readWrite" ] })

quit()
