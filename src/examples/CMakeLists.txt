#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES HiPerConTracer.R
              Ping-ICMP-P56277-10.44.33.50-20230520T085512.537837-000000003.results.xz
              Ping-P13735-158.39.4.2-20221012T142120.681066-000003330.results.bz2
              Ping-P13735-2001:700:4100:4::2-20221012T142120.713761-000003330.results.bz2
              Ping-P256751-0.0.0.0-20211212T125352.632431-000000001.results.bz2
              Ping-P79025-0.0.0.0-20221115T161345.273650-000000001.results.bz2
              Ping-UDP-P56277-10.44.33.50-20230520T085512.541049-000000003.results.xz
              README.md
              r-ping-example
              r-traceroute-example
              Traceroute-ICMP-P42416-10.44.35.50-20230520T084931.149494-000000001.results.xz
              Traceroute-P13735-158.39.4.2-20221012T142120.713464-000002601.results.bz2
              Traceroute-P13735-2001:700:4100:4::2-20221012T142120.713994-000002722.results.bz2
              Traceroute-P258235-0.0.0.0-20211212T132253.414640-000000001.results.bz2
              Traceroute-P79025-0.0.0.0-20221115T161345.274003-000000001.results.bz2
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/hipercontracer/examples)
