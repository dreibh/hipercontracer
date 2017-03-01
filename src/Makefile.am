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
# icmpheader.h \
# ipv4header.h \
# ipv6header.h \
# traceserviceheader.h
# t1_LDADD = -lboost_system -lboost_filesystem -lboost_thread -lboost_date_time -lboost_iostreams -lpthread


dist_bin_SCRIPTS = tracedataimporter addressinfogenerator

dist_man_MANS = hipercontracer.1 tracedataimporter.1 addressinfogenerator.1

EXTRA_DIST = hipercontracer-database-configuration

SUBDIRS = SQL
