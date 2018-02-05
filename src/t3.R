# install.packages("mongolite")
# install.packages("data.table")
# install.packages("iptools")
# install.packages("Rcpp")   # Needed by "anytime"; pre-installed version is too old!
# install.packages("anytime")

library(mongolite)
library(data.table)
library(iptools)
library(anytime)


dbserver   <- "rolfsbukta.alpha.test"
dbport     <- 27017
dbuser     <- "researcher"
dbpassword <- "!researcher!"
database   <- "pingtraceroutedb"


# See https://jeroen.github.io/mongolite/connecting-to-mongodb.html#authentication
URL     <- sprintf("mongodb://%s:%s@%s:%d/%s?ssl=true",
                   dbuser, dbpassword, dbserver, dbport, database)
options <- ssl_options(weak_cert_validation=TRUE, allow_invalid_hostname=TRUE)
# print(URL)


ping <- mongo("ping", url = URL, options = options)
traceroute <- mongo("traceroute", url = URL, options = options)
# print(ping)



# ###### Convert binary IP address from MongoDB to string ###################
binary_ip_to_string <- function(ipAddress)
{
   binaryIP <- as.numeric(unlist(ipAddress[1]))
   if(length(binaryIP) == 4) {
      a <- as.numeric(binaryIP[1])
      b <- binaryIP[2]
      c <- binaryIP[3]
      d <- binaryIP[4]
      n <- bitwShiftL(a, 24) + bitwShiftL(b, 16) + bitwShiftL(c, 8) + d
      return(numeric_to_ip(n))
   }
   else if(length(binaryIP) == 16) {
      s <- ""
      for(i in 0:7) {
         n <- bitwShiftL(binaryIP[1 + (2*i)], 8) + binaryIP[1 + (2*i + 1)]
         v <- sprintf("%x", n)
         if(s == "") {
            s <- v
         }
         else {
            s <- paste(sep=":", s, v)
         }
      }      
      return(s)
   }
   stop("Not an IPv4 or IPv6 address!")
}


# ###### Convert Unix time in microseconds to string ########################
unix_time_to_string <- function(timeStamp)
{
   s <- as.character(anytime(timeStamp / 1000000), asUTC=TRUE, tz="UTC")
   u <- as.character(timeStamp %% 1000000)
   return(paste(sep=".", s, u))
}


cat("###### Ping ######\n")
pingData <- data.table(ping$find('{ "timestamp" : 16777216 }', limit=32))
for(i in 1:length(pingData$timestamp)) {
   pingResult <- pingData[i]
   
   timestamp   <- unix_time_to_string(pingResult$timestamp)
   source      <- binary_ip_to_string(pingResult$source)
   destination <- binary_ip_to_string(pingResult$destination)
   
   cat(sep="", sprintf("%4d", i), ": ", timestamp, "\t",
       source, " -> ", destination,
       "\t(", pingResult$rtt, " ms, csum ", sprintf("0x%x", pingResult$checksum),
       ", status ", pingResult$status, ")\n")
}


cat("###### Traceroute ######\n")
tracerouteData <- data.table(traceroute$find('{ "timestamp" : 16777217 }', limit=4))
for(i in 1:length(tracerouteData$timestamp)) {
   tracerouteResult <- tracerouteData[i]
   
   timestamp   <- unix_time_to_string(tracerouteResult$timestamp)
   source      <- binary_ip_to_string(tracerouteResult$source)
   destination <- binary_ip_to_string(tracerouteResult$destination)
   
   cat(sep="", sprintf("%4d", i), ": ", timestamp, "\t",
       source, " -> ", destination,
       " (round ", tracerouteResult$round,
       ", csum ", sprintf("0x%x", tracerouteResult$checksum),
       ", flags ", tracerouteResult$statusFlags,
       ")\n")

   hopTable <- tracerouteResult$hops[[1]]
   for(j in 1:length(hopTable$hop)) {
      hop    <- binary_ip_to_string(hopTable$hop[j])
      status <- hopTable$status[j]
      rtt    <- hopTable$rtt[j]

      cat(sep="", "\t", sprintf("%2d", j), ":\t", hop, "\t(rtt ", rtt, " ms, status ", status, ")\n")
   }
}
