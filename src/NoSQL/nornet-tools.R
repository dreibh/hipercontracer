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

library("anytime")
library("assertthat")
library("bitops")
library("openssl")


# ###### Convert Unix time in nanoseconds to string #########################
unix_time_to_string <- function(timeStamp)
{
   seconds      <- as.character(anytime(timeStamp / 1000000000), asUTC=TRUE, tz="UTC")
   nanoseconds <- sprintf("%06d", timeStamp %% 1000000000)
   timeString <- paste(sep=".", seconds, nanoseconds)
}


# ###### Convert strong to Unix time in nanoseconds #########################
string_to_unix_time <- function(timeString)
{
   timeStamp <- 1000000000.0 * as.numeric(anytime(timeString, asUTC=TRUE))
   return(timeStamp)
}


# ###### Convert binary IPv4/IPv6 address to string #########################
binary_ip_to_string <- function(ipAddress)
{
   binaryIP <- as.numeric(ipAddress[[1]])
   # base64IP <- base64_encode( as.raw(ipAddress[[1]]) )

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


# ###### Extract colon-separated 16-bit hex values to 8-bit values ##########
get_blocks <- function(input)
{
   if(input != "") {
      blocks <- strsplit(input, ":", fixed=TRUE)[[1]]
      tuple <- rep(NA, 2 * length(blocks))
      for(i in 1:length(blocks)) {
         value <- strtoi(paste(sep="", "0x", as.character(blocks[i])))
         if( (value < 0) || (value > 0xffff)) {
            return(NA)
         }
         tuple[2*(i - 1) + 1] <- bitShiftR(bitAnd(value, 0xff00), 8)
         tuple[2*(i - 1) + 2] <- bitAnd(value, 0xff)
      }
      return(tuple)
   }
   else {
      return(c())   # Return empty list in case of empty input!
   }
}


# ###### Convert string to numeric IPv4/IPv6 address ########################
string_to_numeric_ip <- function(address)
{
   # ====== IPv4 ============================================================
   ipv4 <- strsplit(address, ".", fixed=TRUE)[[1]]
   if(length(ipv4) == 4) {
      tuple <- as.numeric(ipv4)
      return(as.raw(tuple))
   }

   # ====== IPv6 ============================================================
   else {
      # ------ Case #1: compressed format, :: at the beginning --------------
      if(grepl("^::", address)[[1]] == TRUE) {
         blocks <- strsplit(address, '::', fixed=TRUE)[[1]]
         if( (length(blocks) == 1) || (length(blocks) == 2) ) {
            # This also handles "::", where blocks[1]==""!
            back <- get_blocks(blocks[length(blocks)])
            if(length(back) < 16) {
               front <- rep(0, 16 - length(back))
               tuple <- c(front, back)
               return(as.raw(tuple))
            }
         }
      }
      # ------ Case #2: compressed format, :: at the end --------------------
      else if(grepl("::$", address)[[1]] == TRUE) {
         blocks <- strsplit(address, '::', fixed=TRUE)[[1]]
         if(length(blocks) == 2) {
            front <- get_blocks(blocks[1])
            if(length(front) < 16) {
               back <- rep(0, 16 - length(front))
               tuple <- c(front, back)
               return(as.raw(tuple))
            }
         }
      }
      # ------ Case #3: compressed format, :: in the middle -----------------
      else if(grepl("::", address)[[1]] == TRUE) {
         blocks <- strsplit(address, '::', fixed=TRUE)[[1]]
         if(length(blocks) == 2) {
            front <- get_blocks(blocks[1])
            back  <- get_blocks(blocks[2])
            if(length(front) + length(back) <= 16) {
               middle <- rep(0, 16 - (length(front) + length(back)))
               tuple <- c(front, middle, back)
               return(as.raw(tuple))
            }

         }
      }
      # ------ Case #4: uncompressed format ---------------------------------
      else {   # uncompressed format
         tuple <- get_blocks(address)
         if(length(tuple) == 16) {
            return(as.raw(tuple))
         }
      }
   }
   return(NA)
}


# ###### Convert string to numeric base64-encoded IPv4/IPv6 address #########
string_to_base64_ip <- function(address)
{
   n <- string_to_numeric_ip(address)
   if(length(n) >= 4) {
      return(base64_encode(as.raw(n)))
   }
   return(NA)
}


# ###### Print Ping results #################################################
printPingResults <- function(pingResults)
{
   if(length(pingResults) > 0) {
      for(i in 1:length(pingResults$timestamp)) {
         pingResult <- pingResults[i]

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
}


# ###### Print Traceroute results ############################################
printTracerouteResults <- function(tracerouteResults)
{
   if(length(tracerouteResults) > 0) {
      for(i in 1:length(tracerouteResults$timestamp)) {
         tracerouteResult <- tracerouteResults[i]

         timestamp   <- unix_time_to_string(tracerouteResult$timestamp)
         source      <- binary_ip_to_string(tracerouteResult$source)
         destination <- binary_ip_to_string(tracerouteResult$destination)

         cat(sep="", sprintf("%4d", i), ": ", timestamp, " (", sprintf("%1.0f", tracerouteResult$timestamp), ")\t",
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
}
