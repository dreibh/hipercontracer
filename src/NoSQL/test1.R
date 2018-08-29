# =================================================================
#          #     #                 #     #
#          ##    #   ####   #####  ##    #  ######   #####
#          # #   #  #    #  #    # # #   #  #          #
#          #  #  #  #    #  #    # #  #  #  #####      #
#          #   # #  #    #  #####  #   # #  #          #
#          #    ##  #    #  #   #  #    ##  #          #
#          #     #   ####   #    # #     #  ######     #
#
#       ---   The NorNet Testbed for Multi-Homed Systems  ---
#                       https://www.nntb.no
# =================================================================
#
# High-Performance Connectivity Tracer (HiPerConTracer)
# Copyright (C) 2015-2018 by Thomas Dreibholz
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact: dreibh@simula.no


# ###### Requirements #######################################################

# Up-to-date GNU R installation:
#
# Add to /etc/apt/sources.list:
#    deb https://cran.r-project.org/bin/linux/ubuntu xenial/
#    deb-src https://cran.r-project.org/bin/linux/ubuntu xenial/
# Replace "xenial" by actual Ubuntu variant!
# sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E084DAB9
# sudo apt-get update
# sudo apt-get dist-upgrade

# Install additional GNU R packages (into user's directory):
#
# options(repos = "https://cran.ms.unimelb.edu.au")
# install.packages("mongolite")    # MongoDB access
# install.packages("data.table")   # Efficient handling of big tables
# install.packages("Rcpp")         # Needed by "anytime"; pre-installed version is too old!
# install.packages("anytime")      # time conversion
# install.packages("assertthat")   # assertions
# install.packages("bitops")       # bit operations 
# install.packages("openssl")      # base64 encode/decode 
# update.packages(ask=FALSE)


library("mongolite")
library("data.table")
library("anytime")
library("assertthat")
library("bitops")
library("openssl")


# dbserver   <- "rolfsbukta.alpha.test"
# dbport     <- 27017
# dbuser     <- "researcher"
# dbpassword <- "!researcher!"
# database   <- "pingtraceroutedb"
# cafile     <- NULL
source("~/.hipercontracer-db-access")


# See https://jeroen.github.io/mongolite/connecting-to-mongodb.html#authentication
URL     <- sprintf("mongodb://%s:%s@%s:%d/%s?ssl=true",
                   dbuser, dbpassword, dbserver, dbport, database)
options <- ssl_options(weak_cert_validation=FALSE,
                       allow_invalid_hostname=TRUE,
                       ca=cafile)
# print(URL)


ping <- mongo("ping", url = URL, options = options)
traceroute <- mongo("traceroute", url = URL, options = options)



# ###### Convert binary IP address from MongoDB to string ###################
binary_ip_to_string <- function(ipAddress)
{
   binaryIP <- as.numeric(unlist(ipAddress[1]))
   # base64IP <- base64_encode( as.raw(unlist(ipAddress[1])) )

   # ====== IPv4 ============================================================
   if(length(binaryIP) == 4) {
      a <- as.numeric(binaryIP[1])
      b <- as.numeric(binaryIP[2])
      c <- as.numeric(binaryIP[3])
      d <- as.numeric(binaryIP[4])
      n <- bitShiftL(a, 24) + bitShiftL(b, 16) + bitShiftL(c, 8) + d
      assert_that(!is.na(n))
      assert_that(is.numeric(n))
      s <- paste(sep=".", a, b, c, d)
      return(s)
   }

   # ====== IPv6 ============================================================
   else if(length(binaryIP) == 16) {
      s <- ""
      for(i in 0:7) {
         n <- bitShiftL(binaryIP[1 + (2*i)], 8) + binaryIP[1 + (2*i + 1)]
         v <- sprintf("%x", n)
         if(s == "") {
            s <- v
         }
         else {
            s <- paste(sep=":", s, v)
         }
      }
      # s <- paste(sep="", s, " (", base64IP, ")")
      return(s)
   }

   # ====== Error ===========================================================
   stop("Not an IPv4 or IPv6 address!")
}


