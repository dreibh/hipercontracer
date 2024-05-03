# Created by: Thomas Dreibholz <thomas.dreibholz@gmail.com>

PORTNAME=	hipercontracer
PORTVERSION=	2.0.0~beta4
PORTREVISION=	1
CATEGORIES=	benchmarks
MASTER_SITES=	https://www.nntb.no/~dreibh/hipercontracer/download/

MAINTAINER=	thomas.dreibholz@gmail.com
COMMENT=	High-Performance Connectivity Tracer (HiPerConTracer)
WWW=		https://www.nntb.no/~dreibh/hipercontracer/

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

LIB_DEPENDS=	bzip2 \
		c-ares \
		libboost_date_time.so:devel/boost-libs \
		libboost_filesystem.so:devel/boost-libs \
		libboost_iostreams.so:devel/boost-libs \
		libboost_log.so:devel/boost-libs \
		libboost_program_options.so:devel/boost-libs \
		libboost_system.so:devel/boost-libs \
		libboost_thread.so:devel/boost-libs \
		lzmalib \
		mysql-connector-c++ \
		postgresql-libpqxx \
		mongo-c-driver

USES=		cmake compiler:c++17-lang python shebangfix tar:xz
USE_LDCONFIG=	yes

PLIST_SUB=	PORTVERSION=${PORTVERSION}
SHEBANG_FILES=	src/get-default-ips

.include <bsd.port.mk>
