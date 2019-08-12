# Created by: Thomas Dreibholz <dreibh@iem.uni-due.de>
# $FreeBSD$

PORTNAME=	hipercontracer
PORTVERSION=	1.4.5
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.uni-due.de/~be0001/hipercontracer/download/

MAINTAINER=	dreibh@iem.uni-due.de
COMMENT=	High-Performance Connectivity Tracer (HiPerConTracer)

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

USES=		tar:xz cmake shebangfix

CMAKE_ARGS+=    -DCMAKE_INSTALL_MANDIR=${PREFIX}/man
SHEBANG_FILES=	src/addressinfogenerator src/get-default-ips src/tracedataimporter

.include <bsd.port.mk>