# ###### Convert Unix time in microseconds to string ########################
unix_time_to_string <- function(timeStamp)
{
   seconds      <- as.character(anytime(timeStamp / 1000000), asUTC=TRUE, tz="UTC")
   microseconds <- sprintf("%06d", timeStamp %% 1000000)
   timeString <- paste(sep=".", seconds, microseconds)
}


# ###### Convert strong to Unix time in microseconds ########################
string_to_unix_time <- function(timeString)
{
   timeStamp <- 1000000.0 * as.numeric(anytime(timeString, asUTC=TRUE))
   return(timeStamp)
}



cat("###### Ping ######\n")

dateStart <- string_to_unix_time('2018-06-01 11:11:11.000000')
dateEnd   <- string_to_unix_time('2018-06-01 11:11:12.000000')
filterStart <- paste(sep="", '{ "timestamp": { "$gte" : ', sprintf("%1.0f", dateStart), ' } }')
filterEnd   <- paste(sep="", '{ "timestamp": { "$lt" : ',  sprintf("%1.0f", dateEnd), ' } }')
filter <- paste(sep="", '{ "$and" : [ ', filterStart, ', ', filterEnd, ' ] }')
cat(sep="", "Filter: ", filter, "\n")

pingData <- data.table(ping$find(filter, limit=16))
if(length(pingData) > 0) {
   for(i in 1:length(pingData$timestamp)) {
      pingResult <- pingData[i]

      timestamp   <- unix_time_to_string(pingResult$timestamp)
      source      <- binary_ip_to_string(pingResult$source)
      destination <- binary_ip_to_string(pingResult$destination)

      cat(sep="", sprintf("%4d", i), ": ", timestamp, " (", sprintf("%1.0f", pingResult$timestamp), ")\t",
         source, " -> ", destination,
         "\t(", pingResult$rtt, " ms, csum ", sprintf("0x%x", pingResult$checksum),
         ", status ", pingResult$status, ")\n")
   }
} else {
   cat("(nothing found!)\n")
}


cat("###### Traceroute ######\n")

dateStart <- string_to_unix_time('2018-06-01 10:00:00.000000')
dateEnd   <- string_to_unix_time('2018-06-01 10:00:01.000000')
filterStart <- paste(sep="", '{ "timestamp": { "$gte" : ', sprintf("%1.0f", dateStart), ' } }')
filterEnd   <- paste(sep="", '{ "timestamp": { "$lt" : ',  sprintf("%1.0f", dateEnd), ' } }')
filter <- paste(sep="", '{ "$and" : [ ', filterStart, ', ', filterEnd, ' ] }')
cat(sep="", "Filter: ", filter, "\n")

tracerouteData <- data.table(traceroute$find(filter))
if(length(tracerouteData) > 0) {
   for(i in 1:length(tracerouteData$timestamp)) {
      tracerouteResult <- tracerouteData[i]

      timestamp   <- unix_time_to_string(tracerouteResult$timestamp)
      source      <- binary_ip_to_string(tracerouteResult$source)
      destination <- binary_ip_to_string(tracerouteResult$destination)

      cat(sep="", sprintf("%4d", i), ": ", timestamp, " (", sprintf("%1.0f", pingResult$timestamp), ")\t",
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
} else {
   cat("(nothing found!)\n")
}


cat("###### Ping, Address Search ######\n")

filter <- paste(sep="", '{ "source": { "$type": "0", "$binary": "rBABAg==" } }')
cat(sep="", "Filter: ", filter, "\n")

q <- NA
pingData <- data.table(ping$find(filter, limit=4))
if(length(pingData) > 0) {
   for(i in 1:length(pingData$timestamp)) {
      pingResult <- pingData[i]

      timestamp   <- unix_time_to_string(pingResult$timestamp)
      source      <- binary_ip_to_string(pingResult$source)
      destination <- binary_ip_to_string(pingResult$destination)
q<-pingResult$source[1]
      
      cat(sep="", sprintf("%4d", i), ": ", timestamp, " (", sprintf("%1.0f", pingResult$timestamp), ")\t",
         source, " -> ", destination,
         "\t(", pingResult$rtt, " ms, csum ", sprintf("0x%x", pingResult$checksum),
         ", status ", pingResult$status, ")\n")
   }
} else {
   cat("(nothing found!)\n")
}
