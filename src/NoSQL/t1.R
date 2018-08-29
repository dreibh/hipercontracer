library("bitops")

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


tests <- c(
 "192.168.1.1",
 "fd00:16:1:0:0:0:0:1",
 "fd00:16:1:0:0:0:0:2",
 "fd55:66::1",
 "::1",
 "::",
 "fd55:66::2::"
)

for(test in tests) {
   cat(sep="", test, " -> ")
   
   binary <- string_to_numeric_ip(test)
#    print(list(binary))
   text   <- binary_ip_to_string(list(binary))
   base64 <- string_to_base64_ip(test)

   cat(sep="\t", text, base64, "\n")
}
