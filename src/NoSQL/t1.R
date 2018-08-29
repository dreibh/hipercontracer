source("nornet-tools.R")


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
   text   <- binary_ip_to_string(list(binary))
   base64 <- string_to_base64_ip(test)

   cat(sep="\t", text, base64, "\n")
}
