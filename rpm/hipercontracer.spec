Name: hipercontracer
Version: 1.6.3~test2
Release: 1
Summary: High-Performance Connectivity Tracer (HiPerConTracer)
Group: Applications/Internet
License: GPL-3+
URL: https://www.uni-due.de/~be0001/hipercontracer/
Source: https://www.uni-due.de/~be0001/hipercontracer/download/%{name}-%{version}.tar.xz

AutoReqProv: on
BuildRequires: cmake
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: boost-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-build
Requires: %{name}-libhipercontracer = %{version}-%{release}
Recommends: python3-psycopg2
Recommends: python3-pymongo
Recommends: python3-urllib3
Recommends: python3-GeoIP


# TEST ONLY:
# define _unpackaged_files_terminate_build 0


%description
High-Performance Connectivity Tracer (HiPerConTracer) is a ping/traceroute service. It performs regular ping and traceroute runs among sites and can export the results into an SQL database.

%prep
%setup -q

%build
# NOTE: CMAKE_VERBOSE_MAKEFILE=OFF for reduced log output!
%cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_VERBOSE_MAKEFILE=OFF .
%cmake_build

%pre
if ! getent group hipercontracer >/dev/null 2>&1; then
   groupadd -r hipercontracer
fi
if ! getent passwd hipercontracer >/dev/null 2>&1; then
   useradd -M -g hipercontracer -r -d / -s /sbin/nologin -c "HiPerConTracer User" hipercontracer
fi

%postun
userdel hipercontracer >/dev/null 2>&1 || true
groupdel hipercontracer >/dev/null 2>&1 || true

%install
%cmake_install

%files
%{_bindir}/addressinfogenerator
%{_bindir}/get-default-ips
%{_bindir}/hipercontracer
%{_bindir}/tracedataimporter
%{_mandir}/man1/addressinfogenerator.1.gz
%{_mandir}/man1/get-default-ips.1.gz
%{_mandir}/man1/hipercontracer.1.gz
%{_mandir}/man1/tracedataimporter.1.gz
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


%package libhipercontracer
Summary: HiPerConTracer library
Group: System Environment/Libraries
Requires: %{name}-libhipercontracer = %{version}-%{release}

%description libhipercontracer
High-Performance Connectivity Tracer (HiPerConTracer) is a
ping/traceroute service. It performs regular ping and traceroute runs
among sites and can export the results into an SQL or Non-SQL database.
The HiPerConTracer library is provided by this package.

%files libhipercontracer
%{_libdir}/libhipercontracer.so.*


%package libhipercontracer-devel
Summary: HiPerConTracer library development files
Group: Development/Libraries
Requires: %{name}-libhipercontracer = %{version}-%{release}
Requires: boost-devel

%description libhipercontracer-devel
High-Performance Connectivity Tracer (HiPerConTracer) is a
ping/traceroute service. It performs regular ping and traceroute runs
among sites and can export the results into an SQL or Non-SQL database.
This package provides header files for the HiPerConTracer library. You need them
to integrate HiPerConTracer into own programs.


%files libhipercontracer-devel
%{_includedir}/hipercontracer/destinationinfo.h
%{_includedir}/hipercontracer/logger.h
%{_includedir}/hipercontracer/ping.h
%{_includedir}/hipercontracer/resultentry.h
%{_includedir}/hipercontracer/resultswriter.h
%{_includedir}/hipercontracer/service.h
%{_includedir}/hipercontracer/tools.h
%{_includedir}/hipercontracer/traceroute.h
%{_libdir}/libhipercontracer*.so
%{_libdir}/libhipercontracer.a


%package hipercontracer-trigger
Summary: HiPerConTracer trigger tool
Group: Applications/Internet
Requires: %{name} = %{version}-%{release}
Requires: %{name}-libhipercontracer = %{version}-%{release}
Recommends: python3-psycopg2
Recommends: python3-pymongo
Recommends: python3-urllib3
Recommends: python3-GeoIP

%description hipercontracer-trigger
High-Performance Connectivity Tracer (HiPerConTracer) is a
ping/traceroute service. It performs regular ping and traceroute runs
among sites and can export the results into an SQL or Non-SQL database.
This tool triggers HiPerConTracer by incoming "Ping" packets.

%files hipercontracer-trigger
%{_bindir}/hpcttrigger
%{_mandir}/man1/hpcttrigger.1.gz


%changelog
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
