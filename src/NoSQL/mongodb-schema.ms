// ###### Ping ##############################################################
use ${DATABASE}

db.createCollection("ping",       { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.ping.createIndex( { timestamp: 1, source: 1, destination: 1, tc: 1 }, { unique: true } )

db.ping.createIndex( { source: 1, destination: 1, timestamp: 1 } )


// ###### Traceroute ########################################################
use ${DATABASE}

db.createCollection("traceroute", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.traceroute.createIndex( { timestamp: 1, source: 1, destination: 1, tc: 1, round: 1, hopNumber: 1 }, { unique: true } )

db.traceroute.createIndex( { source: 1, destination: 1, timestamp: 1 } )


quit()
