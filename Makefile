SRC=    daemon.c command.c argcargv.c config.c queue.c modem.c sendmail.c tp.c \
	${KSRC}
DOBJ=	daemon.o command.o argcargv.o config.o queue.o modem.o sendmail.o \
	${KOBJ}
COBJ=	tp.o ${KOBJ}

DESTDIR=/usr/local

#KERBEROS=	/usr/local/kerberos
#KINCPATH=	-I${KERBEROS}/include
#KLIBPATH=	-L${KERBEROS}/lib
#KLIBS=	-lkrb -ldes
#KDEFS=	-DKRB
#KSRC=	binhex.c
#KOBJ=	binhex.o

INCPATH=	-I../libnet ${KINCPATH}
DEFS=	-DLOG_TPPD=LOG_LOCAL6 ${KDEFS}
CFLAGS=	${DEFS} ${OPTOPTS} ${INCPATH}
TAGSFILE=	tags
LIBPATH=	-L../libnet ${KLIBPATH}
LIBS=	${ADDLIBS} -lnet ${KLIBS}
CC=	cc
INSTALL=	install

all : tppd tp

tp : ${COBJ} Makefile
	${CC} ${CFLAGS} ${LDFLAGS} -o tp ${COBJ} ${LIBPATH} ${LIBS}

tppd : ${DOBJ} Makefile
	${CC} ${CFLAGS} ${LDFLAGS} -o tppd ${DOBJ} ${LIBPATH} ${LIBS}

install : all
	${INSTALL} -o root -g tpp -m 4750 -c tppd ${DESTDIR}/etc
	${INSTALL} -c tp ${DESTDIR}/bin

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
