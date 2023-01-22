# Created by: Thomas Dreibholz <thomas.dreibholz@gmail.com>
# $FreeBSD: head/net/sctplib/Makefile 509201 2019-08-18 11:11:21Z pi $

PORTNAME=	sctplib
PORTVERSION=	1.0.30
CATEGORIES=	net
MASTER_SITES=	https://www.nntb.no/~dreibh/sctplib/download/

MAINTAINER=	thomas.dreibholz@gmail.com
COMMENT=	User-space implementation of the SCTP protocol RFC 4960

LICENSE=	GPLv3+
LICENSE_FILE=	${WRKSRC}/COPYING

USES=		gnome libtool pkgconfig
USE_LDCONFIG=	yes
USE_GNOME=	glib20

GNU_CONFIGURE=	yes
CONFIGURE_ARGS=	--enable-sctp-over-udp
INSTALL_TARGET=	install-strip
PLIST_FILES=	include/sctp.h \
		lib/libsctplib.a \
		lib/libsctplib.so \
		lib/libsctplib.so.1 \
		lib/libsctplib.so.1.0.8

.include <bsd.port.mk>
