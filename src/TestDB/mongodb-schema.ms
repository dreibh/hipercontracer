use pingtraceroutedb

// ====== Create collections ================================================
db.createCollection("ping", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.createCollection("traceroute", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
show collections

// ====== Create indices ====================================================
db.ping.createIndex( { timestamp: 1 })
db.traceroute.createIndex( { timestamp: 1 })
