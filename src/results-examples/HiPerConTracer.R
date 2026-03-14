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
# Copyright (C) 2015-2026 by Thomas Dreibholz
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

suppressPackageStartupMessages({
   library("bit64")
   library("nanotime")
   options(digits.secs = 9)
   library("digest")
   library("data.table")
   library("dplyr")
   library("xtable")
})


# Path to HPCT Results:
hpct_results_tool = 'hpct-results'


# ###### Show table statistics ##############################################
showTableStatistics <- function(table)
{
   firstColumn <- colnames(table)[1]
   cat(sep="",
       "\x1b[32mEntries: ", length(table[[firstColumn]]), ", ",
       "Table Size: ", round(as.numeric(object.size(table)) / (1024*1024), 2), " MiB\x1b[0m\n")
}


# ###### Look up table from RDS file ########################################
cacheLookup <- function(cacheName)
{
   dataTable <- NULL
   if( (!is.na(cacheName)) && (file.exists(cacheName)) ) {
      cat(sep="", "\x1b[34mLoading cached table from ", cacheName, " ...\x1b[0m\n")
      dataTable <- readRDS(cacheName)
   }
   return(dataTable)
}


# ###### Store table as RDS file ############################################
cacheStorage <- function(cacheName, dataTable)
{
   if(!is.na(cacheName)) {
      cat(sep="", "\x1b[34mWriting data to cache file ", cacheName, " ...\x1b[0m\n")
      saveRDS(dataTable, cacheName)
   }
}


# ###### Process HiPerConTracer Ping results ################################
processHiPerConTracerPingResults <- function(dataTable)
{
   # print(colnames(dataTable))
   dataTable <- dataTable %>%
                   mutate(Timestamp    = as.nanotime(as.numeric(paste(sep="", "0x", Timestamp))),
                          Protocol     = recode(substr(Ping, 3, 1000000),
                                                "i" = "ICMP",
                                                "u" = "UDP",
                                                "t" = "TCP"),
                          IPVersion    = ifelse(grepl(":", DestinationIP), 6, 4),
                          Checksum     = as.numeric(paste(sep="", "0x", Checksum)),
                          TrafficClass = as.numeric(paste(sep="", "0x", TrafficClass))) %>%
                   arrange(Timestamp, MeasurementID, SourceIP, DestinationIP, BurstSeq)
   return(dataTable)
}


# ###### Process HiPerConTracer Traceroute results ##########################
processHiPerConTracerTracerouteResults <- function(dataTable)
{
   # print(colnames(dataTable))
   dataTable <- dataTable %>%
                   mutate(Timestamp    = as.nanotime(as.numeric(paste(sep="", "0x", Timestamp))),
                          Protocol     = recode(substr(Traceroute, 3, 1000000),
                                                "i" = "ICMP",
                                                "u" = "UDP",
                                                "t" = "TCP"),
                          IPVersion    = ifelse(grepl(":", DestinationIP), 6, 4),
                          Checksum     = as.numeric(paste(sep="", "0x", Checksum)),
                          TrafficClass = as.numeric(paste(sep="", "0x", TrafficClass)),
                          PathHash     = as.numeric(paste(sep="", "0x", PathHash)),
                          StatusFlags  = as.numeric(paste(sep="", "0x", StatusFlags))) %>%
                   arrange(Timestamp, MeasurementID, SourceIP, DestinationIP, RoundNumber, HopNumber) %>%
                   mutate(# Link source, if available:
                          LinkDestinationIP = ifelse( (Status < 200) | (Status == 255),
                                                      HopIP,
                                                      NA ),
                          # Link destination, if available:
                          LinkSourceIP = ifelse(HopNumber == 1,
                                               SourceIP,
                                               shift(LinkDestinationIP, 1, type="lag")))
   return(dataTable)
}


# ###### Process HiPerConTracer Jitter results ##############################
processHiPerConTracerJitterResults <- function(dataTable)
{
   # print(colnames(dataTable))
   dataTable <- dataTable %>%
                   mutate(Timestamp    = as.nanotime(as.numeric(paste(sep="", "0x", Timestamp))),
                          Protocol     = recode(substr(Jitter, 3, 1000000),
                                                "i" = "ICMP",
                                                "u" = "UDP",
                                                "t" = "TCP"),
                          IPVersion    = ifelse(grepl(":", DestinationIP), 6, 4),
                          Checksum     = as.numeric(paste(sep="", "0x", Checksum)),
                          TrafficClass = as.numeric(paste(sep="", "0x", TrafficClass))) %>%
                   arrange(Timestamp, MeasurementID, SourceIP, DestinationIP, BurstSeq)
   return(dataTable)
}


