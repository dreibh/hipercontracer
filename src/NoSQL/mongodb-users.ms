use ${DATABASE}

db.dropUser("maintainer")
db.dropUser("importer")
db.dropUser("researcher")
db.createUser({ user: "root",       pwd: "$ROOT_PASSWORD}",        roles: [ "dbOwner" ] })
db.createUser({ user: "maintainer", pwd: "${MAINTAINER_PASSWORD}", roles: [ "dbAdmin", "readWrite" ] })
db.createUser({ user: "importer",   pwd: "${IMPORTER_PASSWORD}",   roles: [ "readWrite" ] })
db.createUser({ user: "researcher", pwd: "${RESEARCHER_PASSWORD}", roles: [ "read" ] })

quit()
