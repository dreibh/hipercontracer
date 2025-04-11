Name: hipercontracer
Version: 2.0.11
Release: 1
Summary: High-Performance Connectivity Tracer (HiPerConTracer)
Group: Applications/Internet
License: GPL-3.0-or-later
URL: https://www.nntb.no/~dreibh/hipercontracer/
Source: https://www.nntb.no/~dreibh/hipercontracer/download/%{name}-%{version}.tar.xz
Packager: Thomas Dreibholz <dreibh@simula.no>

AutoReqProv: on
BuildRequires: boost-devel
BuildRequires: bzip2-devel
BuildRequires: cmake
BuildRequires: ghostscript
BuildRequires: GraphicsMagick
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: libbson-devel
BuildRequires: libpqxx-devel
BuildRequires: mariadb-connector-c-devel
BuildRequires: mongo-c-driver-devel
BuildRequires: openssl-devel
BuildRequires: pdf2svg
BuildRequires: xz-devel
BuildRequires: zlib-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-build
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-libhipercontracer = %{version}-%{release}
Requires: acl
Requires: iproute
Recommends: %{name}-viewer = %{version}-%{release}
Recommends: ethtool
Suggests: %{name}-collector = %{version}-%{release}
Suggests: %{name}-dbeaver-tools = %{version}-%{release}
Suggests: %{name}-dbshell = %{version}-%{release}
Suggests: %{name}-importer = %{version}-%{release}
Suggests: %{name}-node = %{version}-%{release}
Suggests: %{name}-query = %{version}-%{release}
Suggests: %{name}-results = %{version}-%{release}
Suggests: %{name}-sync = %{version}-%{release}
Suggests: %{name}-trigger = %{version}-%{release}
Suggests: %{name}-udp-echo-server = %{version}-%{release}
Suggests: netperfmeter
Suggests: td-system-info


%description
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains the actual HiPerConTracer program.

%prep
%setup -q

%build
# NOTE: CMAKE_VERBOSE_MAKEFILE=OFF for reduced log output!
%cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_VERBOSE_MAKEFILE=OFF -DWITH_STATIC_LIBRARIES=ON -DWITH_SHARED_LIBRARIES=ON .
%cmake_build

%install
%cmake_install

%files
%{_bindir}/get-default-ips
%{_bindir}/hipercontracer
%{_datadir}/bash-completion/completions/hipercontracer
%{_mandir}/man1/get-default-ips.1.gz
%{_mandir}/man1/hipercontracer.1.gz
%{_sysconfdir}/hipercontracer/hipercontracer-12345678.conf
%{_prefix}/lib/systemd/system/hipercontracer.service
%{_prefix}/lib/systemd/system/hipercontracer@.service


%package common
Summary: HiPerConTracer common files
Group: Applications/File
BuildArch: noarch
Recommends: %{name} = %{version}-%{release}
Recommends: %{name}-results = %{version}-%{release}
Suggests: R-base
Suggests: python3

%description common
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
The package contains common files for HiPerConTracer and the HiPerConTracer
Tools packages.

%pre common
# Make sure the administrative user exists
if ! getent group hipercontracer >/dev/null 2>&1; then
   groupadd -r hipercontracer
fi
if ! getent passwd hipercontracer >/dev/null 2>&1; then
   useradd -M -g hipercontracer -r -d /var/hipercontracer -s /sbin/nologin -c "HiPerConTracer User" hipercontracer
fi
if ! getent group hpct-nodes >/dev/null 2>&1; then
   groupadd -r hpct-nodes
fi
usermod -a -G hpct-nodes hipercontracer

# Set up HiPerConTracer directories:
mkdir -p -m 755 /var/hipercontracer
chown hipercontracer:hipercontracer /var/hipercontracer || true

for subDirectory in data good bad ; do
   mkdir -p -m 755 /var/hipercontracer/$subDirectory
   chown hipercontracer:hpct-nodes /var/hipercontracer/$subDirectory || true
   setfacl -Rm d:u:hipercontracer:rwx,u:hipercontracer:rwx /var/hipercontracer/$subDirectory || true
done

mkdir -p -m 700 /var/hipercontracer/ssh
chown hipercontracer:hipercontracer /var/hipercontracer/ssh || true

