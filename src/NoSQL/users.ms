use pingtraceroutedb
db.dropUser("importer")
db.dropUser("researcher")
db.createUser({ user: "importer",   pwd: "!importer!",   roles: [ "readWrite" ] })
db.createUser({ user: "researcher", pwd: "!researcher!", roles: [ "dbAdmin", "readWrite" ] })
