/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <strings.h>
#include <netdb.h>
#include <stdio.h>

#include <net.h>

#ifdef KRB
#ifdef _svr4__
#include <kerberos/krb.h>
#else __svr4__
#include <krb.h>
#endif __svr4__
#endif KRB

char			*host = "tpp";

main( ac, av )
    int			ac;
    char		*av[];
{
    char		*prog, hostname[ 256 ], *line, *from, buf[ 1024 ];
    int			c, err = 0, s, i;
    unsigned short	port = 0;
#ifdef KRB
    KTEXT_ST		auth;
    unsigned long	cksum;
    int			rc;
    char		hexktext[ MAX_KTXT_LEN * 2 + 1 ];
    char		instance[ INST_SZ ], realm[ REALM_SZ ];
#endif KRB
    struct sockaddr_in	sin;
    struct hostent	*hp;
    struct servent	*se;
    NET			*net;
    int			verbose = 0, multiple = 0;
    extern char		*optarg;
    extern int		optind;

    if (( prog = rindex( av[ 0 ], '/' )) == NULL ) {
	prog = av[ 0 ];
    } else {
	prog++;
    }

    while (( c = getopt( ac, av, "vmh:p:" )) != EOF ) {
	switch ( c ) {
	case 'v' :	/* verbose */
	    verbose++;
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
	fprintf( stderr, "Usage:\t%s [ -v ] user message ...\n", prog );
	exit( 1 );
    }

    if ( optind + 1 == ac ) {
	multiple++;
    }

    /* look up the port */
    if ( port == 0 ) {
	if (( se = getservbyname( "tpp", "tcp" )) == NULL ) {
	    fprintf( stderr, "%s: can't find tpp service\n%s: continuing...\n",
		    prog, prog );
	    port = htons( 6661 );
	} else {
	    port = se->s_port;
	}
    }

    if (( hp = gethostbyname( host )) == NULL ) {
	herror( host );
	exit( 1 );
    }
    strcpy( hostname, hp->h_name );

    if (( s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ) {
	perror( "socket" );
	exit( 1 );
    }

    bzero( &sin, sizeof( struct sockaddr_in ));
    sin.sin_family = AF_INET;
    sin.sin_port = port;
    for ( i = 0; hp->h_addr_list[ i ] != NULL; i++ ) {
	/* address in sin, for later */
	bcopy( hp->h_addr_list[ i ], &sin.sin_addr.s_addr, hp->h_length );
	if ( connect( s, &sin, sizeof( struct sockaddr_in )) == 0 ) {
	    break;
	}
	perror( hostname );
    }
    if ( hp->h_addr_list[ i ] == NULL ) {
	exit( 1 );
    }

    if (( net = net_attach( s, 1024 * 1024 )) == NULL ) {
	perror( "net_attach" );
	exit( 1 );
    }

    if (( line = net_getline( net, NULL )) == NULL ) {
	perror( "net_getline" );
	exit( 1 );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	exit( 1 );
    }

#ifdef KRB
    if (( hp = gethostbyaddr( &sin.sin_addr.s_addr,
	    sizeof( sin.sin_addr.s_addr ), AF_INET )) == NULL ) {
	herror( inet_ntoa( sin.sin_addr ));
	exit( 1 );
    }
    cksum = time( 0 ) ^ getpid();
    strcpy( instance, krb_get_phost( hp->h_name ));
    strcpy( realm, krb_realmofhost( hp->h_name ));
    if (( rc = krb_mk_req( &auth, "rcmd", instance, realm, cksum )) ==
	    KSUCCESS ) {
	bin2hex( auth.dat, hexktext, auth.length );
	if ( net_writef( net, "AUTH KRB4 %s\r\n", hexktext ) < 0 ) {
	    perror( "net_writef" );
	    exit( 1 );
	}
	if ( verbose )	printf( ">>> AUTH KRB4 %s\n", hexktext );
    } else {
	fprintf( stderr, "%s: %s\n", prog, krb_err_txt[ rc ] );
	fprintf( stderr, "%s: continuing...\n", prog );
#endif KRB

	if (( from = cuserid( NULL )) == NULL ) {
	    fprintf( stderr, "%s: who are you?\n", prog );
	    exit( 1 );
	}
	if ( net_writef( net, "AUTH NONE %s\r\n", from ) < 0 ) {
	    perror( "net_writef" );
	    exit( 1 );
	}
	if ( verbose )	printf( ">>> AUTH NONE %s\n", from );
#ifdef KRB
    }
#endif KRB

    if (( line = net_getline( net, NULL )) == NULL ) {
	perror( "net_getline" );
	exit( 1 );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	exit( 1 );
    }

    if ( multiple ) {
	for (; optind < ac; optind++ ) {
	    if ( net_writef( net, "PAGE %s\r\n", av[ optind ] ) < 0 ) {
		perror( "net_writef" );
		exit( 1 );
	    }
	    if ( verbose )	printf( ">>> PAGE %s\n", av[ optind ] );

	    if (( line = net_getline( net, NULL )) == NULL ) {
		perror( "net_getline" );
		exit( 1 );
	    }
	    if ( verbose )	printf( "<<< %s\n", line );
	    if ( *line != '2' ) {
		fprintf( stderr, "%s\n", line );
		exit( 1 );
	    }
	}
    } else {
	if ( net_writef( net, "PAGE %s\r\n", av[ optind ] ) < 0 ) {
	    perror( "net_writef" );
	    exit( 1 );
	}
	if ( verbose )	printf( ">>> PAGE %s\n", av[ optind ] );
	optind++;

	if (( line = net_getline( net, NULL )) == NULL ) {
	    perror( "net_getline" );
	    exit( 1 );
	}
	if ( verbose )	printf( "<<< %s\n", line );
	if ( *line != '2' ) {
	    fprintf( stderr, "%s\n", line );
	    exit( 1 );
	}
    }

    if ( net_writef( net, "DATA\r\n" ) < 0 ) {
	perror( "net_writef" );
	exit( 1 );
    }
    if ( verbose )	printf( ">>> DATA\n" );

    if (( line = net_getline( net, NULL )) == NULL ) {
	perror( "net_getline" );
	exit( 1 );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '3' ) {
	fprintf( stderr, "%s\n", line );
	exit( 1 );
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
			if ( net_writef( net, "." ) < 0 ) {
			    perror( "net_writef" );
			    exit( 1 );
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
		if ( net_writef( net, "%s\r\n", buf ) < 0 ) {
		    perror( "net_writef" );
		    exit( 1 );
		}
		if ( verbose )	printf( "%s\n", buf );
	    } else {
		state = ST_TRUNC;
		if ( net_writef( net, "%s", buf ) < 0 ) {
		    perror( "net_writef" );
		    exit( 1 );
		}
		if ( verbose )	printf( "%s", buf );
	    }
	}
	if ( net_writef( net, "\r\n.\r\n" ) < 0 ) {
	    perror( "net_writef" );
	    exit( 1 );
	}
	if ( verbose )	printf( ">>> .\n" );
    } else {
	if ( verbose )	printf( ">>> " );
	if ( *av[ optind ] == '.' ) {
	    if ( net_writef( net, "." ) < 0 ) {
		perror( "net_writef" );
		exit( 1 );
	    }
	    if ( verbose )	printf( "." );
	}
	for (; optind < ac; optind++ ) {
	    if ( optind < ac - 1 ) {
		if ( net_writef( net, "%s ", av[ optind ] ) < 0 ) {
		    perror( "net_writef" );
		    exit( 1 );
		}
		if ( verbose )	printf( "%s ", av[ optind ] );
	    } else {
		if ( net_writef( net, "%s\r\n.\r\n", av[ optind ] ) < 0 ) {
		    perror( "net_writef" );
		    exit( 1 );
		}
		if ( verbose )	printf( "%s\n>>> .\n", av[ optind ] );
	    }
	}
    }

    if (( line = net_getline( net, NULL )) == NULL ) {
	perror( "net_getline" );
	exit( 1 );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	exit( 1 );
    }

    if ( net_writef( net, "QUIT\r\n" ) < 0 ) {
	perror( "net_writef" );
	exit( 1 );
    }
    if ( verbose )	printf( ">>> QUIT\n" );

    if (( line = net_getline( net, NULL )) == NULL ) {
	perror( "net_getline" );
	exit( 1 );
    }
    if ( verbose )	printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	exit( 1 );
    }

    printf( "Page queued on %s\n", hostname );
    exit( 0 );
}