%postun common
# Remove administrative user:
userdel  hipercontracer >/dev/null 2>&1 || true
groupdel hipercontracer >/dev/null 2>&1 || true
groupdel hpct-nodes     >/dev/null 2>&1 || true

# Remove data directory (if empty):
for subDirectory in data good bad ssh ; do
   rmdir /var/hipercontracer/$subDirectory || true
done
rmdir /var/hipercontracer >/dev/null 2>&1 || true

%files common
%{_datadir}/hipercontracer/hipercontracer.bib
%{_datadir}/hipercontracer/hipercontracer.pdf
%{_datadir}/hipercontracer/hipercontracer.png
%{_datadir}/hipercontracer/results-examples/HiPerConTracer.R
%{_datadir}/hipercontracer/results-examples/*-*.results.*
%{_datadir}/hipercontracer/results-examples/*-*.hpct
%{_datadir}/hipercontracer/results-examples/*-*.hpct.*
%{_datadir}/hipercontracer/results-examples/README.md
%{_datadir}/hipercontracer/results-examples/r-install-dependencies
%{_datadir}/hipercontracer/results-examples/r-ping-example
%{_datadir}/hipercontracer/results-examples/r-traceroute-example
%{_datadir}/icons/hicolor/*x*/apps/hipercontracer.png
%{_datadir}/icons/hicolor/scalable/apps/hipercontracer.svg
%{_datadir}/mime/packages/hipercontracer.xml


%package libhipercontracer
Summary: API library of HiPerConTracer
Group: System Environment/Libraries
Requires: %{name}-libhipercontracer = %{version}-%{release}

%description libhipercontracer
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
The HiPerConTracer library is provided by this package.

%files libhipercontracer
%{_libdir}/libhipercontracer.so.*


%package libhipercontracer-devel
Summary: Development files for HiPerConTracer API library
Group: Development/Libraries
Requires: %{name}-libhipercontracer = %{version}-%{release}
Requires: boost-devel

%description libhipercontracer-devel
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package provides header files for the HiPerConTracer library. You need
them to integrate HiPerConTracer into own programs.

%files libhipercontracer-devel
%{_includedir}/hipercontracer/check.h
%{_includedir}/hipercontracer/destinationinfo.h
%{_includedir}/hipercontracer/iomodule-base.h
%{_includedir}/hipercontracer/iomodule-icmp.h
%{_includedir}/hipercontracer/iomodule-udp.h
# %{_includedir}/hipercontracer/jitter.h
%{_includedir}/hipercontracer/logger.h
%{_includedir}/hipercontracer/ping.h
%{_includedir}/hipercontracer/resultentry.h
%{_includedir}/hipercontracer/resultswriter.h
%{_includedir}/hipercontracer/service.h
%{_includedir}/hipercontracer/tools.h
%{_includedir}/hipercontracer/traceroute.h
%{_libdir}/libhipercontracer*.so
%{_libdir}/libhipercontracer.a


%package libuniversalimporter
Summary: API library of HiPerConTracer Universal Importer
Group: System Environment/Libraries
Requires: %{name}-libuniversalimporter = %{version}-%{release}

%description libuniversalimporter
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
The HiPerConTracer Universal Importer library is provided by this
package.

%files libuniversalimporter
%{_libdir}/libuniversalimporter.so.*


%package libuniversalimporter-devel
Summary: Development files for HiPerConTracer Universal Importer API library
Group: Development/Libraries
Requires: %{name}-libuniversalimporter = %{version}-%{release}
Requires: boost-devel

%description libuniversalimporter-devel
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package provides header files for the HiPerConTracer Universal Importer
library. You need them to integrate HiPerConTracer Universal Importer into
own programs.

%files libuniversalimporter-devel
%{_includedir}/universalimporter/database-configuration.h
%{_includedir}/universalimporter/database-statement.h
%{_includedir}/universalimporter/databaseclient-base.h
%{_includedir}/universalimporter/databaseclient-debug.h
%{_includedir}/universalimporter/databaseclient-mariadb.h
%{_includedir}/universalimporter/databaseclient-mongodb.h
%{_includedir}/universalimporter/databaseclient-postgresql.h
%{_includedir}/universalimporter/importer-configuration.h
%{_includedir}/universalimporter/logger.h
%{_includedir}/universalimporter/reader-base.h
%{_includedir}/universalimporter/results-exception.h
%{_includedir}/universalimporter/tools.h
%{_includedir}/universalimporter/universal-importer.h
%{_includedir}/universalimporter/worker.h
%{_libdir}/libuniversalimporter*.so
%{_libdir}/libuniversalimporter.a


