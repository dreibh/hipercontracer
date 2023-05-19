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

library(anytime)
library(data.table)


# ###### Open file with support for decompression ###########################
openFile <- function(name)
{
   if(length(grep("\\.xz$", name)) > 0) {
      inputFile <- xzfile(name)
   }
   else if(length(grep("\\.bz2$", name)) > 0) {
      inputFile <- bzfile(name)
   }
   else if(length(grep("\\.gz$", name)) > 0) {
      inputFile <- gzfile(name)
   }
   else {
      inputFile <- open(name)
   }
   return(inputFile)
}


# ###### Read HiPerConTracer output file ####################################
readHiPerConTracerPingResults <- function(name)
{
   cat(sep="", "Trying to read ", name, " ...\n")

   inputFile <- openFile(name)
   lines <- readLines(inputFile)

   version   <- NA
   row       <- NA
   data      <- data.table()
   for(line in lines) {
      values <- strsplit(line, " +")[[1]]
      if(substr(values[1], 1, 2) == "#P") {
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
            row <- data.table(Timestamp    = 1000 * as.numeric(paste(sep="", "0x", values[4])),   # Convert to ns!
                              Protocol     = protocol,
                              Source       = values[2],
                              Destination  = values[3],
                              BurstSeq     = NA,
                              TrafficClass = as.numeric(paste(sep="", "0x", values[8])),
                              PacketSize   = as.numeric(values[9]),
                              Checksum     = as.numeric(paste(sep="", "0x", values[5])),
                              Status       = as.numeric(values[6]),
                              TimeSource   = NA,
                              RTT.App      = as.numeric(values[7]),
                              Queuing      = NA,
                              RTT.SW       = NA,
                              RTT.HW       = NA
                           )
         }
         # ====== Version 2 =================================================
         else {
            row <- data.table(Timestamp    = as.numeric(paste(sep="", "0x", values[4])),
                              Protocol     = protocol,
                              Source       = values[2],
                              Destination  = values[3],
                              BurstSeq     = as.numeric(values[5]),
                              TrafficClass = as.numeric(paste(sep="", "0x", values[6])),
                              PacketSize   = as.numeric(values[7]),
                              Checksum     = as.numeric(paste(sep="", "0x", values[8])),
                              Status       = as.numeric(values[9]),
                              TimeSource   = as.numeric(paste(sep="", "0x", values[10])),
                              RTT.App      = as.numeric(values[11]),
                              Queuing      = ifelse(as.numeric(values[12]) >= 0.0, as.numeric(values[12]), NA),
                              RTT.SW       = ifelse(as.numeric(values[13]) >= 0.0, as.numeric(values[13]), NA),
                              RTT.HW       = ifelse(as.numeric(values[14]) >= 0.0, as.numeric(values[14]), NA)
                           )
         }
         data <- rbind(data, row)
      }
      else {
         print(values)
         stop(paste(sep="", "ERROR: Bad input \"", line, "\"!"))
      }
   }
   return(data)
}


# # ###### Read HiPerConTracer output file ####################################
# readHiPerConTracerPingResults <- function(name)
# {
#    print(paste(sep="", "Trying to read ", name, " ..."))
#
#    # NOTE:See the "hipercontracer" manpage for the format description!
#    columns <- c(
#       "Ping",          # "#P"
#       "Source",        # Source address
#       "Destination",   # Destination address
#       "Timestamp",     # Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
#       "Checksum",      # Checksum (hexadeciaml)
#       "Status",        # Status (decimal)
#       "RTT",           # RTT in microseconds (decimal)
#       "TrafficClass",  # Traffic Class setting (hexadeciaml)
#       "PacketSize"     # Packet size, in bytes (decimal)
#    )
#    data <- fread(name, col.names = columns, header = FALSE)
#
#    # Convert time stamp with anytime() (time is in UTC!):
#    data$TimeStamp <- anytime(as.numeric(paste(sep="", "0x", data$TimeStamp)) / 1000000.0, tz="UTC")
#    # Convert RTT to ms:
#    data$RTT <- data$RTT / 1000.0
#    # Convert TrafficClass and Checksum to numbers
#    data$Checksum     <- as.numeric(paste(sep="", "0x", data$Checksum))
#    data$TrafficClass <- as.numeric(paste(sep="", "0x", data$TrafficClass))
#
#    return(data)
# }



# ###### Main program #######################################################

# ====== Handle arguments ===================================================
if( (length(commandArgs()) >= 1) && ((commandArgs()[2] == "--slave") || (commandArgs()[2] == "--no-echo")) ) {
   args <- commandArgs(trailingOnly = TRUE)
   if (length(args) < 1) {
     stop("Usage: r-ping-example ping-results-file [csv-file]")
   }
   name <- args[1]
   csv  <- args[2]
} else {
   # !!! NOTE: Set name here, for testing interactively in R! !!!
   name <- "Ping-P256751-0.0.0.0-20211212T125352.632431-000000001.results.bz2"
   csv  <- NA
}

# ====== Read input data ====================================================
data <- readHiPerConTracerPingResults(name)
print(summary(data))

# ====== Write CSV file =====================================================
if(!is.na(csv)) {
   cat(sep="", "Writing ", csv, " ...\n")
   write.csv(data, csv, quote = FALSE, row.names = FALSE)
}
