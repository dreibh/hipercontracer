Name: hipercontracer
Version: 1.2.0~rc1
Release: 1
Summary: High-Performance Connectivity Tracer (HiPerConTracer)
Group: Applications/Internet
License: GPLv3
URL: https://www.uni-due.de/~be0001/hipercontracer/
Source: https://www.uni-due.de/~be0001/hipercontracer/download/%{name}-%{version}.tar.gz

AutoReqProv: on
BuildRequires: cmake
BuildRequires: boost-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-build

%define _unpackaged_files_terminate_build 0

%description
High-Performance Connectivity Tracer (HiPerConTracer) is a ping/traceroute service. It performs regular ping and traceroute runs among sites and can export the results into an SQL database.

%prep
%setup -q

%build
%cmake -DCMAKE_INSTALL_PREFIX=/usr .
make %{?_smp_mflags}

%pre
/usr/sbin/groupadd -g 888 -r hipercontracer >/dev/null 2>&1 || :
/usr/sbin/useradd -M -N -g hipercontracer -o -r -d /tmp -s /bin/bash -c "HiPerConTracer User" -u 888 hipercontracer >/dev/null 2>&1 || :

%install
make install DESTDIR=%{buildroot}

%files
%{_bindir}/addressinfogenerator
%{_bindir}/hipercontracer
%{_bindir}/tracedataimporter
%{_datadir}/man/man1/addressinfogenerator.1.gz
%{_datadir}/man/man1/hipercontracer.1.gz
%{_datadir}/man/man1/tracedataimporter.1.gz
%{_datadir}/doc/hipercontracer/examples/hipercontracer-database-configuration
%{_datadir}/doc/hipercontracer/examples/SQL/README
%{_datadir}/doc/hipercontracer/examples/SQL/database.sql
%{_datadir}/doc/hipercontracer/examples/SQL/install-database-and-users
%{_datadir}/doc/hipercontracer/examples/SQL/login-as-importer
%{_datadir}/doc/hipercontracer/examples/SQL/login-as-researcher
%{_datadir}/doc/hipercontracer/examples/SQL/procedures.sql
%{_datadir}/doc/hipercontracer/examples/SQL/schema.sql
%{_datadir}/doc/hipercontracer/examples/SQL/users.sql
%{_datadir}/doc/hipercontracer/examples/NoSQL/admin.ms
%{_datadir}/doc/hipercontracer/examples/NoSQL/database.ms
%{_datadir}/doc/hipercontracer/examples/NoSQL/install-database-and-users
%{_datadir}/doc/hipercontracer/examples/NoSQL/schema.ms
%{_datadir}/doc/hipercontracer/examples/NoSQL/users.ms
%{_datadir}/doc/hipercontracer/examples/NoSQL/R-query-example.R
%{_datadir}/doc/hipercontracer/examples/NoSQL/README

%doc

%changelog
* Tue Feb 28 2017 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.1.0
- Created RPM package.
