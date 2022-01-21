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
#  Copyright (C) 2015-2022 by Thomas Dreibholz
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


# ###### Read HiPerConTracer output file ####################################
readHiPerConTracerResults <- function(name)
{
   print(paste(sep="", "Trying to read ", name, " ..."))

   # NOTE:See the "hipercontracer" manpage for the format description!
   columns <- c(
      "Ping",          # "#P"
      "Source",        # Source address
      "Destination",   # Destination address
      "TimeStamp",     # Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
      "Checksum",      # Checksum (hexadeciaml)
      "Status",        # Status (decimal)
      "RTT",           # RTT in microseconds (decimal)
      "TrafficClass",  # Traffic Class setting (hexadeciaml)
      "PacketSize"     # Packet size, in bytes (decimal)
   )
   data <- fread(name, col.names = columns, header = FALSE)

   # Convert time stamp with anytime() (time is in UTC!):
   data$TimeStamp <- anytime(as.numeric(paste(sep="", "0x", data$TimeStamp)) / 1000000.0, tz="UTC")
   # Convert RTT to ms:
   data$RTT <- data$RTT / 1000.0
   # Convert TrafficClass and Checksum to numbers
   data$Checksum     <- as.numeric(paste(sep="", "0x", data$Checksum))
   data$TrafficClass <- as.numeric(paste(sep="", "0x", data$TrafficClass))

   return(data)
}



# ###### Main program #######################################################

# ====== Handle arguments ===================================================
if( (length(commandArgs()) >= 1) && ((commandArgs()[2] == "--slave") || (commandArgs()[2] == "--no-echo")) ) {
   args <- commandArgs(trailingOnly = TRUE)
   if (length(args) < 1) {
     stop("Usage: r-example.R name")
   }
   name <- args[1]
} else {
   # !!! NOTE: Set name here, for testing interactively in R! !!!
   name <- "Ping-P256751-0.0.0.0-20211212T125352.632431-000000001.results.bz2"
}

data <- readHiPerConTracerResults(name)
summary(data)
