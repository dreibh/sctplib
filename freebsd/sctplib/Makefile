# Created by: Thomas Dreibholz <dreibh@iem.uni-due.de>
# $FreeBSD$

PORTNAME=	sctplib
PORTVERSION=	1.0.23
CATEGORIES=	net
MASTER_SITES=	https://www.uni-due.de/~be0001/sctplib/download/

MAINTAINER=	dreibh@iem.uni-due.de
COMMENT=	User-space implementation of the SCTP protocol RFC 4960

LICENSE=	GPLv3+
LICENSE_FILE=   ${WRKSRC}/COPYING

LIB_DEPENDS=	libglib-2.0.so:devel/glib20

GNU_CONFIGURE=	yes
CONFIGURE_ARGS=	--enable-sctp-over-udp

.include <bsd.port.mk>
