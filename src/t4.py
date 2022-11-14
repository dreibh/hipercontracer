#!/usr/bin/python3

import ipaddress
import sys

addrs = [
      "8.8.8.8",
      "127.0.0.1",
      "193.99.144.80",
      "224.244.244.224",
      "2400:cb00:2048:1::6814:155",
      "::1",
      "3ffe::1:2:3:4",
      "2001:700:4100:101::1"
]

for i in range(0, len(addrs)):
   a = ipaddress.ip_address(addrs[i])
   sys.stdout.write("a" + str(i+1) + ": " + addrs[i] + "=" + str(a.packed) + "\n")
