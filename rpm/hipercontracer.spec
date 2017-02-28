Name: hipercontracer
Version: 1.1.0~rc1.0
Release: 1
Summary: IPv4/IPv6 Subnet Calculator
Group: Applications/Internet
License: GPLv3
URL: http://www.iem.uni-due.de/~dreibh/hipercontracer/
Source: http://www.iem.uni-due.de/~dreibh/hipercontracer/download/%{name}-%{version}.tar.gz

AutoReqProv: on
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: libtool
BuildRoot: %{_tmppath}/%{name}-%{version}-build

%define _unpackaged_files_terminate_build 0

%description
High-Performance Connectivity Tracer (HiPerConTracer) is a ping/traceroute service. It performs regular ping and traceroute runs among sites and can export the results into an SQL database.

%prep
%setup -q

%build
autoreconf -if

%configure
make %{?_smp_mflags}

%install
make DESTDIR=%{buildroot} install

%files
%{_bindir}/addressinfogenerator
%{_bindir}/hipercontracer
%{_bindir}/tracedataimporter
%{_datadir}/man/man1/addressinfogenerator.1.gz
%{_datadir}/man/man1/hipercontracer.1.gz
%{_datadir}/man/man1/tracedataimporter.1.gz

%doc

%changelog
* Tue Feb 28 2017 Thomas Dreibholz <dreibh@iem.uni-due.de> - 1.1.0
- Created RPM package.