%package trigger
Summary: Triggered HiPerConTracer service
Group: Applications/Internet
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-libhipercontracer = %{version}-%{release}
Recommends: %{name} = %{version}-%{release}

%description trigger
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This tool triggers HiPerConTracer measurements by incoming "Ping" packets.

%files trigger
%{_bindir}/hpct-trigger
%{_datadir}/bash-completion/completions/hpct-trigger
%{_mandir}/man1/hpct-trigger.1.gz
%{_sysconfdir}/hipercontracer/hpct-trigger-87654321.conf
%{_prefix}/lib/systemd/system/hpct-trigger.service
%{_prefix}/lib/systemd/system/hpct-trigger@.service


%package sync
Summary: HiPerConTracer Sync Tool to synchronise results files to a server
Group: Applications/File
BuildArch: noarch
Requires: %{name}-common = %{version}-%{release}
Requires: openssh-clients
Requires: rsync
Recommends: %{name} = %{version}-%{release}
Recommends: %{name}-results = %{version}-%{release}

%description sync
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains the HiPerConTracer Sync Tool for running RSync
synchronisation of data to a central HiPerConTracer Collector server.

%files sync
%{_bindir}/hpct-sync
%{_mandir}/man1/hpct-sync.1.gz
%{_datadir}/bash-completion/completions/hpct-sync
%{_sysconfdir}/hipercontracer/hpct-sync.conf
%{_prefix}/lib/systemd/system/hpct-sync.service
%{_prefix}/lib/systemd/system/hpct-sync.timer


%package rtunnel
Summary: HiPerConTracer Reverse Tunnel Tool for reverse SSH tunnel setup
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-sync = %{version}-%{release}
Requires: openssh-server
BuildArch: noarch

%description rtunnel
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This is the HiPerConTracer Reverse Tunnel Tool. It maintains an SSH
reverse tunnel from a measurement node to a HiPerConTracer Collector server.

%files rtunnel
%{_bindir}/hpct-rtunnel
%{_datadir}/bash-completion/completions/hpct-rtunnel
%{_mandir}/man1/hpct-rtunnel.1.gz
%{_prefix}/lib/systemd/system/hpct-rtunnel.service


%package node
Summary: HiPerConTracer Node Tools for maintaining a measurement node
Requires: %{name} = %{version}-%{release}
Requires: %{name}-rtunnel = %{version}-%{release}
Requires: %{name}-sync = %{version}-%{release}
Requires: sudo
Recommends: td-system-tools-system-info
Recommends: td-system-tools-system-maintenance
BuildArch: noarch

%description node
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This is the HiPerConTracer Node package. It contains helper scripts to attach a
HiPerConTracer Node to a HiPerConTracer Collector server.

%files node
%{_bindir}/hpct-node-setup
%{_datadir}/bash-completion/completions/hpct-node-setup
%{_mandir}/man1/hpct-node-setup.1.gz
# %{_sysconfdir}/system-info.d/30-hpct-node
# %{_sysconfdir}/system-maintenance.d/30-hpct-node


%package collector
Summary: HiPerConTracer Collector Tools for collecting measurement results
Requires: %{name}-common = %{version}-%{release}
Requires: openssh-clients
Requires: iproute
Requires: openssh-server
Requires: rsync
Requires: sudo

%description collector
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This is the HiPerConTracer Collector Tools package. It contains helper scripts
to set up and maintain a HiPerConTracer Collector server.

%files collector
%{_bindir}/hpct-node-removal
%{_bindir}/hpct-nodes-list
%{_bindir}/hpct-ssh
%{_datadir}/bash-completion/completions/hpct-node-removal
%{_datadir}/bash-completion/completions/hpct-ssh
%{_mandir}/man1/hpct-node-removal.1.gz
%{_mandir}/man1/hpct-nodes-list.1.gz
%{_mandir}/man1/hpct-ssh.1.gz
# %{_sysconfdir}/system-info.d/35-hpct-collector
# %{_sysconfdir}/system-maintenance.d/35-hpct-collector


%package importer
Summary: HiPerConTracer Importer for importing results into a database
Group: Applications/Database
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-libuniversalimporter = %{version}-%{release}
Recommends: %{name} = %{version}-%{release}
Recommends: %{name}-dbshell = %{version}-%{release}
Suggests: python3

