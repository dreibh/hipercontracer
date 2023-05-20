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
library(dplyr, warn.conflicts = FALSE)
library(readr)


# ###### Read HiPerConTracer output file ####################################
readHiPerConTracerTracerouteResults <- function(name)
{
   cat(sep="", "Trying to read ", name, " ...\n")
   inputData <- read_file(name)

   # ====== Identify version ================================================
   version <- NA
   if(substr(inputData, 1, 3) == "#T ") {
      version <- 1
   }
   else if(substr(inputData, 1, 2) == "#T") {
      version <- 2
   }
   else {
      stop("Unexpected format!")
   }

   # ====== Combine TAB lines to their header, for faster processing ========
   originalLines <- strsplit(inputData, "\n")[[1]]
   combinedLines <- list()
   head <- NA
   rm(inputData)
   for(line in originalLines) {
      if(substr(line, 1, 1) == "#") {
         head <- line
      }
      else if(substr(line, 1, 1) == "\t") {
         combined <- paste(head, "\t", substr(line, 2, 1000000))
         combinedLines <- append(combinedLines, combined)
      }
      else {
         stop("Unexpected format!")
      }
   }
   rm(originalLines)
   inputData <- paste(sep="\n", combinedLines)
   rm(combinedLines)

   # ====== Version 1 =======================================================
   if(version == 1) {
      columns <- c(
         "Traceroute",        # "#T"
         "Source",            # Source address
         "Destination",       # Destination address
         "Timestamp",         # Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
         "Round",             # Round number (decimal)
         "Checksum",          # Checksum (hexadeciaml)
         "TotalHops",         # Total hops (decimal)
         "StatusFlags",       # Status flags (hexadecimal)
         "PathHash",          # Hash of the path (hexadecimal)
         "TrafficClass",      # Traffic Class setting (hexadeciaml)
         "PacketSize",        # Packet size, in bytes (decimal)

         "TAB",               # NOTE: must be "\t" from combination above!
         "HopNumber",         # Number of the hop.
         "Status",            # Status code (in hexadecimal here!)
         "RTT.App",           # RTT in microseconds (decimal)
         "LinkDestination"    # Hop IP address.
      )
      data <- fread(text = inputData, sep = " ", col.names = columns, header = FALSE) %>%
                 mutate(Timestamp        = 1000 * as.numeric(paste(sep="", "0x", Timestamp)),   # Convert to ns!
                        Checksum         = as.numeric(paste(sep="", "0x", Checksum)),
                        TrafficClass     = as.numeric(paste(sep="", "0x", TrafficClass)),
                        PathHash         = as.numeric(paste(sep="", "0x", PathHash)),
                        StatusFlags      = as.numeric(paste(sep="", "0x", StatusFlags)),
                        Status           = as.numeric(paste(sep="", "0x", Status)),
                        RTT.App          = 1000 * RTT.App,   # Convert to ns!
                        Protocol         = "ICMP",
                        TimeSource       = NA,
                        Delay.AppSend    = NA,
                        Delay.Queuing    = NA,
                        Delay.AppReceive = NA,
                        RTT.SW           = NA,
                        RTT.HW           = NA)
   }

   # ====== Version 2 =======================================================
   else if(version == 2) {
      columns <- c(
         "Traceroute",         # "#T<p>"
         "Source",             # Source address
         "Destination",        # Destination address
         "Timestamp",          # Absolute time since the epoch in UTC, in microseconds (hexadeciaml)
         "Round",              # Round number (decimal)
         "TotalHops",          # Total hops (decimal)
         "TrafficClass",       # Traffic Class setting (hexadeciaml)
         "PacketSize",         # Packet size, in bytes (decimal)
         "Checksum",           # Checksum (hexadeciaml)
         "StatusFlags",        # Status flags (hexadecimal)
         "PathHash",           # Hash of the path (hexadecimal)

         "TAB",                # NOTE: must be "\t" from combination above!
         "HopNumber",          # Number of the hop.
         "Status",             # Status code (decimal!)
         "TimeSource",         # Source of the timing information (hexadecimal) as: AAQQSSHH
         "Delay.AppSend",      # The measured application send delay (nanoseconds, decimal; -1 if not available).
         "Delay.Queuing",      # The measured kernel software queuing delay (nanoseconds, decimal; -1 if not available).
         "Delay.AppReceive",   # The measured application receive delay (nanoseconds, decimal; -1 if not available).
         "RTT.App",            # The measured application RTT (nanoseconds, decimal).
         "RTT.SW",             # The measured kernel software RTT (nanoseconds, decimal; -1 if not available).
         "RTT.HW",             # The measured kernel hardware RTT (nanoseconds, decimal; -1 if not available).
         "LinkDestination"     # Hop IP address.
      )
      print(inputData)
      data <- fread(text = inputData, sep = " ", col.names = columns, header = FALSE) %>%
                 mutate(Timestamp    = as.numeric(paste(sep="", "0x", Timestamp)),
                        Protocol     = substr(Traceroute, 3, 1000000),
                        Checksum     = as.numeric(paste(sep="", "0x", Checksum)),
                        TrafficClass = as.numeric(paste(sep="", "0x", TrafficClass)),
                        PathHash     = as.numeric(paste(sep="", "0x", PathHash)),
                        StatusFlags  = as.numeric(paste(sep="", "0x", StatusFlags)))

      data$Protocol[data$Protocol == "i"] <- "ICMP"
      data$Protocol[data$Protocol == "u"] <- "UDP"
   }

   # ====== Post-processing =================================================
   data <- data %>%
              # ------ Remove unnecessary columns ---------------------------
              select(!c("Traceroute", "TAB")) %>%
              # ------ Set LinkSource ---------------------------------------
              mutate(LinkSource =
                        ifelse(HopNumber == 1,
                           Source,
                           shift(LinkDestination, 1, type="lag"))) %>%
              # ------ Reorder entries --------------------------------------
              relocate(Timestamp, Protocol, Source, Destination,
                       Round, HopNumber, TotalHops, TrafficClass, PacketSize, Checksum, StatusFlags, Status, PathHash,
                       LinkSource, LinkDestination,
                       TimeSource,
                       Delay.AppSend, Delay.Queuing, Delay.AppReceive,
                       RTT.App, RTT.SW, RTT.HW)

   return(data)

#    version   <- NA
#    row       <- NA
#    rowList   <- list()
#    hop       <- 0
#    data      <- data.table()
#    for(line in combinedLines) {
#       values <- strsplit(line, " +")[[1]]
#       if(substr(values[1], 1, 2) == "#T") {
#          protocol <- substr(values[1], 3, 1000000)
#          if(protocol == "") {
#             protocol <- "ICMP"
#             version  <- 1
#          }
#          else if(protocol == "i") {
#             protocol <- "ICMP"
#             version  <- 2
#          }
#          else if(protocol == "u") {
#             protocol <- "UDP"
#             version  <- 2
#          }
#          else {
#             stop(paste(sep="", "ERROR: Unexpected protocol \"", line, "\"!"))
#          }
#
#          # ====== Version 1 =================================================
#          if(version == 1) {
#             row <- data.table(Timestamp       = 1000 * as.numeric(paste(sep="", "0x", values[4])),   # Convert to ns!
#                               Protocol        = protocol,
#                               Source          = values[2],
#                               Destination     = values[3],
#                               Round           = as.numeric(values[5]),
#                               TotalHops       = as.numeric(values[7]),
#                               TrafficClass    = as.numeric(paste(sep="", "0x", values[10])),
#                               PacketSize      = as.numeric(values[11]),
#                               Checksum        = as.numeric(paste(sep="", "0x", values[6])),
#                               StatusFlags     = as.numeric(paste(sep="", "0x", values[8])),
#                               PathHash        = as.numeric(paste(sep="", "0x", values[9])),
#                               Hop             = NA,
#                               Status          = NA,
#                               LinkSource      = values[2],
#                               LinkDestination = NA,
#                               RTT.App         = NA,
#                               Queuing         = NA,
#                               RTT.SW          = NA,
#                               RTT.HW          = NA
#                            )
#          }
#          # ====== Version 2 =================================================
#          else {
#             row <- data.table(Timestamp       = as.numeric(paste(sep="", "0x", values[4])),
#                               Protocol        = protocol,
#                               Source          = values[2],
#                               Destination     = values[3],
#                               Round           = as.numeric(values[5]),
#                               TotalHops       = as.numeric(values[6]),
#                               TrafficClass    = as.numeric(paste(sep="", "0x", values[7])),
#                               PacketSize      = as.numeric(values[8]),
#                               Checksum        = as.numeric(paste(sep="", "0x", values[9])),
#                               StatusFlags     = as.numeric(paste(sep="", "0x", values[10])),
#                               PathHash        = as.numeric(paste(sep="", "0x", values[11])),
#                               Hop             = NA,
#                               Status          = NA,
#                               LinkSource      = values[2],
#                               LinkDestination = NA,
#                               TimeSource      = NA,
#                               RTT.App         = NA,
#                               Queuing         = NA,
#                               RTT.SW          = NA,
#                               RTT.HW          = NA
#                            )
#          }
#          hop <- 1
#       }
#       else if(substr(values[1], 1, 1) == "\t") {
#          # ====== Version 1 =================================================
#          if(version == 1) {
#             # "\t" has own value!
#             row$Hop             <- as.numeric(values[2])
#             row$Status          <- as.numeric(paste(sep="", "0x", values[3]))
#             row$RTT.App         <- 1000 * as.numeric(values[4])   # Convert to ns!
#             row$LinkDestination <- ifelse(is.na(values[5]), "*", values[5])
#          }
#          # ====== Version 2 =================================================
#          else {
#             row$Hop             <- as.numeric(substr(values[1], 2, 1000000))   # Remove "\t" from first value!
#             row$Status          <- as.numeric(values[2])
#             row$TimeSource      <- as.numeric(paste(sep="", "0x", values[3]))
#             row$RTT.App         <- as.numeric(values[4])
#             row$Queuing         <- ifelse(as.numeric(values[5]) >= 0.0, as.numeric(values[5]), NA)
#             row$RTT.SW          <- ifelse(as.numeric(values[6]) >= 0.0, as.numeric(values[6]), NA)
#             row$RTT.HW          <- ifelse(as.numeric(values[7]) >= 0.0, as.numeric(values[7]), NA)
#             row$LinkDestination <- ifelse(is.na(values[8]), "*", values[8])
#          }
#          assert(hop == row$Hop)
#
#          rowList <- append(rowList, list(row))
#
#          # The source for the next hop is the current hop:
#          row$LinkSource <- row$LinkDestination
#          hop            <- hop + 1
#       }
#       else {
#          stop(paste(sep="", "ERROR: Bad input \"", line, "\"!"))
#       }
#    }
#
#    data <- data.table::rbindlist(rowList)
#    return(data)
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
   # name <- "Traceroute-P13735-158.39.4.2-20221012T142120.713464-000002601.results.bz2"
   name <- "Traceroute-ICMP-P42416-10.44.35.50-20230520T084931.149494-000000001.results.xz"
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
