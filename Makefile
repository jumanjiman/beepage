SRC=    daemon.c command.c argcargv.c config.c queue.c modem.c sendmail.c tp.c
DOBJ=	daemon.o command.o argcargv.o config.o queue.o modem.o sendmail.o
COBJ=	tp.o

DESTDIR=

INCPATH =	-I../libnet
DEFS=	-DLOG_TPPD=LOG_LOCAL6
CFLAGS=	${DEFS} ${OPTOPTS} ${INCPATH}
TAGSFILE=	tags
LIBDIRS=	-L../libnet
LIBS=	${ADDLIBS} -lnet
CC=	cc
INSTALL=	install

all : tppd tp

tp : ${COBJ}
	${CC} ${CFLAGS} ${LDFLAGS} -o tp ${COBJ} ${LIBDIRS} ${LIBS}

tppd : ${DOBJ}
	${CC} ${CFLAGS} ${LDFLAGS} -o tppd ${DOBJ} ${LIBDIRS} ${LIBS}

install : all
	${INSTALL} -c msapd ${DESTDIR}/etc

clean :
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