# ###### Read HiPerConTracer results from HiPerConTracer file ###############
readHiPerConTracerResults <- function(hpctFileName, processingFunction, cacheLabel = NA)
{
   cacheName <- NA
   if(!is.na(cacheLabel)) {
      cacheName <- paste(sep="", cacheLabel, "-cache-hpct-", digest(hpctFileName, "sha256"), ".rds")
   }

   # ====== Read data table =================================================
   dataTable <- cacheLookup(cacheName)
   if(is.null(dataTable)) {
      cat(sep="", "\x1b[34mReading ", hpctFileName, " ...\x1b[0m\n")

      inputCommand <- paste(sep=" ",
                            hpct_results_tool, hpctFileName, "--unsorted")
      dataTable <- fread(cmd = inputCommand)

      # ====== Post-processing ==============================================
      dataTable <- processingFunction(dataTable)

      cacheStorage(cacheName, dataTable)
   }
   showTableStatistics(dataTable)
   return(dataTable)
}


# ###### Read HiPerConTracer Ping results from HiPerConTracer file ##########
readHiPerConTracerPingResults <- function(hpctFileName, cacheLabel = NA)
{
   return(readHiPerConTracerResults(hpctFileName,
                                    processHiPerConTracerPingResults,
                                    cacheLabel))
}


# ###### Read HiPerConTracer Traceroute results from HiPerConTracer file ####
readHiPerConTracerTracerouteResults <- function(hpctFileName, cacheLabel = NA)
{
   return(readHiPerConTracerResults(hpctFileName,
                                    processHiPerConTracerTracerouteResults,
                                    cacheLabel))
}


# ###### Read HiPerConTracer results from CSV file ##########################
readHiPerConTracerResultsFromCSV <- function(csvFileName, processingFunction, cacheLabel = NA)
{
   cacheName <- NA
   if(!is.na(cacheLabel)) {
      cacheName <- paste(sep="", cacheLabel, "-cache-csv-", digest(csvFileName, "sha256"), ".rds")
   }

   # ====== Read data table =================================================
   dataTable <- cacheLookup(cacheName)
   if(is.null(dataTable)) {
      cat(sep="", "\x1b[34mReading ", csvFileName, " ...\x1b[0m\n")
      dataTable <- fread(csvFileName)

      # ====== Post-processing ==============================================
      dataTable <- processingFunction(dataTable)

      cacheStorage(cacheName, dataTable)
   }
   showTableStatistics(dataTable)
   return(dataTable)
}


# ###### Read HiPerConTracer Ping results from CSV file ######################
readHiPerConTracerPingResultsFromCSV <- function(csvFileName, cacheLabel = NA)
{
   return(readHiPerConTracerResultsFromCSV(csvFileName,
                                           processHiPerConTracerPingResults,
                                           cacheLabel))
}


# ###### Read HiPerConTracer Traceroute results from CSV file ################
readHiPerConTracerTracerouteResultsFromCSV <- function(csvFileName, cacheLabel = NA)
{
   return(readHiPerConTracerResultsFromCSV(csvFileName,
                                           processHiPerConTracerTracerouteResults,
                                           cacheLabel))
}


# ###### Read HiPerConTracer results from directory #########################
readHiPerConTracerResultsFromDirectory <- function(path, pattern, processingFunction, cacheLabel = NA)
{
   files <- list.files(path = path, pattern = pattern, full.names=TRUE)
   if(length(files) < 1) {
      stop("No input files found!")
   }
   cacheName <- NA
   if(!is.na(cacheLabel)) {
      cacheName <- paste(sep="", cacheLabel, "-cache-", digest(files, "sha256"), ".rds")
   }

   # ====== Read data table =================================================
   dataTable <- cacheLookup(cacheName)
   if(is.null(dataTable)) {
      cat(sep="", "\x1b[34mReading ", length(files), " files ...\x1b[0m\n")

      # ====== Generate files with input file names =========================
      filesName <- paste(sep="", cacheLabel, "-files-", digest(files, "sha256"), ".list")
      filesNameFile <- file(filesName, "w")
      cat(file = filesNameFile, paste(as.list(files), collapse = "\n"))
      cat(file = filesNameFile, "\n")
      close(filesNameFile)

      inputCommand <- paste(sep=" ",
                            hpct_results_tool,
                            "--unsorted",
                            "--input-file-names-from-file", filesName)
      dataTable <- fread(cmd = inputCommand)

      # ====== Post-processing ==============================================
      dataTable <- processingFunction(dataTable)

      cacheStorage(cacheName, dataTable)
   }
   showTableStatistics(dataTable)
   return(dataTable)
}


