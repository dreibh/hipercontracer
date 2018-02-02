use admin
db.dropUser("maintainer")
db.createUser({ user: "maintainer", pwd: "!maintainer!", roles: [ { role: "userAdminAnyDatabase", db: "admin" } ] })
