/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>

#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <syslog.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>

#include "path.h"
#include "net.h"
#include "modem.h"

int		debug = 0;
int		backlog = 5;

char		*maildomain = NULL;

    void
hup()
{
    syslog( LOG_INFO, "reload" );

    if ( srvdb_read( _PATH_SRVDB ) < 0 ) {
	syslog( LOG_ERR, "%s: failed", _PATH_SRVDB );
	exit( 1 );
    }
    if ( usrdb_read( _PATH_USRDB ) < 0 ) {
	syslog( LOG_ERR, "%s: failed", _PATH_USRDB );
	exit( 1 );
    }
}

    void
chld()
{
    int			pid, status;
    struct rusage	ru;
    extern int		errno;

    while (( pid = wait3( &status, WNOHANG, &ru )) > 0 ) {

	/* keep track of queue state */
	modem_checkin( pid );
	srvdb_checkin( pid );

	if ( WIFEXITED( status )) {
	    if ( WEXITSTATUS( status )) {
		syslog( LOG_ERR, "child %d exited with %d", pid,
			WEXITSTATUS( status ));
	    } else {
		syslog( LOG_INFO, "child %d done usr %d.%d sys %d.%d", pid,
			ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
			ru.ru_stime.tv_sec, ru.ru_stime.tv_usec );
	    }
	} else if ( WIFSIGNALED( status )) {
	    syslog( LOG_ERR, "child %d died on signal %d", pid,
		    WTERMSIG( status ));
	} else {
	    syslog( LOG_ERR, "child %d died", pid );
	}
    }

    if ( pid < 0 && errno != ECHILD ) {
	syslog( LOG_ERR, "wait3: %m" );
	exit( 1 );
    }
    return;
}

