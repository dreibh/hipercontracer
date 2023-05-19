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
library(data.table)
library(dplyr, warn.conflicts = FALSE)
library(readr)


# ###### Read HiPerConTracer output file ####################################
readHiPerConTracerPingResults <- function(name)
{
   cat(sep="", "Trying to read ", name, " ...\n")
   inputData <- read_file(name)

   # ====== Identify version ================================================
   if(substr(inputData, 1, 3) == "#P ") {
      version <- 1
   }
   else if(substr(inputData, 1, 2) == "#P") {
      version <- 2
   }

   # ====== Version 1 =======================================================
   if(version == 1) {
      columns <- c(
         "Ping",           # "#P"
         "Source",         # Source address
         "Destination",    # Destination address
         "Timestamp",      # Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
         "Checksum",       # Checksum (hexadeciaml)
         "Status",         # Status (decimal)
         "RTT.App",        # RTT in microseconds (decimal)
         "TrafficClass",   # Traffic Class setting (hexadeciaml)
         "PacketSize"      # Packet size, in bytes (decimal)
      )
      data <- fread(text = inputData, sep = " ", col.names = columns, header = FALSE) %>%
                 mutate(Timestamp    = 1000 * as.numeric(paste(sep="", "0x", Timestamp)),   # Convert to ns!
                        Checksum     = as.numeric(paste(sep="", "0x", Checksum)),
                        TrafficClass = as.numeric(paste(sep="", "0x", TrafficClass)),
                        RTT.App      = 1000 * RTT.App,   # Convert to ns!
                        Protocol     = "ICMP",
                        BurstSeq     = NA,
                        TimeSource   = NA,
                        Queuing      = NA,
                        RTT.SW       = NA,
                        RTT.HW       = NA)
   }

   # ====== Version 2 =======================================================
   else if(version == 2) {
      columns <- c(
         "Ping",           # "#P"
         "Source",         # Source address
         "Destination",    # Destination address
         "Timestamp",      # Timestamp (nanoseconds since the UTC epoch, hexadecimal).
         "BurstSeq",       # Sequence number within a burst (decimal), numbered from 0.
         "TrafficClass",   # Traffic Class setting (hexadeciaml)
         "PacketSize",     # Packet size, in bytes (decimal)
         "Checksum",       # Checksum (hexadeciaml)
         "Status",         # Status (decimal)
         "TimeSource",     # Source of the timing information (hexadecimal) as: AAQQSSHH
         "RTT.App",        # The measured application RTT (nanoseconds, decimal).
         "Queuing",        # The measured kernel software queuing delay (nanoseconds, decimal; -1 if not available).
         "RTT.SW",         # The measured kernel software RTT (nanoseconds, decimal; -1 if not available).
         "RTT.HW"          # The measured kernel hardware RTT (nanoseconds, decimal; -1 if not available).
      )
      data <- fread(text = inputData, sep = " ", col.names = columns, header = FALSE) %>%
                 mutate(Timestamp    = as.numeric(paste(sep="", "0x", Timestamp)),
                        Protocol     = substr(Ping, 3, 1000000),
                        Checksum     = as.numeric(paste(sep="", "0x", Checksum)),
                        TrafficClass = as.numeric(paste(sep="", "0x", TrafficClass)))

      data$Protocol[data$Protocol == "i"] <- "ICMP"
      data$Protocol[data$Protocol == "u"] <- "UDP"
   }

   else {
      stop(paste(sep="", "ERROR: Unexpected version \"", lines[[1]], "\"!"))
   }

   data <- data %>%
              relocate(Timestamp, Protocol, Source, Destination,
                       BurstSeq, TrafficClass, PacketSize, Checksum, Status,
                       TimeSource, RTT.App, Queuing, RTT.SW, RTT.HW) %>%
              select(!Ping)
   return(data)
}



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
   name <- "Ping-ICMP-P294036-192.168.0.16-20230519T105110.860718-000000001.results.xz"
   # name <- "Ping-P256751-0.0.0.0-20211212T125352.632431-000000001.results.bz2"
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
