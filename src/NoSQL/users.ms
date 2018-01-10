use admin
db.dropUser("maintainer")
db.dropUser("importer")
db.dropUser("researcher")
db.createUser({ user: "maintainer", pwd: "!maintainer!", roles: [ { role: "userAdminAnyDatabase", db: "admin" } ] })
db.createUser({ user: "importer",   pwd: "!importer!",   roles: [ { role: "readWrite", db: "ping" }, { role: "readWrite", db: "traceroute" } ] })
db.createUser({ user: "researcher", pwd: "!researcher!", roles:
   [ { role: "dbAdmin",   db: "ping" },
     { role: "readWrite", db: "ping" },
     { role: "dbAdmin",   db: "traceroute" },
     { role: "readWrite", db: "traceroute" } ]
})