%description importer
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains the HiPerConTracer Importer Tool to import data from
HiPerConTracer results files into an SQL or NoSQL database.

%files importer
%{_bindir}/hpct-importer
%{_datadir}/bash-completion/completions/hpct-importer
%{_mandir}/man1/hpct-importer.1.gz
%{_datadir}/hipercontracer/NoSQL/R-query-example.R
%{_datadir}/hipercontracer/NoSQL/README-MongoDB.md
%{_datadir}/hipercontracer/NoSQL/mongodb-database.ms
%{_datadir}/hipercontracer/NoSQL/mongodb-schema.ms
%{_datadir}/hipercontracer/NoSQL/mongodb-test.ms
%{_datadir}/hipercontracer/NoSQL/mongodb-users.ms
%{_datadir}/hipercontracer/NoSQL/nornet-tools.R
%{_datadir}/hipercontracer/SQL/README-MySQL+MariaDB.md
%{_datadir}/hipercontracer/SQL/README-PostgreSQL.md
%{_datadir}/hipercontracer/SQL/mariadb-database.sql
%{_datadir}/hipercontracer/SQL/mariadb-delete-all-rows.sql
%{_datadir}/hipercontracer/SQL/mariadb-schema.sql
%{_datadir}/hipercontracer/SQL/mariadb-test.sql
%{_datadir}/hipercontracer/SQL/mariadb-users.sql
%{_datadir}/hipercontracer/SQL/postgresql-database.sql
%{_datadir}/hipercontracer/SQL/postgresql-delete-all-rows.sql
%{_datadir}/hipercontracer/SQL/postgresql-schema.sql
%{_datadir}/hipercontracer/SQL/postgresql-test.sql
%{_datadir}/hipercontracer/SQL/postgresql-users.sql
%{_datadir}/hipercontracer/TestDB/0-make-configurations
%{_datadir}/hipercontracer/TestDB/1-install-database
%{_datadir}/hipercontracer/TestDB/2-initialise-database
%{_datadir}/hipercontracer/TestDB/3-test-database
%{_datadir}/hipercontracer/TestDB/4-clean-database
%{_datadir}/hipercontracer/TestDB/5-perform-hpct-importer-test
%{_datadir}/hipercontracer/TestDB/6-perform-hpct-query-test
%{_datadir}/hipercontracer/TestDB/9-uninstall-database
%{_datadir}/hipercontracer/TestDB/CertificateHelper.py
%{_datadir}/hipercontracer/TestDB/README.md
%{_datadir}/hipercontracer/TestDB/generate-test-certificates
%{_datadir}/hipercontracer/TestDB/hpct-users.conf.example
%{_datadir}/hipercontracer/TestDB/name-in-etc-hosts
%{_datadir}/hipercontracer/TestDB/run-full-test
%{_datadir}/hipercontracer/TestDB/test-tls-connection
%{_datadir}/hipercontracer/hipercontracer-database.conf
%{_datadir}/hipercontracer/hipercontracer-importer.conf
%{_sysconfdir}/hipercontracer/hpct-importer.conf
%{_prefix}/lib/systemd/system/hpct-importer.service


%package query
Summary: HiPerConTracer Query Tool to query results from a database
Group: Applications/Database
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-libuniversalimporter = %{version}-%{release}
Recommends: %{name} = %{version}-%{release}
Recommends: %{name}-dbshell = %{version}-%{release}
Recommends: %{name}-results = %{version}-%{release}
Recommends: %{name}-viewer = %{version}-%{release}

%description query
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains HiPerConTracer Query Tool to retrieve results from a
HiPerConTracer SQL or NoSQL database.

%files query
%{_bindir}/hpct-query
%{_datadir}/bash-completion/completions/hpct-query
%{_mandir}/man1/hpct-query.1.gz


%package results
Summary: HiPerConTracer Results Tool to process results files
Group: Applications/File
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-libuniversalimporter = %{version}-%{release}
Recommends: %{name} = %{version}-%{release}
Recommends: %{name}-viewer = %{version}-%{release}

%description results
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains the HiPerConTracer Results Tool to process
HiPerConTracer results files, particularly for converting them to CSV files
for reading them into spreadsheets, analysis tools, etc.

