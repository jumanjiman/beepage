/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <net.h>

#include "compat.h"
#include "command.h"
#include "path.h"
#include "config.h"
#include "modem.h"
#include "queue.h"

int		debug = 0;
int		backlog = 5;

char		*maildomain = NULL;
char		*version = VERSION;

void		hup ___P(( int ));
void		chld ___P(( int ));
int		main ___P(( int, char *av[] ));

    void
hup( sig )
    int			sig;
{
    syslog( LOG_INFO, "reload %s", version );

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
chld( sig )
    int			sig;
{
    int			pid, status;
    extern int		errno;

    while (( pid = waitpid( 0, &status, WNOHANG )) > 0 ) {

	/* keep track of queue state */
	modem_checkin( pid );
	srvdb_checkin( pid );

	if ( WIFEXITED( status )) {
	    if ( WEXITSTATUS( status )) {
		syslog( LOG_ERR, "child %d exited with %d", pid,
			WEXITSTATUS( status ));
	    } else {
		syslog( LOG_INFO, "child %d done", pid );
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

    int
main( ac, av )
    int		ac;
    char	*av[];
{
    struct sigaction	sa, osahup, osachld;
    struct sockaddr_in	sin;
    struct hostent	*hp;
    struct servent	*se;
    int			c, s, err = 0, fd, sinlen;
    int			dontrun = 0;
    int			restart = 0;
    int			pidfd;
    FILE		*pf;
    char		*pidfile = _PATH_PIDFILE;
    char		*prog;
    unsigned short	port = 0;
    extern int		optind;
    extern char		*optarg;

    if (( prog = strrchr( av[ 0 ], '/' )) == NULL ) {
	prog = av[ 0 ];
    } else {
	prog++;
    }

    while (( c = getopt( ac, av, "Vrcdp:b:M:" )) != -1 ) {
	switch ( c ) {
	case 'V' :		/* virgin */
	    printf( "%s\n", version );
	    exit( 0 );

	case 'r' :		/* restart server */
	    restart++;
	    break;

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

    if ( restart ) {
	int		pid;

	/* open pid file */
	if (( pidfd = open( pidfile, O_RDONLY, 0 )) < 0 ) {
	    perror( pidfile );
	    exit( 1 );
	}
	/* make sure we *can't* get a lock */
	if ( flock( pidfd, LOCK_EX | LOCK_NB ) == 0 || errno != EWOULDBLOCK ) {
	    fprintf( stderr, "%s: no daemon running!\n", prog );
	    exit( 1 );
	}

	if (( pf = fdopen( pidfd, "r" )) == NULL ) {
	    fprintf( stderr, "%s: can't fdopen pidfd!\n", pidfile );
	    exit( 1 );
	}
	fscanf( pf, "%d\n", &pid );
	if ( pid <= 0 ) {
	    fprintf( stderr, "%s: bad pid %d!\n", pidfile, pid );
	    exit( 1 );
	}
	if ( kill( pid, SIGHUP ) < 0 ) {
	    perror( "kill" );
	    exit( 1 );
	}
	exit( 0 );
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
    memset( &sin, 0, sizeof( struct sockaddr_in ));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = port;
    if ( bind( s, (struct sockaddr *)&sin, sizeof( struct sockaddr_in )) < 0 ) {
	perror( "bind" );
	exit( 1 );
    }
    if ( listen( s, backlog ) < 0 ) {
	perror( "listen" );
	exit( 1 );
    }

    /* open and truncate the pid file */
    if (( pidfd = open( pidfile, O_CREAT | O_WRONLY, 0644 )) < 0 ) {
	perror( pidfile );
	exit( 1 );
    }
    if ( flock( pidfd, LOCK_EX | LOCK_NB ) < 0 ) {
	if ( errno == EWOULDBLOCK ) {
	    fprintf( stderr, "%s: already running!\n", prog );
	} else {
	    perror( "flock" );
	}
	exit( 1 );
    }
    if ( ftruncate( pidfd, (off_t)0 ) < 0 ) {
	perror( "ftruncate" );
	exit( 1 );
    }

    /*
     * Disassociate from controlling tty.
     */
    if ( !debug ) {
	int		i, dt;

	switch ( fork()) {
	case 0 :
	    if ( setsid() < 0 ) {
		perror( "setsid" );
		exit( 1 );
	    }
	    dt = getdtablesize();
	    for ( i = 0; i < dt; i++ ) {
		if ( i != s && i != pidfd ) {	/* keep socket and pidfd open */
		    (void)close( i );
		}
	    }
	    if (( i = open( "/", O_RDONLY, 0 )) != 0 ) {
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

    if (( pf = fdopen( pidfd, "w" )) == NULL ) {
	syslog( LOG_ERR, "can't fdopen pidfd" );
	exit( 1 );
    }
    fprintf( pf, "%d\n", (int)getpid());
    fflush( pf );	/* leave pf open, since it's flock-ed */

    /* catch SIGHUP */
    memset( &sa, 0, sizeof( struct sigaction ));
    sa.sa_handler = hup;
    if ( sigaction( SIGHUP, &sa, &osahup ) < 0 ) {
	syslog( LOG_ERR, "sigaction: %m" );
	exit( 1 );
    }

    /* catch SIGCHLD */
    memset( &sa, 0, sizeof( struct sigaction ));
    sa.sa_handler = chld;
    if ( sigaction( SIGCHLD, &sa, &osachld ) < 0 ) {
	syslog( LOG_ERR, "sigaction: %m" );
	exit( 1 );
    }

    syslog( LOG_INFO, "restart %s", version );

    /*
     * Begin accepting connections.
     */
    for (;;) {
	queue_check( &osachld, &osahup );

	sinlen = sizeof( struct sockaddr_in );
	if (( fd = accept( s, (struct sockaddr *)&sin, &sinlen )) < 0 ) {
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

	    /* reset CHLD and HUP */
	    if ( sigaction( SIGCHLD, &osachld, 0 ) < 0 ) {
		syslog( LOG_ERR, "sigaction: %m" );
		exit( 1 );
	    }
	    if ( sigaction( SIGHUP, &osahup, 0 ) < 0 ) {
		syslog( LOG_ERR, "sigaction: %m" );
		exit( 1 );
	    }

	    /* why this doesn't work... */
	    if (( hp = gethostbyaddr( (void *)&sin.sin_addr,
		    sizeof( struct in_addr ), AF_INET )) == NULL ) {
		syslog( LOG_INFO, "child for %s",
			inet_ntoa( sin.sin_addr ));
	    } else {
		syslog( LOG_INFO, "child for %s %s",
			inet_ntoa( sin.sin_addr ), hp->h_name );
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

	    break;
	}
    }
}
