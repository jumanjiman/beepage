################## Some of this may need to be edited ##################

DESTDIR=/usr/local
MANDIR=${DESTDIR}/man
BINDIR=${DESTDIR}/bin
ETCDIR=${DESTDIR}/etc

# on older systems
#SBINDIR=${DESTDIR}/etc
# on newer systems
SBINDIR=${DESTDIR}/sbin

TPPDSYSLOG=LOG_LOCAL6

# Stock compiler:
CC=	cc

# For gcc:
#CC=	gcc
# These options might work on your system:
#OPTOPTS=-Wall -Wstrict-prototypes -Wmissing-prototypes -Wconversion -Werror

# For most platforms:
INSTALL=	install

# For Solaris:
#INSTALL=	/usr/ucb/install
#ADDLIBS=	-lsocket -lnsl

# To enable Kerberos v4, uncomment these lines.
#KDEFS=	-DKRB
#KSRC=	binhex.c
#KOBJ=	binhex.o

# For MIT Kerberos v4
#KERBEROS=	/usr/local/kerberos
#KINCPATH=	-I${KERBEROS}/include
#KLIBPATH=	-L${KERBEROS}/lib
#KLIBS=	-lkrb -ldes

# For Solaris Kerberos v4
#KINCPATH=	-I/usr/include/kerberos
#KLIBS=	-lkrb

################ Nothing below should need editing ###################

SRC=    daemon.c command.c argcargv.c config.c queue.c modem.c sendmail.c \
	tp.c flock.c ${KSRC}
DOBJ=	daemon.o command.o argcargv.o config.o queue.o modem.o sendmail.o \
	flock.o ${KOBJ}
COBJ=	tp.o ${KOBJ}

INCPATH=	-Ilibnet ${KINCPATH}
DEFS=	-DLOG_TPPD=${TPPDSYSLOG} ${KDEFS}
CFLAGS=	${DEFS} ${OPTOPTS} ${INCPATH}
TAGSFILE=	tags
LIBPATH=	-Llibnet ${KLIBPATH}
LIBS=	${ADDLIBS} -lnet ${KLIBS}

all : tppd tp

tp.o : tp.c
	${CC} ${CFLAGS} \
	    -DVERSION=\"`cat VERSION`\" \
	    -c tp.c

tp : libnet/libnet.a ${COBJ} Makefile
	${CC} ${CFLAGS} ${LDFLAGS} -o tp ${COBJ} ${LIBPATH} ${LIBS}

daemon.o : daemon.c
	${CC} ${CFLAGS} \
	    -D_PATH_USRDB=\"${ETCDIR}/tppd.users\" \
	    -D_PATH_GRPDB=\"${ETCDIR}/tppd.aliases\" \
	    -D_PATH_SRVDB=\"${ETCDIR}/tppd.services\" \
	    -D_PATH_PIDFILE=\"${ETCDIR}/tppd.pid\" \
	    -DVERSION=\"`cat VERSION`\" \
	    -c daemon.c

tppd : libnet/libnet.a ${DOBJ} Makefile
	${CC} ${CFLAGS} ${LDFLAGS} -o tppd ${DOBJ} ${LIBPATH} ${LIBS}

FRC :

libnet/libnet.a : FRC
	cd libnet; ${MAKE} ${MFLAGS} CC=${CC}

VERSION=`date +%y%m%d`
DISTDIR=../beepage-${VERSION}

dist : clean
	mkdir ${DISTDIR}
	tar chfFFX - EXCLUDE . | ( cd ${DISTDIR}; tar xvf - )
	chmod +w ${DISTDIR}/Makefile
	echo ${VERSION} > ${DISTDIR}/VERSION

install : all
	${INSTALL} -o root -g tpp -m 4750 -c tppd ${SBINDIR}/
	${INSTALL} -c tp ${BINDIR}/
	${INSTALL} -m 0444 -c tp.1 ${MANDIR}/man1/
	sed -e s@:ETCDIR:@${ETCDIR}@ < tppd.8 > tppd.0
	${INSTALL} -m 0444 tppd.0 ${MANDIR}/man8/tppd.8
	rm -f tppd.0

clean :
	cd libnet; ${MAKE} ${MFLAGS} clean
	rm -f a.out core* *.o *.bak *[Ee]rrs tags
	rm -f tppd tp

tags : ${SRC}
	cwd=`pwd`; \
	for i in ${SRC}; do \
	    ctags -t -a -f ${TAGSFILE} $$cwd/$$i; \
	done

depend :
	for i in ${SRC} ; do \
	    ${CC} -M ${DEFS} ${INCPATH} $$i | \
	    awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		else rec = rec " " $$2 } } \
		END { print rec } ' >> makedep; done
	sed -n '1,/^# DO NOT DELETE THIS LINE/p' Makefile > Makefile.tmp
	cat makedep >> Makefile.tmp
	rm makedep
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile.tmp
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile.tmp
	echo '# see make depend above' >> Makefile.tmp
	rm -f Makefile.bak
	cp Makefile Makefile.bak
	mv Makefile.tmp Makefile

# DO NOT DELETE THIS LINE