# ###### Read HiPerConTracer Ping results from directory ####################
readHiPerConTracerPingResultsFromDirectory <- function(path, pattern, cacheLabel = NA)
{
   return(readHiPerConTracerResultsFromDirectory(path, pattern,
                                                 processHiPerConTracerPingResults,
                                                 cacheLabel))
}


# ###### Read HiPerConTracer Traceroute results from directory ##############
readHiPerConTracerTracerouteResultsFromDirectory <- function(path, pattern, cacheLabel = NA)
{
   return(readHiPerConTracerResultsFromDirectory(path, pattern,
                                                 processHiPerConTracerTracerouteResults,
                                                 cacheLabel))
}


# ###### Read HiPerConTracer Jitter results from directory ##################
readHiPerConTracerJitterResultsFromDirectory <- function(path, pattern, cacheLabel = NA)
{
   return(readHiPerConTracerResultsFromDirectory(path, pattern,
                                                 processHiPerConTracerJitterResults,
                                                 cacheLabel))
}


# ###### Map timestamps to bins of given size ###############################
makeTimestampBins <- function(timestamp, binSizeInSeconds)
{
   binInNS   <- as.integer64(1e9) * as.integer64(binSizeInSeconds)
   tseconds  <- (as.integer64(timestamp) / binInNS) * binInNS
   timestamp <- nanotime(tseconds)
   return(timestamp)
}


# ###### plyr::mapvalues() replacement ######################################
# Copy from: https://github.com/hadley/plyr/blob/main/R/revalue.r
# ===========================================================================
#
#' Replace specified values with new values, in a vector or factor.
#'
#' Item in \code{x} that match items \code{from} will be replaced by
#' items in \code{to}, matched by position. For example, items in \code{x} that
#' match the first element in \code{from} will be replaced by the first
#' element of \code{to}.
#'
#' If \code{x} is a factor, the matching levels of the factor will be
#' replaced with the new values.
#'
#' The related \code{revalue} function works only on character vectors
#' and factors, but this function works on vectors of any type and factors.
#'
#' @param x the factor or vector to modify
#' @param from a vector of the items to replace
#' @param to a vector of replacement values
#' @param warn_missing print a message if any of the old values are
#'   not actually present in \code{x}
#'
#' @seealso \code{\link{revalue}} to do the same thing but with a single
#'   named vector instead of two separate vectors.
#' @export
#' @examples
#' x <- c("a", "b", "c")
#' mapvalues(x, c("a", "c"), c("A", "C"))
#'
#' # Works on factors
#' y <- factor(c("a", "b", "c", "a"))
#' mapvalues(y, c("a", "c"), c("A", "C"))
#'
#' # Works on numeric vectors
#' z <- c(1, 4, 5, 9)
#' mapvalues(z, from = c(1, 5, 9), to = c(10, 50, 90))
mapvalues <- function(x, from, to, warn_missing = TRUE) {
  if (length(from) != length(to)) {
    stop("`from` and `to` vectors are not the same length.")
  }
  if (!is.atomic(x) && !is.null(x)) {
    stop("`x` must be an atomic vector or NULL.")
  }

  if (is.factor(x)) {
    # If x is a factor, call self but operate on the levels
    levels(x) <- mapvalues(levels(x), from, to, warn_missing)
    return(x)
  }

  mapidx <- match(x, from)
  mapidxNA  <- is.na(mapidx)

  # index of items in `from` that were found in `x`
  from_found <- sort(unique(mapidx))
  if (warn_missing && length(from_found) != length(from)) {
    message("The following `from` values were not present in `x`: ",
      paste(from[!(1:length(from) %in% from_found) ], collapse = ", "))
  }

  x[!mapidxNA] <- to[mapidx[!mapidxNA]]
  x
}