%files results
%{_bindir}/hpct-results
%{_bindir}/pipe-checksum
%{_datadir}/bash-completion/completions/hpct-results
%{_datadir}/bash-completion/completions/pipe-checksum
%{_mandir}/man1/hpct-results.1.gz
%{_mandir}/man1/pipe-checksum.1.gz


%package viewer
Summary: HiPerConTracer Viewer Tool to display results files
Group: Applications/File
BuildArch: noarch
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-libuniversalimporter = %{version}-%{release}
Requires: bzip2
Requires: gzip
Requires: less
Requires: xz
Recommends: %{name} = %{version}-%{release}
Recommends: %{name}-results = %{version}-%{release}

%description viewer
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains the HiPerConTracer Viewer Tool to display
HiPerConTracer results files.

%files viewer
%{_bindir}/hpct-viewer
%{_datadir}/applications/hpct-viewer.desktop
%{_datadir}/bash-completion/completions/hpct-viewer
%{_mandir}/man1/hpct-viewer.1.gz


%package udp-echo-server
Summary: HiPerConTracer UDP Echo server for responding to UDP Pings
Group: Applications/Internet
Recommends: %{name} = %{version}-%{release}

%description udp-echo-server
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains a simple UDP Echo server to respond to UDP Pings.

%files udp-echo-server
%{_bindir}/udp-echo-server
%{_datadir}/bash-completion/completions/udp-echo-server
%{_mandir}/man1/udp-echo-server.1.gz
%{_sysconfdir}/hipercontracer/udp-echo-server.conf
%{_prefix}/lib/systemd/system/udp-echo-server.service


%package dbshell
Summary: HiPerConTracer Database Shell for access testing to a database
Group: Applications/Database
BuildArch: noarch
Recommends: %{name} = %{version}-%{release}
Recommends: %{name}-dbeaver-tools = %{version}-%{release}
Recommends: (mariadb or mysql)
Recommends: mongodb-mongosh
Recommends: postgresql
Recommends: pwgen

%description dbshell
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains a simple tool to start a database shell, based on the
settings from a given database configuration file. It is mainly intended to
test database access using the configuration files for HiPerConTracer Importer
and HiPerConTracer Query Tool.

%files dbshell
%{_bindir}/dbshell
%{_datadir}/bash-completion/completions/dbshell
%{_mandir}/man1/dbshell.1.gz


%package dbeaver-tools
Summary: HiPerConTracer DBeaver Tools for configuring access to databases
Group: Applications/Database
BuildArch: noarch
Recommends: %{name}-dbshell = %{version}-%{release}
Requires: jq
Requires: openssl

%description dbeaver-tools
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This package contains helper scripts to merge HiPerConTracer database
configurations into DBeaver configurations, for maintaining databases
in DBeaver.

%files dbeaver-tools
%{_bindir}/decrypt-dbeaver-configuration
%{_bindir}/encrypt-dbeaver-configuration
%{_bindir}/make-dbeaver-configuration
%{_mandir}/man1/decrypt-dbeaver-configuration.1.gz
%{_mandir}/man1/encrypt-dbeaver-configuration.1.gz
%{_mandir}/man1/make-dbeaver-configuration.1.gz


%package all
Summary: HiPerConTracer meta-package for all tools of the framework
Group: Applications/Database
BuildArch: noarch
Requires: %{name} = %{version}-%{release}
Requires: %{name}-collector = %{version}-%{release}
Requires: %{name}-dbeaver-tools = %{version}-%{release}
Requires: %{name}-dbshell = %{version}-%{release}
Requires: %{name}-importer = %{version}-%{release}
Requires: %{name}-node = %{version}-%{release}
Requires: %{name}-query = %{version}-%{release}
Requires: %{name}-results = %{version}-%{release}
Requires: %{name}-sync = %{version}-%{release}
Requires: %{name}-trigger = %{version}-%{release}
Requires: %{name}-udp-echo-server = %{version}-%{release}
Requires: %{name}-viewer = %{version}-%{release}

