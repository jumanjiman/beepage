/*
 * Copyright (c) 1995 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/param.h>
#include <sys/time.h>

#include <fcntl.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <pwd.h>
#include <errno.h>

#include <net.h>

#ifdef KRB
#ifdef _svr4__
#else __svr4__
#include <krb.h>
#endif __svr4__
#endif KRB

#include "path.h"
#include "queue.h"

struct queue	*pq = NULL;

struct command {
    char	*c_name;
    int		(*c_func)();
};

f_quit( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    net_writef( net, "%d QUIT OK, closing connection\r\n", 201 );
    exit( 0 );
}

f_noop( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    net_writef( net, "%d NOOP OK\r\n", 202 );
    return( 0 );
}

f_help( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    net_writef( net, "%d What is this, SMTP?\r\n", 203 );
    return( 0 );
}

f_auth( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
#ifdef KRB
    KTEXT_ST	auth;
    AUTH_DAT	ad;
    char	instance[ INST_SZ ];
    int		rc;
#endif KRB

    if ( ac != 3 ) {
	net_writef( net, "%d AUTH syntax error\r\n", 510 );
	return( 1 );
    }
    if ( pq != NULL ) {
	net_writef( net, "%d Only one AUTH allowed\r\n", 511 );
	return( 1 );
    }

#ifdef KRB
    if ( strcasecmp( "KRB4", av[ 1 ] ) == 0 ) {
	if (( auth.length = hex2bin( av[ 2 ], auth.dat )) < 0 ) {
	    net_writef( net, "%d AUTH KRB4 authenticator corrupt\r\n", 413 );
	    return( 1 );
	}
	strcpy( instance, "*" );
	if (( rc = krb_rd_req( &auth, "rcmd", instance, 0, &ad, "" )) !=
		RD_AP_OK ) {
	    net_writef( net, "%d AUTH KRB4 failed: %s\r\n", 412,
		    krb_err_txt[ rc ] );
	    return( 1 );
	}

	if (( pq = queue_init( ad.pname, Q_KERBEROS )) == NULL ) {
	    net_writef( net, "%d User %s not permitted\r\n", 411, ad.pname );
	    return( 1 );
	}
	net_writef( net, "%d AUTH KRB4 as %s succeeds\r\n", 211, ad.pname );
	return( 0 );
    } else
#endif KRB
    if ( strcasecmp( "NONE", av[ 1 ] ) == 0 ) {
	if (( pq = queue_init( av[ 2 ], 0 )) == NULL ) {
	    net_writef( net, "%d User %s not permitted\r\n", 414, av[ 2 ] );
	    return( 1 );
	}
	net_writef( net, "%d AUTH NONE as %s succeeds\r\n", 210, av[ 2 ] );
	return( 0 );
    }

    net_writef( net, "%d AUTH type %s not supported\r\n", 410, av[ 1 ] );
    return( 1 );
}

f_page( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    if ( ac != 2 ) {
	net_writef( net, "%d Must specify a recipient\r\n", 520 );
	return( 1 );
    }
    if ( pq == NULL ) {
	net_writef( net, "%d Must AUTH first\r\n", 521 );
	return( 1 );
    }

    switch ( queue_recipient( pq, av[ 1 ] )) {
    case 1 :
	net_writef( net, "%d User %s unknown\r\n", 421, av[ 1 ] );
	return( 1 );

#ifdef KRB
    case 2 :
	net_writef( net, "%d User %s requires Kerberos authentication\r\n",
		422, av[ 1 ] );
	return( 1 );
#endif KRB

    case 0 :
	net_writef( net, "%d User %s ok\r\n", 220, av[ 1 ] );
	return( 0 );

    default :
	net_writef( net, "%d Can't page %s\r\n", 522, av[ 1 ] );
	return( 1 );
    }
}

f_data( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    char	*line;
    int		lenerr = 0;

    if ( ac != 1 ) {
	net_writef( net, "%d Syntax error\r\n", 530 );
	return( 1 );
    }
    if ( pq == NULL ) {
	net_writef( net, "%d Must AUTH first\r\n", 531 );
	return( 1 );
    }
    if ( queue_create( pq ) < 0 ) {
	net_writef( net, "%d Queue creation error\r\n", 532 );
	return( 1 );
    }

    net_writef( net, "%d Enter page, end with \".\"\r\n", 330 );

    while (( line = net_getline( net, NULL )) != NULL ) {
	if ( *line == '.' ) {
	    if ( strcmp( line, "." ) == 0 ) {
		break;
	    }
	    line++;
	}

	if ( queue_line( pq, line ) < 0 ) {
	    lenerr = 1;
	}
    }
    if ( line == NULL ) {	/* EOF */
	queue_cleanup( pq );
	return( -1 );
    }

    if ( queue_done( pq ) < 0 ) {
	net_writef( net, "%d Can't queue message\r\n", 534 );
	return( 1 );
    }

    if ( lenerr ) {
	net_writef( net, "%d Page is too long, will be truncated\r\n", 231 );
    } else {
	net_writef( net, "%d Page queued\r\n", 230 );
    }
    return( 0 );
}

struct command	commands[] = {
    { "NOOP",		f_noop },
    { "QUIT",		f_quit },
    { "HELP",		f_help },
    { "AUTH",		f_auth },
    { "PAGE",		f_page },	/* set page recipient */
    { "DATA",		f_data },	/* text of the page */
/*    { "CONFIRM",	f_confirm },	/* confirmation address */
/*    { "QUEUE",	f_queue },	/* list paging queue's contents */
};

int		ncommands = sizeof( commands ) / sizeof( commands[ 0 ] );
char		hostname[ MAXHOSTNAMELEN ];

cmdloop( fd )
    int		fd;
{
    NET			*net;
    int			ac, i, mask;
    char		**av, *line;
    struct timeval	tv;

    if (( net = net_attach( fd, 1024 * 1024 )) == NULL ) {
	syslog( LOG_ERR, "net_attach: %m" );
	exit( 1 );
    }

    if ( gethostname( hostname, MAXHOSTNAMELEN ) < 0 ) {
	syslog( LOG_ERR, "gethostname: %m" );
	exit( 1 );
    }
    net_writef( net, "%d TPP 1 %s TAP network interface\r\n", 200, hostname );

    tv.tv_sec = 60 * 10;	/* 10 minutes */
    tv.tv_usec = 60 * 10;
    while (( line = net_getline( net, &tv )) != NULL ) {
	tv.tv_sec = 60 * 10;
	tv.tv_usec = 60 * 10;
	if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argcargv: %m" );
	    return( 1 );
	}

	if ( ac ) {
	    for ( i = 0; i < ncommands; i++ ) {
		if ( strcasecmp( av[ 0 ], commands[ i ].c_name ) == 0 ) {
		    if ( (*(commands[ i ].c_func))( net, ac, av ) < 0 ) {
			return( 1 );
		    }
		    break;
		}
	    }
	    if ( i >= ncommands ) {
		net_writef( net, "%d Command %s unregcognized\r\n", 500,
			av[ 0 ] );
	    }
	} else {
	    net_writef( net, "%d Illegal null command\r\n", 501 );
	}
    }

    return( 0 );
}
