# install.packages("mongolite")
# install.packages("data.table")

library(mongolite)
library(data.table)


dbserver   <- "10.1.1.47" # rolfsbukta.alpha.test"
dbport     <- 27017
dbuser     <- "researcher"
dbpassword <- "!researcher!"
database   <- "pingtraceroutedb"


# See https://jeroen.github.io/mongolite/connecting-to-mongodb.html#authentication
URL     <- sprintf("mongodb://%s:%s@%s:%d/%s?ssl=true",
                   dbuser, dbpassword, dbserver, dbport, database)
options <- ssl_options(weak_cert_validation=TRUE, allow_invalid_hostname=TRUE)
print(URL)


ping <- mongo("ping", url = URL, options = options)
traceroute <- mongo("traceroute", url = URL, options = options)
# print(ping)

d <- data.table(traceroute$find('{ "timestamp" : 16777216 }'))
print(d)
