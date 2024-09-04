use pingtraceroutedb

var cursor = db.ping.aggregate([{ $group: { _id: { timestamp:   "$timestamp",
                                                   source:      "$source",
                                                   destination: "$destination",
                                                   tc:          "$tc"
                                                 },
                                  uniqueIds: { $addToSet: "$_id" },
                                  count: { $sum: 1 } } }, { $match: { count: { $gt: 1 } } }]);

while (cursor.hasNext()) {
   var document = cursor.next();
   var index = 1;
   while (index < document.uniqueIds.length) {
      db.ping.deleteOne( { _id: document.uniqueIds[index] });
      index = index + 1;
   }
   print(document)
}

db.ping.createIndex({
   timestamp:   1,
   source:      1,
   destination: 1,
   tc:          1
}, { unique: true })
