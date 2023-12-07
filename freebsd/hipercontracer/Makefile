# Created by: Thomas Dreibholz <thomas.dreibholz@gmail.com>

PORTNAME=	hipercontracer
PORTVERSION=	1.6.10
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
		libboost_system.so:devel/boost-libs \
		libboost_thread.so:devel/boost-libs

USES=		cmake compiler:c++14-lang python:3.9+ shebangfix tar:xz
USE_LDCONFIG=	yes

SHEBANG_FILES=	src/addressinfogenerator src/get-default-ips src/tracedataimporter

CMAKE_ARGS+=	-DCMAKE_INSTALL_MANDIR=${PREFIX}/man
PLIST_SUB=	PORTVERSION=${PORTVERSION}

.include <bsd.port.mk>
