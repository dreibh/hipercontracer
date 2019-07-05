use admin
db.dropUser("maintainer")
db.createUser({ user: "maintainer",
                pwd: "!maintainer!",
                roles: [
                   "userAdminAnyDatabase",
                   "dbAdminAnyDatabase",
                   "readWriteAnyDatabase",
                   "clusterAdmin"
                ] })
