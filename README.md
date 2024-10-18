High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute service. It performs regular Ping and Traceroute runs among sites and can import the results into an SQL or NoSQL database. HiPerConTracer currently offers runs over ICMP and UDP. Currently, database backends for MariaDB/MySQL, PostgreSQL and MongoDB are provided. Furthermore, it provides tools to query databases (export reesults files), as well as to process results files (either written by HiPerConTracer directly, or exported from a database).


# Build HiPerConTracer

cmake .

make

sudo make install


# Run HiPerConTracer

Examples:
- sudo ./hipercontracer -#1000 --source 0.0.0.0 --destination 193.99.144.80 --traceroute --ping --resultsdirectory ./data --verbose
- sudo ./hipercontracer -#1000 --source 0.0.0.0 --source :: --destination 193.99.144.80 --destination 2a02:2e0:3fe:1001:302:: --traceroute --ping --resultsdirectory ./data --verbose

Note: HiPerConTracer expects the directory "./data" (provided with "--resultsdirectory") to write the results files to!

See the the manpage "hipercontracer" for all options, including a description of the results file formats!


# Convert Results into a CSV File

- find ./data -maxdepth 1 -name "Ping*.hpct.*" | ./hpct-results --input-file-names-from-stdin --separator=, -o ping.csv.gz
- find ./data -maxdepth 1 -name "Traceroute*.hpct.*" | ./hpct-results --input-file-names-from-stdin --separator=, -o traceroute.csv.gz

Hint: You can use extension .gz for GZip, .bz for BZip2, .xz for XZ, or none for uncompressed output into the output CSV file!

See the the manpage "hpct-results" for all options!

Note: For simple measurements, you can use HiPerConTracer and the Results Tool directly, without using a database!


# Setting Up a Database

See src/SQL and src/NoSQL for schema, user and permission setups. Use the database of your choice (MariaDB/MySQL, PostgreSQL, MongoDB).

Hint: All HiPerConTracer tools support TLS setup. It is **strongly** recommended to properly setup TLS for secure access to a database!


# Using the Importer to Store Results in a Database

## Write a Configuration File for the Importer

See [src/hipercontracer-database.conf](src/hipercontracer-database.conf) for an example. Make sure that the database access details are correct, so that the Importer tool can connect to the right database and has the required permissions! See src/SQL and src/NoSQL for schema, user and permission setups.

Note: Make sure the "data" directory, as well as the directory for "good" imports and the directory for "bad" (i.e. failed) imports are existing and accessible by the user running the importer!

## Run the Importer

Examples:
- ./hpct-importer ~/testdb-users-mariadb-importer.conf -verbose
- ./hpct-importer ~/testdb-users-mariadb-importer.conf -verbose --quit-when-idle

Note: If running without "--quit-when-idle" (recommended), the importer keeps running and imports new files as soon as they appear in the results directory. The importer uses INotify!

See the the manpage "hpct-importer" for all options!


# Using the Query Tool to Retrieve Results from a Database

## Write a Configuration File for the Query Tool

See [src/hipercontracer-database.conf](src/hipercontracer-database.conf) for an example. Make sure that the database access details are correct, so that the Query tool can connect to the right database and has the required permissions! See src/SQL and src/NoSQL for schema, user and permission setups.

## Run the Query Tool

Examples:
- ./hpct-query ~/testdb-users-mariadb-researcher.conf ping -o ping.hpct.gz
- ./hpct-query ~/testdb-users-mariadb-researcher.conf ping -o ping.hpct.gz --from-measurement-id 1000 --to-measurement-id 1000
- ./hpct-query ~/testdb-users-mariadb-researcher.conf traceroute -o traceroute.hpct.gz --loglevel 0 --from-measurement-id 1000 --to-measurement-id 1000
- ./hpct-query ~/testdb-users-mariadb-researcher.conf traceroute -o traceroute.hpct.gz --verbose --from-time "2023-09-22 00:00:00"
- ./hpct-query ~/testdb-users-mariadb-researcher.conf traceroute -o traceroute.hpct.gz --verbose --from-time "2023-09-22 00:00:00" --to-time "2023-09-23 00:00:00"

The output is in the same format as the originally written HiPerConTracer results. See the the manpage "hipercontracer" for all options, including a description of the results file formats!

Note: Make sure to specify a Measurement ID range, or a time range. Otherwise, the Query tool will export **everything**!

Hint: You can use extension .gz for GZip, .bz for BZip2, .xz for XZ, or none for uncompressed output into the output CSV file!

See the the manpage "hpct-query" for all options!

To convert the results to CSV format, use for example:
- ./hpct-results ping.hpct.gz --separator=, --output ping.csv.gz
- ./hpct-results traceroute.hpct.gz --separator=, --output traceroute.csv.gz


# Examples to Process Results from CSV

See [src/examples](src/examples) for some examples.

## GNU R

See [src/examples/install-packages.R](src/results-examples/install-packages.R) to get the necessary library packages installed!

- ./r-ping-example ../ping.csv
- ./r-traceroute-example ../traceroute.csv.gz

## LibreOffice (or any similar spreadsheet program)

Import CSV file into spreadsheet.

Hints:
- Use English (US) language, to avoid strange number conversions.
- Choose column separator (" ", ",", etc.), if not automatically detected.
