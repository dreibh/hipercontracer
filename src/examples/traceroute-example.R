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
library(assert)
library(data.table)


# ###### Read HiPerConTracer output file ####################################
readHiPerConTracerTracerouteResults <- function(name)
{
   print(paste(sep="", "Trying to read ", name, " ..."))
   inputFile <- xzfile(name)
   lines     <- readLines(inputFile)

   row       <- NA
   hop       <- 0
   data      <- data.table()
   for(line in lines) {
      values <- strsplit(line, " +")[[1]]
      if(substr(values[1], 1, 2) == "#T") {
         protocol <- substr(values[1], 3, 1000000)
         if(protocol == "i") {
            protocol <- "ICMP"
         }
         else if(protocol == "u") {
            protocol <- "UDP"
         }
         else {
            stop(paste(sep="", "ERROR: Unexpected protocol \"", line, "\"!"))
         }

         row <- data.table(Timestamp       = as.numeric(paste(sep="", "0x", values[4])),
                           Protocol        = protocol,
                           Source          = values[2],
                           Destination     = values[3],
                           Round           = as.numeric(values[5]),
                           TotalHops       = as.numeric(values[6]),
                           TotalHops       = as.numeric(paste(sep="", "0x", values[7])),
                           PacketSize      = as.numeric(values[8]),
                           Checksum        = as.numeric(paste(sep="", "0x", values[9])),
                           StatusFlags     = as.numeric(paste(sep="", "0x", values[10])),
                           PathHash        = as.numeric(paste(sep="", "0x", values[11])),
                           Hop             = NA,
                           Status          = NA,
                           LinkSource      = values[2],
                           LinkDestination = NA,
                           RTT.App         = NA,
                           Queuing         = NA,
                           RTT.SW          = NA,
                           RTT.HW          = NA
                          )
         hop <- 1
         print(row)
      }
      else if(substr(values[1], 1, 1) == "\t") {
         # Remove "\t" from first value:
         values[1] <- substr(values[1], 2, 1000000)

         row$Hop             <- as.numeric(values[1])
         row$Status          <- as.numeric(values[2])
         row$TimeSource      <- as.numeric(paste(sep="", "0x", values[3]))
         row$RTT.App         <- ifelse(as.numeric(values[4]) >= 0.0, as.numeric(values[4]), NA)
         row$Queuing         <- ifelse(as.numeric(values[5]) >= 0.0, as.numeric(values[5]), NA)
         row$RTT.SW          <- ifelse(as.numeric(values[6]) >= 0.0, as.numeric(values[6]), NA)
         row$RTT.HW          <- ifelse(as.numeric(values[7]) >= 0.0, as.numeric(values[7]), NA)
         row$LinkDestination <- ifelse(is.na(values[8]), "*", values[8])
         print(values)
         print(row)

         assert(hop == row$Hop)
         hop <- hop + 1

         data <- rbind(data, row)

         row$LinkSource <- row$LinkDestination
      }
      else {
         print(values)
         stop(paste(sep="", "ERROR: Bad input \"", line, "\"!"))
      }
   }
   return(data)
}



# ###### Main program #######################################################

# ====== Handle arguments ===================================================
if( (length(commandArgs()) >= 1) && ((commandArgs()[2] == "--slave") || (commandArgs()[2] == "--no-echo")) ) {
   args <- commandArgs(trailingOnly = TRUE)
   if (length(args) < 1) {
     stop("Usage: r-traceroute-example traceroute-results-file")
   }
   name <- args[1]
} else {
   # !!! NOTE: Set name here, for testing interactively in R! !!!
   name <- "Traceroute-ICMP-P293172-192.168.0.16-20230519T102355.319475-000000001.results.xz"
   name <- "Traceroute-UDP-P293172-192.168.0.16-20230519T102355.320500-000000001.results.xz"
}

data <- readHiPerConTracerTracerouteResults(name)
print(colnames(data))
print(summary(data))

write.csv(data, "traceroute.csv", quote = FALSE, row.names = FALSE)
