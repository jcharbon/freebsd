#
# $FreeBSD$
#

PROG=	bhyvectl
SRCS=	bhyvectl.c

MAN=

LIBADD=	vmmapi

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../../sys/amd64/vmm

.include <bsd.prog.mk>
