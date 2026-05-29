PORTNAME=	hipercontracer
DISTVERSION=	2.2.1
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.nntb.no/~dreibh/hipercontracer/download/

MAINTAINER=	thomas.dreibholz@gmail.com
COMMENT=	High-Performance Connectivity Tracer (HiPerConTracer)
WWW=		https://www.nntb.no/~dreibh/hipercontracer/

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

LIB_DEPENDS=	libboost_date_time.so:devel/boost-libs \
		libboost_filesystem.so:devel/boost-libs \
		libboost_iostreams.so:devel/boost-libs \
		libboost_log.so:devel/boost-libs \
		libboost_program_options.so:devel/boost-libs \
		libboost_thread.so:devel/boost-libs \
		libinotify.so:devel/libinotify
RUN_DEPENDS=	getopt>=0:misc/getopt

USES=		cmake compiler:c++20-lang python \
		shared-mime-info shebangfix ssl tar:xz
USE_LDCONFIG=	yes

SHEBANG_FILES=	src/results-examples/r-install-dependencies \
		src/results-examples/r-ping-example \
		src/results-examples/r-traceroute-example \
		src/TestDB/generate-test-certificates

# Work-around for build issue on ARM (https://bugs.freebsd.org/285706):
MAKE_ENV=	OMP_NUM_THREADS=1

PLIST_SUB=	DISTVERSION=${DISTVERSION}

OPTIONS_DEFINE=					COLLECTOR \
						DBEAVERTOOLS \
						DBSHELL \
						ICONS \
						IMPORTER_BACKEND_MARIADB \
						IMPORTER_BACKEND_MONGO IMPORTER \
						IMPORTER_BACKEND_POSTGRESQL \
						NODE \
						QUERYTOOL \
						RESULTSTOOL \
						RTUNNEL \
						SYNC \
						TRIGGER \
						UDPECHO \
						VIEWERTOOL
OPTIONS_DEFAULT=				${OPTIONS_DEFINE}
OPTIONS_SUB=					yes

COLLECTOR_DESC=					Build HiPerConTracer Collector Tools
COLLECTOR_RUN_DEPENDS=				rsync:net/rsync \
						sudo:security/sudo

DBEAVERTOOLS_DESC=				Build DBeaver Tools
DBSHELL_DESC=					Build DBShell
ICONS_DESC=					Build Icons and Logo Files

IMPORTER_DESC=					Build HiPerConTracer Importer Tool
IMPORTER_BACKEND_MARIADB_DESC=			Enable MariaDB/MySQL support for HiPerConTracer Importer Tool
IMPORTER_BACKEND_MONGO_DESC=			Enable MongoDB support for HiPerConTracer Importer Tool
IMPORTER_BACKEND_POSTGRESQL_DESC=		Enable PostgreSQL support for HiPerConTracer Importer Tool

NODE_DESC=					Build HiPerConTracer Node Tools
NODE_RUN_DEPENDS=				sudo:security/sudo

QUERYTOOL_DESC=					Build HiPerConTracer Query Tool
RESULTSTOOL_DESC=				Build HiPerConTracer Results Tool

RTUNNEL_DESC=					Build HiPerConTracer Reverse Tunnel Tool

SYNC_DESC=					Build HiPerConTracer Synchronisation Tool
SYNC_RUN_DEPENDS=				rsync:net/rsync

TRIGGER_DESC=					Build HiPerConTracer Trigger Tool
UDPECHO_DESC=					Build UDP Echo Server

VIEWERTOOL_DESC=				Build HiPerConTracer Viewer Tool
VIEWERTOOL_RUN_DEPENDS=				xz:archivers/xz \
						gzip:archivers/gzip \
						bzip2:archivers/bzip2 \
						zstd:archivers/zstd \
						less:sysutils/less
VIEWERTOOL_USES=				desktop-file-utils

COLLECTOR_CMAKE_BOOL=				WITH_COLLECTOR
DBEAVERTOOLS_CMAKE_BOOL=			WITH_DBEAVER_TOOLS
DBSHELL_CMAKE_BOOL=				WITH_DBSHELL

ICONS_CMAKE_BOOL=				WITH_ICONS
ICONS_BUILD_DEPENDS=				gm:graphics/GraphicsMagick \
						gs:print/ghostscript10 \
						pdf2svg:graphics/pdf2svg

IMPORTER_CMAKE_BOOL=				WITH_IMPORTER

IMPORTER_BACKEND_MARIADB_IMPLIES=		WITH_IMPORTER
IMPORTER_BACKEND_MARIADB_LIB_DEPENDS=		libmariadb.so:databases/mariadb118-client
IMPORTER_BACKEND_MARIADB_CMAKE_BOOL=		ENABLE_BACKEND_MARIADB

IMPORTER_BACKEND_MONGODB_IMPLIES=		WITH_IMPORTER
IMPORTER_BACKEND_MONGO_LIB_DEPENDS=		libbson-1.0.so:devel/libbson \
				l		ibmongoc-1.0.so:devel/mongo-c-driver
IMPORTER_BACKEND_MONGO_CMAKE_BOOL=		ENABLE_BACKEND_MONGODB

IMPORTER_BACKEND_POSTGRESQL_IMPLIES=		WITH_IMPORTER
IMPORTER_BACKEND_POSTGRESQL_LIB_DEPENDS=	libpqxx.so:databases/postgresql-libpqxx
IMPORTER_BACKEND_POSTGRESQL_CMAKE_BOOL=		ENABLE_BACKEND_POSTGRESQL

NODE_CMAKE_BOOL=				WITH_NODE
QUERYTOOL_CMAKE_BOOL=				WITH_QUERY
RESULTSTOOL_CMAKE_BOOL=				WITH_RESULTS
RTUNNEL_CMAKE_BOOL=				WITH_RTUNNEL
SYNC_CMAKE_BOOL=				WITH_SYNC
TRIGGER_CMAKE_BOOL=				WITH_TRIGGER
UDPECHO_CMAKE_BOOL=				WITH_UDP_ECHO_SERVER
VIEWERTOOL_CMAKE_BOOL=				WITH_VIEWER

.include <bsd.port.mk>
