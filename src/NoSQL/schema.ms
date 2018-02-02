use pingtraceroutedb
db.createCollection("ping", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
db.createCollection("traceroute", { storageEngine: { wiredTiger: { configString: 'block_compressor=zlib' }}})
show collections
