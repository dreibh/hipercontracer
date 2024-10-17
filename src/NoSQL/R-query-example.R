# ==========================================================================
#     _   _ _ ____            ____          _____
#    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
#    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
#    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
#    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
#
#       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
#                 https://www.nntb.no/~dreibh/hipercontracer/
# ==========================================================================
#
# High-Performance Connectivity Tracer (HiPerConTracer)
# Copyright (C) 2015-2025 by Thomas Dreibholz
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
#
# install.packages("mongolite")    # MongoDB access
# install.packages("data.table")   # Efficient handling of big tables
# install.packages("Rcpp")         # Needed by "anytime"; pre-installed version is too old!
# install.packages("anytime")      # time conversion
# install.packages("assertthat")   # assertions
# install.packages("bitops")       # bit operations
# install.packages("openssl")      # base64 encode/decode
#
# update.packages(ask=FALSE)


library("mongolite")
library("data.table")
library("anytime")
library("assertthat")
library("bitops")
library("openssl")

source("nornet-tools.R")


# ====== Connect to MongoDB =================================================
# dbserver   <- "rolfsbukta.alpha.test"
# dbport     <- 27017
# dbuser     <- "researcher"
# dbpassword <- "!researcher!"
# database   <- "pingtraceroutedb"
cafile <- NULL
source("~/.hipercontracer-db-access")

# See https://jeroen.github.io/mongolite/connecting-to-mongodb.html#authentication
options <- ssl_options(weak_cert_validation=FALSE,
                       allow_invalid_hostname=TRUE,
                       ca=cafile)
URL     <- sprintf("mongodb://%s:%s@%s:%d/%s?ssl=true",
                   dbuser, dbpassword, dbserver, dbport, database)
# print(URL)

ping <- mongo("ping", url = URL, options = options)
traceroute <- mongo("traceroute", url = URL, options = options)


# ====== Ping Example =======================================================
cat("###### Ping ######\n")

dateStart <- string_to_unix_time('2018-06-01 11:11:11.000000000')
dateEnd   <- string_to_unix_time('2018-06-01 11:11:12.000000000')
filterStart <- paste(sep="", '{ "timestamp": { "$gte" : ', sprintf("%1.0f", dateStart), ' } }')
filterEnd   <- paste(sep="", '{ "timestamp": { "$lt" : ',  sprintf("%1.0f", dateEnd), ' } }')
filter <- paste(sep="", '{ "$and" : [ ', filterStart, ', ', filterEnd, ' ] }')
cat(sep="", "Filter: ", filter, "\n")

pingResults <- data.table(ping$find(filter, limit=16))
printPingResults(pingResults)


# ====== Traceroute Example =================================================
cat("###### Traceroute ######\n")

dateStart <- string_to_unix_time('2018-06-01 10:00:00.000000000')
dateEnd   <- string_to_unix_time('2018-06-01 10:00:01.000000000')
filterStart <- paste(sep="", '{ "timestamp": { "$gte" : ', sprintf("%1.0f", dateStart), ' } }')
filterEnd   <- paste(sep="", '{ "timestamp": { "$lt" : ',  sprintf("%1.0f", dateEnd), ' } }')
filter <- paste(sep="", '{ "$and" : [ ', filterStart, ', ', filterEnd, ' ] }')
cat(sep="", "Filter: ", filter, "\n")

tracerouteResults <- data.table(traceroute$find(filter))
printTracerouteResults(tracerouteResults)


# ====== Ping with Address Search Example ===================================
cat("###### Ping with Address Search ######\n")

dateStart <- string_to_unix_time('2018-08-01 11:11:10.000000000')
dateEnd   <- string_to_unix_time('2018-08-01 11:11:15.000000000')
address2  <- string_to_base64_ip("158.39.4.2")
address1  <- string_to_base64_ip("2001:700:4100:4::2")

filterStart  <- paste(sep="", '{ "timestamp": { "$gte" : ', sprintf("%1.0f", dateStart), ' } }')
filterEnd    <- paste(sep="", '{ "timestamp": { "$lt" : ',  sprintf("%1.0f", dateEnd), ' } }')
filterA1     <- paste(sep="", '{ "sourceIP": { "$type": "0", "$binary": "', address1, '" } }')
filterA2     <- paste(sep="", '{ "sourceIP": { "$type": "0", "$binary": "', address2, '" } }')
filterA1orA2 <- paste(sep="", '{ "$or" : [ ', filterA1, ', ', filterA2, ' ] }')
filter <- paste(sep="", '{ "$and" : [ ', filterStart, ', ', filterEnd, ', ', filterA1orA2 , ' ] }')
cat(sep="", "Filter: ", filter, "\n")

pingResults <- data.table(ping$find(filter))
printPingResults(pingResults)
