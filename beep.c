/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>

#include <snet.h>

#include "binhex.h"

#ifdef KRB
#include <krb.h>
#endif /* KRB */

#ifdef notdef
char			*krb_realmofhost();
char			*krb_get_phost();
#endif /* notdef */

char			*host = "beepage";
char			*version = VERSION;

int			main ___P(( int, char *[] ));

    int
main( ac, av )
    int			ac;
    char		*av[];
{
    struct passwd	*pw;
    char		*prog, hostname[ 256 ], *line, buf[ 1024 ];
    int			c, err = 0, s, i;
    unsigned short	port = 0;
#ifdef KRB
    KTEXT_ST		auth;
    long		cksum;
    int			rc;
    char		hexktext[ MAX_KTXT_LEN * 2 + 1 ];
    char		instance[ INST_SZ ], realm[ REALM_SZ ];
    char		khostname[ 256 ];
#endif /* KRB */
    struct timeval	tv;
    struct sockaddr_in	sin;
    struct hostent	*hp;
    struct servent	*se;
    SNET		*sn;
    int			verbose = 0, multiple = 0, quiet = 0;
    int			nsuccess = 0, nfail = 0;
    extern char		*optarg;
    extern int		optind;

    if (( prog = strrchr( av[ 0 ], '/' )) == NULL ) {
	prog = av[ 0 ];
    } else {
	prog++;
    }

    while (( c = getopt( ac, av, "Vvqmh:p:" )) != EOF ) {
	switch ( c ) {
	case 'V' :	/* virgin */
	    printf( "%s\n", version );
	    exit( EX_OK );
	    break;

	case 'v' :	/* verbose */
	    verbose++;
	    break;

	case 'q' :	/* quiet, bitch! */
	    quiet++;
	    break;

	case 'm' :	/* many/multiple really means stdin... */
	    multiple++;
	    break;

	case 'h' :	/* host */
	    host = optarg;
	    break;

	case 'p' :	/* port */
	    port = htons( atoi( optarg ));
	    break;

	default :
	    err++;
	}
    }
    if ( err || optind == ac ) {
	fprintf( stderr,
		"Usage:\t%s [ -v ] [ -h host ] [ -p port ] user message ...\n",
		prog );
	exit( EX_USAGE );
    }

    if ( optind + 1 == ac ) {
	multiple++;
    }

    /* look up the port */
    if ( port == 0 ) {
	if (( se = getservbyname( "tpp", "tcp" )) == NULL ) {
	    if ( ! quiet ) {
		fprintf( stderr,
		    "%s: can't find tpp service\n%s: continuing...\n",
		    prog, prog );
	    }
	    port = htons( 6661 );
	} else {
	    port = se->s_port;
	}
    }

    if (( hp = gethostbyname( host )) == NULL ) {
	fprintf( stderr, "%s: Can't find address.\n", host );
	exit( EX_TEMPFAIL );
    }
    strcpy( hostname, hp->h_name );

    memset( &sin, 0, sizeof( struct sockaddr_in ));
    sin.sin_family = AF_INET;
    sin.sin_port = port;
    for ( i = 0; hp->h_addr_list[ i ] != NULL; i++ ) {
	if (( s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ) {
	    perror( "socket" );
	    exit( EX_TEMPFAIL );
	}
	/* address in sin, for later */
	memcpy( &sin.sin_addr.s_addr, hp->h_addr_list[ i ],
		(unsigned)hp->h_length );
	if ( connect( s, (struct sockaddr *)&sin,
		sizeof( struct sockaddr_in )) < 0 ) {

	    perror( inet_ntoa( *(struct in_addr *)hp->h_addr_list[ i ] ) );
	    close( s );
	    continue;
	}

	if (( sn = snet_attach( s, 1024 * 1024 )) == NULL ) {
	    perror( "snet_attach" );
	    exit( EX_OSERR );
	}

	tv.tv_sec = 10 * 60;
	tv.tv_usec = 0;
	snet_timeout( sn, SNET_READ_TIMEOUT | SNET_WRITE_TIMEOUT, &tv );

	if (( line = snet_getline( sn, NULL )) == NULL ) {
	    perror( "snet_getline" );
	    exit( EX_IOERR );
	}
	if ( verbose )	printf( "<<< %s\n", line );
	if ( *line != '2' ) {
	    if ( ! quiet )	fprintf( stderr, "%s\n", line );
	    close( s );
	    continue;
	} else {
	    break;
	}
    }

    if ( hp->h_addr_list[ i ] == NULL ) {
	exit( EX_TEMPFAIL );
    }

#ifdef KRB
    if (( hp = gethostbyaddr( (char *)&sin.sin_addr.s_addr,
	    sizeof( sin.sin_addr.s_addr ), AF_INET )) == NULL ) {
	fprintf( stderr, "%s: Can't find name.\n", inet_ntoa( sin.sin_addr ));
	exit( EX_TEMPFAIL );
    }
    strcpy( khostname, hp->h_name );
    cksum = time( 0 ) ^ getpid();
    strcpy( instance, krb_get_phost( khostname ));
    strcpy( realm, krb_realmofhost( khostname ));
    if (( rc = krb_mk_req( &auth, "rcmd", instance, realm, cksum )) !=
	    KSUCCESS ) {
	if ( ! quiet ) {
	    fprintf( stderr, "%s: %s\n%s: continuing...\n",
		    prog, krb_err_txt[ rc ], prog );
	}
    } else {
	bin2hex( auth.dat, hexktext, auth.length );
	if ( snet_writef( sn, "AUTH KRB4 %s\r\n", hexktext ) < 0 ) {
	    perror( "snet_writef" );
	    exit( EX_IOERR );
	}
	if ( verbose )	printf( ">>> AUTH KRB4 %s\n", hexktext );

	if (( line = snet_getline( sn, NULL )) == NULL ) {
	    perror( "snet_getline" );
	    exit( EX_IOERR );
	}
	if ( verbose )	printf( "<<< %s\n", line );
	if ( *line == '2' ) {
	    goto authdone;
	}
	fprintf( stderr, "%s\n", line );
    }
#endif /* KRB */
    if (( pw = getpwuid( getuid())) == NULL ) {
	fprintf( stderr, "%s: who are you?\n", prog );
	exit( EX_CONFIG );
    }
    if ( snet_writef( sn, "AUTH NONE %s\r\n", pw->pw_name ) < 0 ) {
	perror( "snet_writef" );
	exit( EX_IOERR );
    }
    if ( verbose )	printf( ">>> AUTH NONE %s\n", pw->pw_name );

    if (( line = snet_getline( sn, NULL )) == NULL ) {
	perror( "snet_getline" );
	exit( EX_IOERR );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	exit( EX_DATAERR );
    }

#ifdef KRB
/*
 * This let's us skip the "AUTH NONE" method when "AUTH KRB" succeeds.
 */
authdone:
#endif /* KRB */

    if ( multiple ) {
	for (; optind < ac; optind++ ) {
	    if ( snet_writef( sn, "PAGE %s\r\n", av[ optind ] ) < 0 ) {
		perror( "snet_writef" );
		exit( EX_IOERR );
	    }
	    if ( verbose )	printf( ">>> PAGE %s\n", av[ optind ] );

	    if (( line = snet_getline( sn, NULL )) == NULL ) {
		perror( "snet_getline" );
		exit( EX_IOERR );
	    }
	    if ( verbose )	printf( "<<< %s\n", line );
	    if ( *line != '2' ) {
		fprintf( stderr, "%s\n", line );
		nfail++;
	    } else {
		nsuccess++;
	    }
	}
	if ( nsuccess == 0 ) {
	    exit( EX_NOUSER );
	}
    } else {
	if ( snet_writef( sn, "PAGE %s\r\n", av[ optind ] ) < 0 ) {
	    perror( "snet_writef" );
	    exit( EX_IOERR );
	}
	if ( verbose )	printf( ">>> PAGE %s\n", av[ optind ] );
	optind++;

	if (( line = snet_getline( sn, NULL )) == NULL ) {
	    perror( "snet_getline" );
	    exit( EX_IOERR );
	}
	if ( verbose )	printf( "<<< %s\n", line );
	if ( *line != '2' ) {
	    fprintf( stderr, "%s\n", line );
	    exit( EX_NOUSER );
	}
    }

    if ( snet_writef( sn, "DATA\r\n" ) < 0 ) {
	perror( "snet_writef" );
	exit( EX_IOERR );
    }
    if ( verbose )	printf( ">>> DATA\n" );

    if (( line = snet_getline( sn, NULL )) == NULL ) {
	perror( "snet_getline" );
	exit( EX_IOERR );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '3' ) {
	fprintf( stderr, "%s\n", line );
	exit( EX_TEMPFAIL );
    }

    if ( multiple ) {
#define ST_BEGIN	0
#define ST_TRUNC	1
	int		state = ST_BEGIN;

	while ( fgets( buf, sizeof( buf ), stdin ) != NULL ) {
	    if ( state == ST_BEGIN ) {
		if ( *buf == '.' ) {
		    if ( strcmp( buf, ".\n" ) == 0 ) {
			break;	/* same as EOF */
		    } else {
			if ( snet_writef( sn, "." ) < 0 ) {
			    perror( "snet_writef" );
			    exit( EX_IOERR );
			}
			if ( verbose )	printf( ">>> ." );
		    }
		} else {
		    if ( verbose )	printf( ">>> " );
		}
	    }

	    if ( buf[ strlen( buf ) - 1 ] == '\n' ) {
		state = ST_BEGIN;
		buf[ strlen( buf ) - 1 ] = '\0';
		if ( snet_writef( sn, "%s\r\n", buf ) < 0 ) {
		    perror( "snet_writef" );
		    exit( EX_IOERR );
		}
		if ( verbose )	printf( "%s\n", buf );
	    } else {
		state = ST_TRUNC;
		if ( snet_writef( sn, "%s", buf ) < 0 ) {
		    perror( "snet_writef" );
		    exit( EX_IOERR );
		}
		if ( verbose )	printf( "%s", buf );
	    }
	}
	if ( snet_writef( sn, "\r\n.\r\n" ) < 0 ) {
	    perror( "snet_writef" );
	    exit( EX_IOERR );
	}
	if ( verbose )	printf( ">>> .\n" );
    } else {
	if ( verbose )	printf( ">>> " );
	if ( *av[ optind ] == '.' ) {
	    if ( snet_writef( sn, "." ) < 0 ) {
		perror( "snet_writef" );
		exit( EX_IOERR );
	    }
	    if ( verbose )	printf( "." );
	}
	for (; optind < ac; optind++ ) {
	    if ( optind < ac - 1 ) {
		if ( snet_writef( sn, "%s ", av[ optind ] ) < 0 ) {
		    perror( "snet_writef" );
		    exit( EX_IOERR );
		}
		if ( verbose )	printf( "%s ", av[ optind ] );
	    } else {
		if ( snet_writef( sn, "%s\r\n.\r\n", av[ optind ] ) < 0 ) {
		    perror( "snet_writef" );
		    exit( EX_IOERR );
		}
		if ( verbose )	printf( "%s\n>>> .\n", av[ optind ] );
	    }
	}
    }

    if (( line = snet_getline( sn, NULL )) == NULL ) {
	perror( "snet_getline" );
	exit( EX_IOERR ) ;
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	exit( EX_TEMPFAIL );
    }

    if ( snet_writef( sn, "QUIT\r\n" ) < 0 ) {
	perror( "snet_writef" );
	exit( EX_IOERR );
    }
    if ( verbose )	printf( ">>> QUIT\n" );

    if (( line = snet_getline( sn, NULL )) == NULL ) {
	perror( "snet_getline" );
	exit( EX_IOERR );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	exit( EX_IOERR );
    }

    if ( ! quiet ) {
	printf( "Page queued on %s\n", hostname );
    }
    if ( nfail != 0 ) {
	exit( EX_NOUSER );
    }
    exit( EX_OK );
}
