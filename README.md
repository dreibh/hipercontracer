High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute measurement framework. It performs regular Ping and Traceroute runs among sites and can import the results into an SQL or NoSQL database. HiPerConTracer currently offers runs over ICMP and UDP.
The HiPerConTracer framework furthermore provides additional tools for helping to obtain, process, collect, store, and retrieve measurement data:

* [HiPerConTracer](#run-a-hipercontracer-measurement) is the measurement tool itself;
* [HiPerConTracer Viewer Tool](#the-hipercontracer-viewer-tool) for displaying the contents of results files;
* [HiPerConTracer Results Tool](#the-hipercontracer-results-tool) for merging and converting results files, e.g.&nbsp;to create a Comma-Separated Value&nbsp;(CSV) file;
* [HiPerConTracer Sync Tool](#the-hipercontracer-sync-tool) for copying data to a remote collector (via [RSync](https://rsync.samba.org/)/[SSH](https://www.openssh.com/));
* [HiPerConTracer Reverse Tunnel Tool](#the-hipercontracer-reverse-tunnel-tool) for maintaining a reverse [SSH](https://www.openssh.com/) from a remote measurement node to a collector server;
* [HiPerConTracer Trigger Tool]() for triggering HiPerConTracer measurements in the reverse direction;
* [HiPerConTracer Importer Tool](#the-hipercontracer-importer-tool) for storing measurement data from results files into SQL or NoSQL databases. Currently, database backends for [MariaDB](https://mariadb.com/)/[MySQL](https://www.mysql.com/), [PostgreSQL](https://www.postgresql.org/) and [MongoDB](https://www.mongodb.com/) are provided;
* [HiPerConTracer Query Tool](#the-hipercontracer-query-tool) for querying data from a database and storing it into a data file;
* [HiPerConTracer Database Shell]() as simple command-line front-end for the underlying database backends;
* [HiPerConTracer Database Tools]() with some helper programs to e.g.&nbsp;join HiPerConTracer database configurations into an existing DBeaver (SQL database GUI) configuration;
* [HiPerConTracer UDP Echo Server]() as UDP Echo ([RFC&nbsp;862](https://datatracker.ietf.org/doc/html/rfc862)) protocol endpoint.

# Build the HiPerConTracer Framework from Sources

Basic procedure:
```
cmake .
make
sudo make install
```

Notes:

- There are various CMake options to enable/disable database backends and tools. A GUI tool like CCMake provides a comfortable way of configuration.
- The CMake run will show missing dependencies, and provide help for installing them on Debian, Ubuntu, Fedora and FreeBSD.


# Run a HiPerConTracer Measurement

HiPerConTracer is the measurement tool itself.

## Example 1
A simply Ping run, without data storage, from arbitrary local addresses, to all IPv4 and IPv6 addresses of [www.heise.de](https://www.heise.de) via ICMP (default):
```
sudo hipercontracer --destination www.heise.de --ping --verbose
```

## Example 2
Run HiPerConTracer measurement #1000000, from arbitrary local IPv4 address to destination 193.99.144.80 with Traceroute and Ping. Store data into sub-directory "data" in the current directory; run as current user $USER:
```
sudo hipercontracer --user $USER -#1000000 --source 0.0.0.0 --destination 193.99.144.80 --traceroute --ping --resultsdirectory data --verbose
```
Notes:
- HiPerConTracer expects the sub-directory "data" (provided with "--resultsdirectory") to write the results files to. The directory must be writable for the user $USER!
- See the manpage "hipercontracer" for various options to set Ping and Traceroute intervals, results file lengths, results file compression, etc.!

## Example 3
Run HiPerConTracer measurement #1000001, from arbitrary local IPv4 (0.0.0.0) and IPv6 (::) addresses to destinations 193.99.144.80 and 2a02:2e0:3fe:1001:302:: with Traceroute and Ping via ICMP (default). Store data into sub-directory "data" in the current directory; run as current user $USER:
```
sudo hipercontracer --user $USER -#1000001 --source 0.0.0.0 --source :: --destination 193.99.144.80 --destination 2a02:2e0:3fe:1001:302:: --traceroute --ping --resultsdirectory data --verbose
```

## Data Examples

Some simple data file examples (from [src/results-examples](https://github.com/dreibh/hipercontracer/tree/master/src/results-examples)):

- [HiPerConTracer ICMP Ping over IPv4](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples/Ping-ICMP-%231000001-0.0.0.0-20241219T090830.364329-000000001.hpct)
- [HiPerConTracer ICMP Ping over IPv6](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples/Ping-ICMP-%231000001-%3A%3A-20241219T090830.364464-000000001.hpct)
- [HiPerConTracer ICMP Traceroute over IPv4](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples/Traceroute-ICMP-%231000001-0.0.0.0-20241219T090830.364422-000000001.hpct)
- [HiPerConTracer ICMP Traceroute over IPv6](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples/Traceroute-ICMP-%231000001-%3A%3A-20241219T090830.364515-000000001.hpct)

Notes:

- See the the [manpage of "hipercontracer"](https://github.com/dreibh/hipercontracer/blob/master/src/hipercontracer.1) for a detailed description of the results file formats: ```man hipercontracer```
- The HiPerConTracer Viewer Tool can be used to display results files, including uncompressed ones.
- The HiPerConTracer Results Tool can be used to merge and/or convert the results files.

## Further Details
See the the manpage of "hipercontracer" for all options, including a description of the results file formats!


# The HiPerConTracer Viewer Tool

The HiPerConTracer Viewer Tool displays the contents of results files.

## Example
```
hpct-viewer src/results-examples/Traceroute-UDP-#88888888-fdb6:6d27:be73:4::50-20231018T102656.821891-000000001.results.xz
```

## Further Details
See the the [manpage of "hpct-viewer"](https://github.com/dreibh/hipercontracer/blob/master/src/hpct-viewer.1) for a detailed description of the available options: ```man hpct-viewer```


# The HiPerConTracer Results Tool

The HiPerConTracer Results Tool provides merging and converting results files, e.g.&nbsp;to create a Comma-Separated Value&nbsp;(CSV) file.

## Example 1
Merge all files matching the pattern "Ping\*.hpct.\*" into CSV file, with "," as separator:
```
find data -maxdepth 1 -name "Ping*.hpct.*" | hpct-results --input-file-names-from-stdin --separator=, -o ping.csv.gz
```
Hint: You can use extension .gz for GZip, .bz for BZip2, .xz for XZ, or none for uncompressed output into the output CSV file!

## Example 2
Merge all files matching the pattern "Traceroute\*.hpct.\*" into CSV file, with ";" as separator:
```
find data -maxdepth 1 -name "Traceroute*.hpct.*" | hpct-results --input-file-names-from-stdin --separator=; -o traceroute.csv.gz
```

## Processing Results from a CSV File

See [src/results-examples](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples) for some examples.

### GNU R

See [src/results-examples/r-install-dependencies](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples/r-install-dependencies) to get the necessary library packages installed!

- [r-ping-example](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples/r-ping-example) ping.csv
- [r-traceroute-example](https://github.com/dreibh/hipercontracer/blob/master/src/results-examples/r-traceroute-example) traceroute.csv.gz

### LibreOffice (or any similar spreadsheet program)

Import CSV file into spreadsheet.

Hints:

- Use English (US) language, to avoid strange number conversions.
- Choose column separator (" ", ",", etc.), if not automatically detected.

## Further Details
See the the [manpage of "hpct-results"](https://github.com/dreibh/hipercontracer/blob/master/src/hpct-results.1) for a detailed description of the available options: ```man hpct-results```


# Setting Up a Database

See [src/SQL](https://github.com/dreibh/hipercontracer/tree/master/src/SQL) and [src/NoSQL](https://github.com/dreibh/hipercontracer/tree/master/src/NoSQL) for schema, user and permission setups. Use the database of your choice (MariaDB/MySQL, PostgreSQL, MongoDB).

Hint: All HiPerConTracer tools support TLS setup. It is **strongly** recommended to properly setup TLS for secure access to a database! See [src/TestDB](https://github.com/dreibh/hipercontracer/tree/master/src/TestDB) as example; this is the CI test, which includes a full TLS setup with all supported database backends.


# The HiPerConTracer Importer Tool

The HiPerConTracer Importer Tool provides the continuous storage of collected measurement data from results files into SQL or NoSQL databases. Currently, database backends for [MariaDB](https://mariadb.com/)/[MySQL](https://www.mysql.com/), [PostgreSQL](https://www.postgresql.org/) and [MongoDB](https://www.mongodb.com/) are provided.

## Write a Configuration File for the Importer

See [src/hipercontracer-database.conf](src/hipercontracer-database.conf) for an example. Make sure that the database access details are correct, so that the Query tool can connect to the right database and has the required permissions! See [src/SQL](https://github.com/dreibh/hipercontracer/tree/master/src/SQL) and [src/NoSQL](https://github.com/dreibh/hipercontracer/tree/master/src/NoSQL) for schema, user and permission setups.

Note: Make sure the "data" directory, as well as the directory for "good" imports and the directory for "bad" (i.e. failed) imports are existing and accessible by the user running the importer!

## Run the Importer Tool

### Example 1
Continuously run, waiting for new files to import:
```
hpct-importer ~/testdb-users-mariadb-importer.conf -verbose
```

### Example 2
Just run one import round, quit when there are no more files to import:
```
hpct-importer ~/testdb-users-mariadb-importer.conf -verbose --quit-when-idle
```

Note: If running without "--quit-when-idle" (recommended), the importer keeps running and imports new files as soon as they appear in the results directory. The importer uses INotify!

## Further Details
See the the [manpage of "hpct-importer"](https://github.com/dreibh/hipercontracer/blob/master/src/hpct-importer.1) for a detailed description of the available options: ```man hpct-importer```


# The HiPerConTracer Query Tool

## Write a Configuration File for the Query Tool

See [src/hipercontracer-database.conf](src/hipercontracer-database.conf) for an example. Make sure that the database access details are correct, so that the Query tool can connect to the right database and has the required permissions! See [src/SQL](https://github.com/dreibh/hipercontracer/tree/master/src/SQL) and [src/NoSQL](https://github.com/dreibh/hipercontracer/tree/master/src/NoSQL) for schema, user and permission setups.

## Run the Query Tool

### Example 1
Export all Ping data to ping.hpct.gz (GZip-compressed data file):
```
hpct-query ~/testdb-users-mariadb-researcher.conf ping -o ping.hpct.gz
```

Notes:

- Make sure to specify a Measurement ID range, or a time range. Otherwise, the Query tool will export **everything**!
- The output is in the same format as the originally written HiPerConTracer results. See the the manpage "hipercontracer" for all options, including a description of the results file formats!
- You can use extension .gz for GZip, .bz for BZip2, .xz for XZ, or none for uncompressed output!

### Example 2
Export all Ping data of Measurement ID #1000 to ping.hpct.gz (GZip-compressed data file):
```
hpct-query ~/testdb-users-mariadb-researcher.conf ping -o ping.hpct.gz --from-measurement-id 1000 --to-measurement-id 1000
```

### Example 3
Export all Traceroute data of Measurement ID #1000 to traceroute.hpct.bz2 (BZip2-compressed data file), with verbose logging:
```
hpct-query ~/testdb-users-mariadb-researcher.conf traceroute -o traceroute.hpct.bz2 --loglevel 0 --from-measurement-id 1000 --to-measurement-id 1000
```

### Example 4
Export all Traceroute data from 2023-09-22 00:00:00 to traceroute.hpct.xz (XZ-compressed data file), with verbose logging:
```
hpct-query ~/testdb-users-mariadb-researcher.conf traceroute -o traceroute.hpct.xz --verbose --from-time "2023-09-22 00:00:00"
```

### Example 5
Export all Traceroute data between 2023-09-22 00:00:00 and 2023-09-23 00:00:00 to traceroute.hpct.xz (XZ-compressed data file):
```
hpct-query ~/testdb-users-mariadb-researcher.conf traceroute -o traceroute.hpct.xz --verbose --from-time "2023-09-22 00:00:00" --to-time "2023-09-23 00:00:00"
```

## Further Details
See the the [manpage of "hpct-query"](https://github.com/dreibh/hipercontracer/blob/master/src/hpct-query.1) for a detailed description of the available options: ```man hpct-query```


# The HiPerConTracer Sync Tool

The HiPerConTracer Sync Tool helps synchronising collected results files to a collection server (denoted as Collector), using [RSync](https://rsync.samba.org/)/[SSH](https://www.openssh.com/)).

## Example
Synchronise results files, with the following settings:

- local node is Node 1000;
- run as user "hipercontracer";
- from local directory /var/hipercontracer;
- to remote server's directory /var/hipercontracer;
- remote server is sognsvann.example (must be a resolvable hostname);
- private key for logging into the remote server via SSH is in /var/hipercontracer/ssh/id_ed25519;
- known_hosts file for SSH is /var/hipercontracer/ssh/known_hosts;
- run with verbose output
```
sudo -u  hipercontracer  hpct-sync \
   --nodeid 1000 --collector sognsvann.example \
   --local /var/hipercontracer --remote /var/hipercontracer \
   --key /var/hipercontracer/ssh/id_ed25519 \
   --known-hosts /var/hipercontracer/ssh/known_hosts --verbose
```

## Further Details
See the the [manpage of "hpct-sync"](https://github.com/dreibh/hipercontracer/blob/master/src/hpct-sync.1) for a detailed description of the available options: ```man hpct-sync```


# The HiPerConTracer Reverse Tunnel Tool

The HiPerConTracer Reverse Tunnel (RTunnel) Tool maintains a reverse [SSH](https://www.openssh.com/) from a remote measurement node to a collector server. The purpose is to allow for SSH login from the collector server to the measurement node, via the reverse tunnel. Then, the measurement node does not need a publicly-reachable IP address (e.g.&nbsp;the node only has a private IP address behind a firewall).

## Example
- local node is Node 1000;
- run as user "hipercontracer";
- connect to Collector server 10.44.35.16, using SSH private key from /var/hipercontracer/ssh/id_ed25519, with SSH known_hosts file /var/hipercontracer/ssh/known_hosts
```
sudo -u hipercontracer hpct-rtunnel \
   --nodeid 1000 --collector 10.44.35.16 \
   --key /var/hipercontracer/ssh/id_ed25519 \
   --known-hosts /var/hipercontracer/ssh/known_hosts
```

## Further Details
See the the [manpage of "hpct-rtunnel"](https://github.com/dreibh/hipercontracer/blob/master/src/hpct-rtunnel.1) for a detailed description of the available options: ```man hpct-rtunnel```




* [HiPerConTracer Trigger Tool]() for triggering HiPerConTracer measurements in the reverse direction;