main( ac, av )
    int		ac;
    char	*av[];
{
    struct sockaddr_in	sin;
    struct sigaction	sa;
    struct hostent	*hp;
    struct servent	*se;
    int			c, s, err = 0, fd, sinlen;
    int			dontrun = 0;
    char		*prog;
    unsigned short	port = 0;
    extern int		optind;
    extern char		*optarg;

    if (( prog = rindex( av[ 0 ], '/' )) == NULL ) {
	prog = av[ 0 ];
    } else {
	prog++;
    }

    while (( c = getopt( ac, av, "cdp:b:M:" )) != -1 ) {
	switch ( c ) {
	case 'c' :		/* check config files */
	    dontrun++;
	    break;

	case 'd' :		/* debug */
	    debug++;
	    break;

	case 'p' :		/* TCP port */
	    port = htons( atoi( optarg ));
	    break;

	case 'b' :		/* listen backlog */
	    backlog = atoi( optarg );
	    break;

	case 'M' :
	    maildomain = optarg;
	    break;

	/* spool dir */
	/* config dir or users and services */

	default :
	    err++;
	}
    }

    /* these two routines both report their own errors, on stderr */
    if ( srvdb_read( _PATH_SRVDB ) < 0 ) {
	exit( 1 );
    }
    if ( usrdb_read( _PATH_USRDB ) < 0 ) {
	exit( 1 );
    }

    if ( chdir( _PATH_SPOOL ) < 0 ) {
	perror( _PATH_SPOOL );
	exit( 1 );
    }

    if ( dontrun ) {
	exit( 0 );
    }

    if ( err || optind == ac ) {
	fprintf( stderr,
		"Usage:\t%s [ -d ] [ -p port ] [ -b backlog ] tty ...\n",
		prog );
	exit( 1 );
    }

    for ( ; optind < ac; optind++ ) {
	if ( modem_add( av[ optind ] ) < 0 ) {
	    fprintf( stderr, "%s: bad tty %s\n", prog, av[ optind ] );
	    exit( 1 );
	}
    }

    if ( port == 0 ) {
	if (( se = getservbyname( "tpp", "tcp" )) == NULL ) {
	    fprintf( stderr, "%s: can't find tpp service\n%s: continuing...\n",
		    prog, prog );
	    port = htons( 6661 );
	} else {
	    port = se->s_port;
	}
    }

    /*
     * Set up listener.
     */
    if (( s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ) {
	perror( "socket" );
	exit( 1 );
    }
    bzero( &sin, sizeof( struct sockaddr_in ));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = port;
    if ( bind( s, &sin, sizeof( struct sockaddr_in )) < 0 ) {
	perror( "bind" );
	exit( 1 );
    }
    if ( listen( s, backlog ) < 0 ) {
	perror( "listen" );
	exit( 1 );
    }

    /*
     * Disassociate from controlling tty.
     */
    if ( !debug ) {
	int		i, dt;

	switch ( fork()) {
	case 0 :
	    dt = getdtablesize();
	    for ( i = 0; i < dt; i++ ) {
		if ( i != s ) {			/* keep listen socket open */
		    (void)close( i );
		}
	    }
	    if (( i = open( "/dev/tty", O_RDWR, 0 )) >= 0 ) {
		(void)ioctl( i, TIOCNOTTY, 0 );
		setpgrp( 0, getpid());
		(void)close( i );
	    }
	    if (( i = open( "/", O_RDONLY, 0 )) >= 0 ) {
		dup2( i, 1 );
		dup2( i, 2 );
	    }
	    break;
	case -1 :
	    perror( "fork" );
	    exit( 1 );
	default :
	    exit( 0 );
	}
    }

    /*
     * Start logging.
     */
#ifdef ultrix
    openlog( prog, LOG_NOWAIT|LOG_PID );
#else ultrix
    openlog( prog, LOG_NOWAIT|LOG_PID, LOG_TPPD );
#endif ultrix

    /* catch SIGHUP */
    bzero( &sa, sizeof( struct sigaction ));
    sa.sa_handler = hup;
    if ( sigaction( SIGHUP, &sa, 0 ) < 0 ) {
	syslog( LOG_ERR, "sigaction: %m" );
	exit( 1 );
    }

    /* catch SIGCHLD */
    bzero( &sa, sizeof( struct sigaction ));
    sa.sa_handler = chld;
    if ( sigaction( SIGCHLD, &sa, 0 ) < 0 ) {
	syslog( LOG_ERR, "sigaction: %m" );
	exit( 1 );
    }

    syslog( LOG_INFO, "restart" );

    /*
     * Begin accepting connections.
     */
    for (;;) {
	queue_check();

	sinlen = sizeof( struct sockaddr_in );
	if (( fd = accept( s, &sin, &sinlen )) < 0 ) {
	    if ( errno == EINTR ) {
		continue;
	    }
	    syslog( LOG_ERR, "accept: %m" );
	    exit( 1 );
	}

	/* start child */
	switch ( c = fork()) {
	case 0 :
	    close( s );

	    /* default SIGCHLD handler */
	    bzero( &sa, sizeof( struct sigaction ));
	    sa.sa_handler = SIG_DFL;
	    if ( sigaction( SIGCHLD, &sa, 0 ) < 0 ) {
		syslog( LOG_ERR, "sigaction: %m" );
		exit( 1 );
	    }

	    /* catch SIGHUP */
	    bzero( &sa, sizeof( struct sigaction ));
	    sa.sa_handler = SIG_DFL;
	    if ( sigaction( SIGHUP, &sa, 0 ) < 0 ) {
		syslog( LOG_ERR, "sigaction: %m" );
		exit( 1 );
	    }

	    exit( cmdloop( fd ));

	case -1 :
	    close( fd );
	    syslog( LOG_ERR, "fork: %m" );
	    sleep( 10 );
	    break;

	default :
	    close( fd );
	    syslog( LOG_INFO, "child %d for %s", c, inet_ntoa( sin.sin_addr ));
#ifdef notdef
	    /* why this doesn't work... */
	    if (( hp = gethostbyaddr( &sin.sin_addr, sizeof( struct in_addr ),
		    AF_INET )) == NULL ) {
		syslog( LOG_INFO, "child %d for %s", c,
			inet_ntoa( sin.sin_addr ));
	    } else {
		syslog( LOG_INFO, "child %d for %s %s", c,
			inet_ntoa( sin.sin_addr ), hp->h_name );
	    }
#endif notdef
	    break;
	}
    }
}
