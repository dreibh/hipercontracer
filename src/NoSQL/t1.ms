use pingtraceroutedb

db.ping.insert([
   { source: "1.2.3.4",
     destination: "9.8.7.6",
     timestamp: NumberLong(123456789123445667),
     status: NumberInt(0x123),
     rtt: NumberInt(10234) },
   { source: "12.12.12.12",
     destination: "11.11.11.11",
     timestamp: ISODate("2018-01-10T08:09:10.567890"),
     status: NumberInt(0x123),
     rtt: NumberInt(32345) }
])
