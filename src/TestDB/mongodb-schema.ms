// ###### Ping ##############################################################
use pingtraceroutedb
db.createCollection("ping",       { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.ping.createIndex( { timestamp: 1 })

// ###### Traceroute ########################################################
use pingtraceroutedb
db.createCollection("traceroute", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.traceroute.createIndex( { timestamp: 1 })
