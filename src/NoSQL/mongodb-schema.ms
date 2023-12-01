// ###### Ping ##############################################################
use ${DATABASE}

db.createCollection("ping", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.ping.createIndex( { sendTimestamp: 1, measurementID: 1, sourceIP: 1, destinationIP: 1, protocol: 1, trafficClass: 1, burstSeq: 1 }, { unique: true } )

db.ping.createIndex( { measurementID: 1, destination: 1, sendTimestamp: 1 } )


// ###### Traceroute ########################################################
use ${DATABASE}

db.createCollection("traceroute", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.traceroute.createIndex( { timestamp: 1, measurementID: 1, sourceIP: 1, destinationIP: 1, protocol: 1, trafficClass: 1, roundNumber: 1 }, { unique: true } )

db.traceroute.createIndex( { measurementID: 1, destinationIP: 1, timestamp: 1 } )


// ###### Jitter ############################################################
use ${DATABASE}

db.createCollection("jitter", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.jitter.createIndex( { timestamp: 1, measurementID: 1, sourceIP: 1, destinationIP: 1, protocol: 1, trafficClass: 1, roundNumber: 1 }, { unique: true } )

db.jitter.createIndex( { measurementID: 1, destinationIP: 1, timestamp: 1 } )


quit()
