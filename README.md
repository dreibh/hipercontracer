High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute service. It performs regular Ping and Traceroute runs among sites and can import the results into an SQL or NoSQL database. HiPerConTracer currently offers runs over ICMP and UDP. Currently, database backends for MariaDB/MySQL, PostgreSQL and MongoDB are provided. Furthermore, it provides tools to query databases (export reesults files), as well as to process results files (either written by HiPerConTracer directly, or exported from a database).


# Build HiPerConTracer

cmake .

make

sudo make install


# Run HiPerConTracer

Examples:
- sudo hipercontracer --source 0.0.0.0 --destination 193.99.144.80 --traceroute --ping --resultsdirectory /tmp/results --verbose
- sudo hipercontracer --source 0.0.0.0 --source :: --destination 193.99.144.80 --destination 2a02:2e0:3fe:1001:302:: --traceroute --ping --resultsdirectory /tmp/results --verbose

See the the manpage "hipercontracer" for all options, including a description of the results file formats!


# Convert results into a CSV file

- find /tmp/results -name "Ping*.results.*" | ./hpct-results-tool --input-file-names-from-stdin -o ping.csv
- find /tmp/results -name "Traceroute*.results.*" | ./hpct-results-tool --input-file-names-from-stdin -o traceroute.csv

Hint: You can use extension .gz for GZip, .bz for BZip2, .xz for XZ, or none for uncompressed output into the output CSV file!

See the the manpage "hpct-results-tool" for all options!


# Setting Up a Database

See src/SQL and src/NoSQL for schema and user setups. Use the database of your choice (MariaDB/MySQL, PostgreSQL, MongoDB).

Hint: All HiPerConTracer tools support TLS setup. It is **strongly** recommended to properly setup TLS for secure access to a database!


# Run the Importer
