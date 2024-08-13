use pingtraceroutedb

var cursor = db.traceroute.aggregate([{ $group: { _id: { timestamp:   "$timestamp",
                                                         source:      "$source",
                                                         destination: "$destination",
                                                         tc:          "$tc",
                                                         round:       "$round"
                                                       },
                                         uniqueIds: { $addToSet: "$_id" },
                                         count: { $sum: 1 } } }, { $match: { count: { $gt: 1 } } }]);

while (cursor.hasNext()) {
   var document = cursor.next();
   var index = 1;
   while (index < document.uniqueIds.length) {
      db.traceroute.deleteOne( { _id: document.uniqueIds[index] });
      index = index + 1;
   }
   print(document)
}

db.traceroute.createIndex({
   timestamp:   1,
   source:      1,
   destination: 1,
   tc:          1,
   round:       1
}, { unique: true })
