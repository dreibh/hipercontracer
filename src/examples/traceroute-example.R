#!/usr/bin/Rscript
#  =================================================================
#           #     #                 #     #
#           ##    #   ####   #####  ##    #  ######   #####
#           # #   #  #    #  #    # # #   #  #          #
#           #  #  #  #    #  #    # #  #  #  #####      #
#           #   # #  #    #  #####  #   # #  #          #
#           #    ##  #    #  #   #  #    ##  #          #
#           #     #   ####   #    # #     #  ######     #
#
#        ---   The NorNet Testbed for Multi-Homed Systems  ---
#                        https://www.nntb.no
#  =================================================================
#
#  High-Performance Connectivity Tracer (HiPerConTracer)
#  Copyright (C) 2015-2023 by Thomas Dreibholz
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#  Contact: dreibh@simula.no

# library(anytime)
library(assert)
library(data.table)
library(readr)


# ###### Read HiPerConTracer output file ####################################
readHiPerConTracerTracerouteResults <- function(name)
{
   cat(sep="", "Trying to read ", name, " ...\n")

   inputData <- read_file(name)
   lines <- strsplit(inputData, "\n")[[1]]

   version   <- NA
   row       <- NA
   rowList   <- list()
   hop       <- 0
   data      <- data.table()
   for(line in lines) {
      values <- strsplit(line, " +")[[1]]
      if(substr(values[1], 1, 2) == "#T") {
         protocol <- substr(values[1], 3, 1000000)
         if(protocol == "") {
            protocol <- "ICMP"
            version  <- 1
         }
         else if(protocol == "i") {
            protocol <- "ICMP"
            version  <- 2
         }
         else if(protocol == "u") {
            protocol <- "UDP"
            version  <- 2
         }
         else {
            stop(paste(sep="", "ERROR: Unexpected protocol \"", line, "\"!"))
         }

         # ====== Version 1 =================================================
         if(version == 1) {
            row <- data.table(Timestamp       = 1000 * as.numeric(paste(sep="", "0x", values[4])),   # Convert to ns!
                              Protocol        = protocol,
                              Source          = values[2],
                              Destination     = values[3],
                              Round           = as.numeric(values[5]),
                              TotalHops       = as.numeric(values[7]),
                              TrafficClass    = as.numeric(paste(sep="", "0x", values[10])),
                              PacketSize      = as.numeric(values[11]),
                              Checksum        = as.numeric(paste(sep="", "0x", values[6])),
                              StatusFlags     = as.numeric(paste(sep="", "0x", values[8])),
                              PathHash        = as.numeric(paste(sep="", "0x", values[9])),
                              Hop             = NA,
                              Status          = NA,
                              LinkSource      = values[2],
                              LinkDestination = NA,
                              RTT.App         = NA,
                              Queuing         = NA,
                              RTT.SW          = NA,
                              RTT.HW          = NA
                           )
         }
         # ====== Version 2 =================================================
         else {
            row <- data.table(Timestamp       = as.numeric(paste(sep="", "0x", values[4])),
                              Protocol        = protocol,
                              Source          = values[2],
                              Destination     = values[3],
                              Round           = as.numeric(values[5]),
                              TotalHops       = as.numeric(values[6]),
                              TrafficClass    = as.numeric(paste(sep="", "0x", values[7])),
                              PacketSize      = as.numeric(values[8]),
                              Checksum        = as.numeric(paste(sep="", "0x", values[9])),
                              StatusFlags     = as.numeric(paste(sep="", "0x", values[10])),
                              PathHash        = as.numeric(paste(sep="", "0x", values[11])),
                              Hop             = NA,
                              Status          = NA,
                              LinkSource      = values[2],
                              LinkDestination = NA,
                              TimeSource      = NA,
                              RTT.App         = NA,
                              Queuing         = NA,
                              RTT.SW          = NA,
                              RTT.HW          = NA
                           )
         }
         hop <- 1
      }
      else if(substr(values[1], 1, 1) == "\t") {
         # ====== Version 1 =================================================
         if(version == 1) {
            # "\t" has own value!
            row$Hop             <- as.numeric(values[2])
            row$Status          <- as.numeric(paste(sep="", "0x", values[3]))
            row$RTT.App         <- 1000 * as.numeric(values[4])   # Convert to ns!
            row$LinkDestination <- ifelse(is.na(values[5]), "*", values[5])
         }
         # ====== Version 2 =================================================
         else {
            row$Hop             <- as.numeric(substr(values[1], 2, 1000000))   # Remove "\t" from first value!
            row$Status          <- as.numeric(values[2])
            row$TimeSource      <- as.numeric(paste(sep="", "0x", values[3]))
            row$RTT.App         <- as.numeric(values[4])
            row$Queuing         <- ifelse(as.numeric(values[5]) >= 0.0, as.numeric(values[5]), NA)
            row$RTT.SW          <- ifelse(as.numeric(values[6]) >= 0.0, as.numeric(values[6]), NA)
            row$RTT.HW          <- ifelse(as.numeric(values[7]) >= 0.0, as.numeric(values[7]), NA)
            row$LinkDestination <- ifelse(is.na(values[8]), "*", values[8])
         }
         assert(hop == row$Hop)

         rowList <- append(rowList, list(row))

         # The source for the next hop is the current hop:
         row$LinkSource <- row$LinkDestination
         hop            <- hop + 1
      }
      else {
         stop(paste(sep="", "ERROR: Bad input \"", line, "\"!"))
      }
   }

   data <- data.table::rbindlist(rowList)
   return(data)
}



# ###### Main program #######################################################

# ====== Handle arguments ===================================================
if( (length(commandArgs()) >= 1) && ((commandArgs()[2] == "--slave") || (commandArgs()[2] == "--no-echo")) ) {
   args <- commandArgs(trailingOnly = TRUE)
   if (length(args) < 1) {
     stop("Usage: r-traceroute-example traceroute-results-file [csv-file]")
   }
   name <- args[1]
   csv  <- args[2]
} else {
   # !!! NOTE: Set name here, for testing interactively in R! !!!
   name <- "Traceroute-ICMP-P293172-192.168.0.16-20230519T102355.319475-000000001.results.xz"
   csv  <- NA
}

# ====== Read input data ====================================================
data <- readHiPerConTracerTracerouteResults(name)
print(summary(data))

# ====== Write CSV file =====================================================
if(!is.na(csv)) {
   cat(sep="", "Writing ", csv, " ...\n")
   write.csv(data, csv, quote = FALSE, row.names = FALSE)
}
