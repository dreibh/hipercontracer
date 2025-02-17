PORTNAME=	hipercontracer
DISTVERSION=	2.0.5
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.nntb.no/~dreibh/hipercontracer/download/

MAINTAINER=	dreibh@simula.no
COMMENT=	High-Performance Connectivity Tracer (HiPerConTracer)
WWW=		https://www.nntb.no/~dreibh/hipercontracer/

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

LIB_DEPENDS=	libinotify.so:devel/libinotify \
		libboost_date_time.so:devel/boost-libs \
		libboost_filesystem.so:devel/boost-libs \
		libboost_iostreams.so:devel/boost-libs \
		libboost_log.so:devel/boost-libs \
		libboost_program_options.so:devel/boost-libs \
		libboost_system.so:devel/boost-libs \
		libboost_thread.so:devel/boost-libs

USES=		cmake \
		compiler:c++20-lang \
		desktop-file-utils \
		python \
		shared-mime-info \
		shebangfix \
		ssl \
		tar:xz
USE_LDCONFIG=	yes

SHEBANG_FILES=	src/results-examples/r-traceroute-example \
		src/results-examples/r-ping-example \
		src/results-examples/r-install-dependencies \
		src/TestDB/generate-test-certificates

PLIST_SUB=	PORTVERSION=${PORTVERSION}

SUB_FILES=	pkg-deinstall pkg-install

OPTIONS_DEFINE=			TRIGGER IMPORTER IMPMARIADB IMPPGSQL IMPMONGO SYNC RTUNNEL NODE COLLECTOR QUERYTOOL RESULTSTOOL VIEWERTOOL UDPECHO DBSHELL DBEAVERTOOLS ICONS
OPTIONS_DEFAULT=		TRIGGER IMPORTER IMPMARIADB IMPPGSQL IMPMONGO SYNC RTUNNEL NODE COLLECTOR QUERYTOOL RESULTSTOOL VIEWERTOOL UDPECHO DBSHELL DBEAVERTOOLS ICONS
TRIGGER_DESC=			Build HiPerConTracer Trigger Tool
IMPORTER_DESC=			Build HiPerConTracer Importer Tool
IMPMARIADB_DESC=		Enable MariaDB/MySQL support for HiPerConTracer Importer Tool
IMPPGSQL_DESC=			Enable PostgreSQL support for HiPerConTracer Importer Tool
IMPMONGO_DESC=			Enable MongoDB support for HiPerConTracer Importer Tool
SYNC_DESC=			Build HiPerConTracer Synchronisation Tool
RTUNNEL_DESC=			Build HiPerConTracer Reverse Tunnel Tool"
NODE_DESC=			Build HiPerConTracer Node Tools"
COLLECTOR_DESC=			Build HiPerConTracer Collector Tools"
QUERYTOOL_DESC=			Build HiPerConTracer Query Tool
RESULTSTOOL_DESC=		Build HiPerConTracer Results Tool
VIEWERTOOL_DESC=		Build HiPerConTracer Viewer Tool
UDPECHO_DESC=			Build UDP Echo Server
DBSHELL_DESC=			Build DBShell
DBEAVERTOOLS_DESC=		Build DBeaver Tools
ICONS_DESC=			Build Icons and Logo Files

TRIGGER_CMAKE_BOOL=		WITH_TRIGGER
IMPORTER_CMAKE_BOOL=		WITH_IMPORTER
SYNC_CMAKE_BOOL=		WITH_SYNC
RTUNNEL_CMAKE_BOOL=		WITH_RTUNNEL
NODE_CMAKE_BOOL=		WITH_NODE
COLLECTOR_CMAKE_BOOL=		WITH_COLLECTOR
QUERYTOOL_CMAKE_BOOL=		WITH_QUERY
RESULTSTOOL_CMAKE_BOOL=		WITH_RESULTS
VIEWERTOOL_CMAKE_BOOL=		WITH_VIEWER
UDPECHO_CMAKE_BOOL=		WITH_UDP_ECHO_SERVER
DBSHELL_CMAKE_BOOL=		WITH_DBSHELL
DBEAVERTOOLS_CMAKE_BOOL=	WITH_DBEAVER_TOOLS

IMPMARIADB_CMAKE_BOOL=		ENABLE_BACKEND_MARIADB
IMPMARIADB_LIB_DEPENDS=		libmariadb.so:databases/mariadb106-client

IMPPGSQL_MAKE_BOOL=		ENABLE_BACKEND_POSTGRESQL
IMPPGSQL_LIB_DEPENDS=		libpqxx.so:databases/postgresql-libpqxx

IMPMONGO_CMAKE_BOOL=		ENABLE_BACKEND_MONGODB
IMPMONGO_LIB_DEPENDS=		libmongoc-1.0.so:devel/mongo-c-driver \
                                libbson-1.0.so:devel/libbson

ICONS_CMAKE_BOOL=		WITH_ICONS
ICONS_BUILD_DEPENDS=		gm:graphics/GraphicsMagick \
				pdf2svg:graphics/pdf2svg \
				gs:print/ghostscript10

PLIST_SUB=	DISTVERSION=${DISTVERSION}

.include <bsd.port.mk>