%description all
High-Performance Connectivity Tracer (HiPerConTracer) is a Ping/Traceroute
measurement framework. HiPerConTracer denotes the actual measurement
tool. It performs regular Ping and Traceroute runs among sites, featuring:
multi-transport-protocol support (ICMP, UDP); multi-homing and parallelism
support; handling of load balancing in the network; multi-platform
support (currently Linux and FreeBSD); high-precision (nanoseconds)
timing support (Linux timestamping, both software and hardware); a
library (shared/static) to integrate measurement functionality into other
software (libhipercontracer); open source and written in a performance-
and portability-focused programming language (C++) with only limited
dependencies.
Furthermore, the HiPerConTracer Framework furthermore provides additional
tools for helping to obtain, process, collect, store, and retrieve
measurement data: HiPerConTracer Viewer Tool for displaying the contents
of results files; Results Tool for merging and converting results files,
e.g. to create a Comma-Separated Value (CSV) file; Sync Tool for copying data
from a measurement node (vantage point) to a remote HiPerConTracer Collector
server (via RSync/SSH); Reverse Tunnel Tool for maintaining a reverse SSH
tunnel from a remote measurement node to a HiPerConTracer Collector server;
Collector/Node Tools for simplifying the setup of HiPerConTracer Nodes and a
HiPerConTracer Collector server; Trigger Tool for triggering HiPerConTracer
measurements in the reverse direction; Importer Tool for storing measurement
data from results files into SQL or NoSQL databases. Currently, database
backends for MariaDB/MySQL PostgreSQL, MongoDB) are provided; Query
Tool for querying data from a database and storing it into a results
file; Database Shell as simple command-line front-end for the underlying
database backends; Database Tools with some helper scripts to e.g. to join
HiPerConTracer database configurations into an existing DBeaver (a popular
SQL database GUI application) configuration; UDP Echo Server as UDP Echo
(RFC 862) protocol endpoint; Wireshark dissector for HiPerConTracer packets.
This metapackage installs all sub-packages of the HiPerConTracer Framework.

%files all


%changelog
* Fri Apr 11 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.11
- New upstream release.
* Thu Apr 10 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.10
- New upstream release.
* Tue Apr 08 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.9
- New upstream release.
* Fri Apr 04 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.8
- New upstream release.
* Thu Apr 03 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.7
- New upstream release.
* Mon Mar 31 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.6
- New upstream release.
* Mon Feb 17 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.5
- New upstream release.
* Sun Feb 16 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.4
- New upstream release.
* Sun Feb 16 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.3
- New upstream release.
* Wed Jan 29 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.2
- New upstream release.
* Tue Jan 28 2025 Thomas Dreibholz <dreibh@simula.no> - 2.0.1
- New upstream release.
* Fri Dec 20 2024 Thomas Dreibholz <dreibh@simula.no> - 2.0.0
- New upstream release.
* Wed Dec 06 2023 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.6.10
- New upstream release.
* Thu Sep 21 2023 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.6.9
- New upstream release.
* Tue Apr 18 2023 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.6.8
- New upstream release.
* Sun Jan 22 2023 Thomas Dreibholz <thomas.dreibholz@gmail.com> - 1.6.7
- New upstream release.
* Sun Sep 11 2022 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.6.6
- New upstream release.
* Wed Feb 16 2022 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.6.5
- New upstream release.
* Fri Dec 03 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.6.4
- New upstream release.
* Mon Nov 08 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.6.3
- New upstream release.
* Wed Nov 03 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.6.2
- New upstream release.
* Wed Sep 01 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.6.1
- New upstream release.
* Mon May 03 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.6.0
- New upstream release.
* Sat Mar 06 2021 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.5.0
- New upstream release.
* Wed Nov 18 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.12
- New upstream release.
* Sat Nov 14 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.11
- New upstream release.
* Fri Nov 13 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.10
- New upstream release.
* Tue Apr 28 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.9
- New upstream release.
* Fri Apr 24 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.8
- New upstream release.
* Fri Feb 07 2020 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.7
- New upstream release.
* Mon Aug 12 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.6
- New upstream release.
* Wed Aug 07 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.5
- New upstream release.
* Thu Aug 01 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.4
- New upstream release.
* Wed Jul 31 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.3
- New upstream release.
* Tue Jul 23 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.2
- New upstream release.
* Thu Jul 11 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.1
- New upstream release.
* Fri Jun 14 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.4.0
- New upstream release.
* Thu Jun 13 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.3.2
- New upstream release.
* Thu Jun 06 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.3.1
- New upstream release.
* Tue May 21 2019 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.3.0
- New upstream release.
* Tue Feb 28 2017 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.1.0
- Created RPM package.
