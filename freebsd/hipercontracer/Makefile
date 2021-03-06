# Created by: Thomas Dreibholz <dreibh@iem.uni-due.de>
# $FreeBSD$

PORTNAME=	hipercontracer
PORTVERSION=	1.5.0
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.uni-due.de/~be0001/hipercontracer/download/

MAINTAINER=	dreibh@iem.uni-due.de
COMMENT=	High-Performance Connectivity Tracer (HiPerConTracer)

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

USES=		tar:xz cmake
USE_LDCONFIG=	yes

LIB_DEPENDS=    libboost_date_time.so:devel/boost-libs \
                libboost_filesystem.so:devel/boost-libs \
                libboost_iostreams.so:devel/boost-libs \
                libboost_log.so:devel/boost-libs \
                libboost_program-options.so:devel/boost-libs \
                libboost_system.so:devel/boost-libs \
                libboost_thread.so:devel/boost-libs

CMAKE_ARGS+=    -DCMAKE_INSTALL_MANDIR=${PREFIX}/man

PLIST_SUB=      VERSION=${PORTVERSION}

.include <bsd.port.mk>
