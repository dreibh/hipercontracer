bin_PROGRAMS = hipercontracer   # t1

hipercontracer_SOURCES = hipercontracer.cc \
icmpheader.h \
ipv4header.h \
ipv6header.h \
traceserviceheader.h \
traceroute.cc \
traceroute.h \
service.h \
sqlwriter.cc \
sqlwriter.h \
ping.cc \
ping.h
hipercontracer_LDADD = -lboost_system -lboost_filesystem -lboost_thread -lboost_date_time -lboost_iostreams -lpthread


# t1_SOURCES = t1.cc \
# sqlwriter.cc \
# sqlwriter.h
# t1_LDADD = -lboost_system -lboost_filesystem -lboost_thread -lboost_date_time -lboost_iostreams -lpthread


EXTRA_DIST = tracedataimporter trace-configuration

dist_man_MANS = hipercontracer.1 tracedataimporter.1
