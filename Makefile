################## Some of this may need to be edited ##################

DESTDIR=/usr/local/beepage
MANDIR=${DESTDIR}/man
BINDIR=${DESTDIR}/bin
ETCDIR=${DESTDIR}/etc

# on older systems
#SBINDIR=${DESTDIR}/etc
# on newer systems
SBINDIR=${DESTDIR}/sbin

USRDB=${ETCDIR}/users
GRPDB=${ETCDIR}/aliases
SRVDB=${ETCDIR}/services
PIDFILE=${ETCDIR}/pid

BEEPAGEDSYSLOG=LOG_LOCAL6

# Stock compiler:
#CC=	cc

# For gcc:
CC=	gcc
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

# For debugging, skip modem code
DMODEM=	-DNOMODEM

################ Nothing below should need editing ###################

SRC=    daemon.c command.c argcargv.c config.c queue.c modem.c sendmail.c \
	compress.c rfc822.c beep.c flock.c rfc2045.c ${KSRC}
DOBJ=	daemon.o command.o argcargv.o config.o queue.o modem.o sendmail.o \
	compress.o flock.o rfc822.o rfc2045.o ${KOBJ}
COBJ=	beep.o ${KOBJ}

INCPATH=	-Ilibnet ${KINCPATH}
DEFS=	-DLOG_BEEPAGED=${BEEPAGEDSYSLOG} ${KDEFS} ${DMODEM}
CFLAGS=	${DEFS} ${OPTOPTS} ${INCPATH}
TAGSFILE=	tags
LIBPATH=	-Llibnet ${KLIBPATH}
LIBS=	${ADDLIBS} -lnet ${KLIBS}

all : beepaged beep

beep.o : beep.c
	${CC} ${CFLAGS} \
	    -DVERSION=\"`cat VERSION`\" \
	    -c beep.c

beep : libnet/libnet.a ${COBJ} Makefile
	${CC} ${CFLAGS} ${LDFLAGS} -o beep ${COBJ} ${LIBPATH} ${LIBS}

daemon.o : daemon.c
	${CC} ${CFLAGS} \
	    -D_PATH_USRDB=\"${USRDB}\" \
	    -D_PATH_GRPDB=\"${GRPDB}\" \
	    -D_PATH_SRVDB=\"${SRVDB}\" \
	    -D_PATH_PIDFILE=\"${PIDFILE}\" \
	    -DVERSION=\"`cat VERSION`\" \
	    -c daemon.c

beepaged : libnet/libnet.a ${DOBJ} Makefile
	${CC} ${CFLAGS} ${LDFLAGS} -o beepaged ${DOBJ} ${LIBPATH} ${LIBS}

FRC :

libnet/libnet.a : FRC
	cd libnet; ${MAKE} ${MFLAGS} CC=${CC}

VERSION=`date +%Y%m%d`
DISTDIR=../beepage-${VERSION}

dist : clean
	mkdir ${DISTDIR}
	tar chfFFX - EXCLUDE . | ( cd ${DISTDIR}; tar xvf - )
	chmod +w ${DISTDIR}/Makefile
	echo ${VERSION} > ${DISTDIR}/VERSION

install : all
	-mkdir -p ${DESTDIR}
	-mkdir -p ${SBINDIR}
	${INSTALL} -o root -g beepage -m 4750 -c beepaged ${SBINDIR}/
	-mkdir -p ${BINDIR}
	${INSTALL} -c beep ${BINDIR}/
	-mkdir -p ${MANDIR}/man1
	${INSTALL} -m 0444 -c beep.1 ${MANDIR}/man1/
	-mkdir -p ${MANDIR}/man8
	sed -e s@:ETCDIR:@${ETCDIR}@ < beepaged.8 > beepaged.0
	${INSTALL} -m 0444 beepaged.0 ${MANDIR}/man8/beepaged.8
	rm -f beepaged.0

clean :
	cd libnet; ${MAKE} ${MFLAGS} clean
	rm -f a.out core* *.o *.bak *[Ee]rrs tags
	rm -f beepaged beep

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
