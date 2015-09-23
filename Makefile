traceservice:	traceservice.cc icmpheader.h  ipv4header.h  ipv6header.h  traceserviceheader.h
	colorgcc traceservice.cc -o traceservice -O2 -Wall -g -lboost_system -lboost_thread -std=c++11 -lboost_date_time -lstdc++
