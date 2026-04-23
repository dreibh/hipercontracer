A Test with Public Servers
==========================

# Notes

[IETF](https://www.ietf.org), [tcpbin.com](https://tcpbin.com) and [Digi International](https://docs.digi.com/resources/documentation/digidocs/90002258/tasks/t_echo_server.htm) provide some public servers for testing.

Addresses tested on: March 23, 2026.

# ICMP

[IETF](https://www.ietf.org):

* 104.16.44.99
* 2606:4700::6810:2d63


HiPerConTracer run:

```bash
 sudo ../hipercontracer -U "$USER" \
    -# 2026032301 \
    -S 0.0.0.0 -S :: \
    -D 104.16.44.99 -D 2606:4700::6810:2d63 \
    -M ICMP \
    --ping \
    --resultsdirectory data --resultstransactionlength 3600
 ```

# UDP Echo

[Digi International](https://docs.digi.com/resources/documentation/digidocs/90002258/tasks/t_echo_server.htm) server:

* 52.43.121.77, port 10001
* 64:ff9b::342b:794d, port 10000

HiPerConTracer run:

 ```bash
 sudo ../hipercontracer -U "$USER" \
    -# 2026032302 \
    -S 0.0.0.0 -S :: \
    -D 52.43.121.77 -D 64:ff9b::342b:794d \
    -M UDP \
    --ping \
    --pingudpdestinationport 10001 \
    --resultsdirectory data --resultstransactionlength 3600
 ```

# TCP Echo

[tcpbin.com](https://tcpbin.com) server:

* 45.79.112.203, port 4242
* 2600:3c01::f03c:91ff:feab:f98b, port 4242

HiPerConTracer run:

 ```bash
 sudo ../hipercontracer -U "$USER" \
    -# 2026032303 \
    -S 0.0.0.0 -S :: \
    -D 45.79.112.203 -D 2600:3c01::f03c:91ff:feab:f98b \
    -M TCP \
    --ping \
    --pingtcpdestinationport 4242 \
    --pinginterval 10000 \
    --resultsdirectory data --resultstransactionlength 3600
 ```

# Wireshark Filter Rule:

 ```
 icmp || (icmpv6 && icmpv6.type < 130) || udp.port == 10001 ||  tcp.port == 4242
 ```

 Note: Set up ["Decode As" rules](https://www.wireshark.org/docs/wsug_html_chunked/ChCustProtocolDissectionSection.html#ChAdvDecodeAs) rules to to use the HiPerConTracer dissector for:

 * UDP, port 10001
 * TCP, port 4242
