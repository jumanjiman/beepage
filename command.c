/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <snet.h>

#ifdef KRB
#include <krb.h>
#endif KRB

#include "binhex.h"
#include "queue.h"
#include "command.h"
#include "rfc822.h"
#include "config.h"

struct pqueue	*pq = NULL;

struct command {
    char	*c_name;
    int		(*c_func) ___P(( SNET *, int, char *[] ));
};

int	f_quit ___P(( SNET *, int, char *[] ));
int	f_noop ___P(( SNET *, int, char *[] ));
int	f_help ___P(( SNET *, int, char *[] ));
int	f_auth ___P(( SNET *, int, char *[] ));
int	f_page ___P(( SNET *, int, char *[] ));
int	f_data ___P(( SNET *, int, char *[] ));

    int
f_quit( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d QUIT OK, closing connection\r\n", 201 );
    exit( 0 );
}

    int
f_noop( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d NOOP OK\r\n", 202 );
    return( 0 );
}

    int
f_help( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    snet_writef( sn, "%d What is this, SMTP?\r\n", 203 );
    return( 0 );
}

    int
f_auth( sn, ac, av )
    SNET	*sn;
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
	snet_writef( sn, "%d AUTH syntax error\r\n", 510 );
	return( 1 );
    }
    if ( pq != NULL ) {
	snet_writef( sn, "%d Only one AUTH allowed\r\n", 511 );
	return( 1 );
    }

#ifdef KRB
    if ( strcasecmp( "KRB4", av[ 1 ] ) == 0 ) {
	if (( auth.length = hex2bin( av[ 2 ], auth.dat )) < 0 ) {
	    snet_writef( sn, "%d AUTH KRB4 authenticator corrupt\r\n", 413 );
	    return( 1 );
	}
	strcpy( instance, "*" );
	if (( rc = krb_rd_req( &auth, "rcmd", instance, 0, &ad, "" )) !=
		RD_AP_OK ) {
	    snet_writef( sn, "%d AUTH KRB4 failed: %s\r\n", 412,
		    krb_err_txt[ rc ] );
	    return( 1 );
	}

	if (( pq = queue_init( ad.pname, Q_KERBEROS, sn )) == NULL ) {
	    snet_writef( sn, "%d User %s not permitted\r\n", 411, ad.pname );
	    return( 1 );
	}
	snet_writef( sn, "%d AUTH KRB4 as %s succeeds\r\n", 211, ad.pname );
	return( 0 );
    } else
#endif KRB
    if ( strcasecmp( "NONE", av[ 1 ] ) == 0 ) {
	if (( pq = queue_init( av[ 2 ], 0, sn )) == NULL ) {
	    snet_writef( sn, "%d User %s not permitted\r\n", 414, av[ 2 ] );
	    return( 1 );
	}
	snet_writef( sn, "%d AUTH NONE as %s succeeds\r\n", 210, av[ 2 ] );
	return( 0 );
    }

    snet_writef( sn, "%d AUTH type %s not supported\r\n", 410, av[ 1 ] );
    return( 1 );
}

    int
f_page( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    if ( ac < 2 ) {
	snet_writef( sn, "%d Must specify a recipient\r\n", 520 );
	return( 1 );
    }
    if ( ac > 2 ) {
	snet_writef( sn, "%d Syntax Error\r\n", 523 );
	return( 1 );
    }
    if ( pq == NULL ) {
	snet_writef( sn, "%d Must AUTH first\r\n", 521 );
	return( 1 );
    }

    switch ( queue_recipient( pq, av[ 1 ] )) {
    case 1 :
	snet_writef( sn, "%d User %s unknown\r\n", 421, av[ 1 ] );
	return( 1 );

#ifdef KRB
    case 2 :
	snet_writef( sn, "%d User %s requires Kerberos authentication\r\n",
		422, av[ 1 ] );
	return( 1 );
#endif KRB

    case 0 :
	snet_writef( sn, "%d User %s ok\r\n", 220, av[ 1 ] );
	return( 0 );

    default :
	snet_writef( sn, "%d Can't page %s\r\n", 522, av[ 1 ] );
	return( 1 );
    }
}

    int
f_data( sn, ac, av )
    SNET	*sn;
    int		ac;
    char	*av[];
{
    char		*line;
    int			dot =0;
    struct datalines    *d_head = NULL;
    struct datalines    *d_tail = NULL;
    int                 keyheaders = 0;
    int                 linecount = 0;
    extern char		*maildomain;
    struct quser	*q;

    if ( ac != 1 ) {
	snet_writef( sn, "%d Syntax error\r\n", 530 );
	return( 1 );
    }
    if ( pq == NULL ) {
	snet_writef( sn, "%d Must AUTH first\r\n", 531 );
	return( 1 );
    }
    if ( queue_create( pq ) < 0 ) {
	snet_writef( sn, "%d Queue creation error\r\n", 532 );
	return( 1 );
    }

    snet_writef( sn, "%d Enter page, end with \".\"\r\n", 330 );

    while (( line = snet_getline( sn, NULL )) != NULL ) {
	if ( *line == '.' ) {
	    if ( strcmp( line, "." ) == 0 ) {
	    	dot = 1;
		break;
	    }
	    line++;
	}
        if ( ++linecount <= 200 ) {
	    if ( dl_append( line, &d_head, &d_tail ) < 0 ) {
		snet_writef( sn, "%d Queue Failed\r\n", 533 );
		return( 1 );
	    }
	    if ( *line == '\0' ) {
		dot = 0;
		break;
	    }
	    /* Look at the line I just saw. Is it a header? */
	    if ( parse_header( line, &keyheaders ) < 0 ) {
		keyheaders = 0;
		break;
	    }
	} else {
	/* lets assume this is mail */
	    if ( d_head != NULL ) {
		dl_output( d_head, pq );
		/* queue_line the newline between headers and body */
		queue_line( pq, line );
		dl_free( &d_head );
	    } else {
		queue_line( pq, line );
	    }
	}
    }
    if ( line == NULL ) {       /* EOF */
        queue_cleanup( pq );
        return( -1 );
    }

	    
  
    if ( linecount < 200 ) {

        /* is this REALLY email? check for key headers */
	if ( ( keyheaders & ( IH_TO | IH_FROM | IH_SUBJ ) ) == 0 ) {
	    /* Add a line separating our headers from the message if not */
	    if ( dl_prepend( "", &d_head ) < 0 ) {
		return( -1 );
	    }
	}

	/* Do I have a From:,  To: and Subject: ? */
	if ( ! ( keyheaders & IH_FROM ) ) {
	    if ( maildomain != NULL ) {
		queue_printf( pq, "From: %s@%s\n", pq->q_sender, maildomain );
	    } else {
		queue_printf( pq, "From: %s\n", pq->q_sender );
	    }
	} 

	if ( ! ( keyheaders & IH_TO )  ) {
	    if ( maildomain != NULL ) {
		queue_printf( pq, "To: %s@%s", pq->q_users->qu_user->u_name, 
				    maildomain );
		for ( q = pq->q_users->qu_next; q != NULL; q = q->qu_next ) {
		    queue_printf( pq, ", %s@%s", q->qu_user->u_name, 
					maildomain );
		}
	    } else {
		queue_printf( pq, "To: %s", pq->q_users->qu_user->u_name );
		for ( q = pq->q_users->qu_next; q != NULL; q = q->qu_next ) {
		    queue_printf( pq, ", %s", q->qu_user->u_name );
		}
	    }
	    queue_printf( pq, "\n" );
	}

	if ( ! (keyheaders & IH_SUBJ ) ) {
	    queue_line( pq, "Subject: page sent" );
	}
	    
	dl_output( d_head, pq );
	dl_free( &d_head );
    }

    /* if i'm "not done" getting data yet, queue the rest */
    if ( dot == 0 ) {
        while (( line = snet_getline( sn, NULL )) != NULL ) {
            if ( *line == '.' ) {
                if ( strcmp( line, "." ) == 0 ) {
                    break;
                }
                line++;
            }
            queue_line( pq, line );
        }
	if ( line == NULL ) {       /* EOF */
	    queue_cleanup( pq );
	    return( -1 );
	}
    }

    if ( queue_done( pq ) < 0 ) {
	snet_writef( sn, "%d Can't queue message\r\n", 534 );
	return( 1 );
    }

    snet_writef( sn, "%d Page queued\r\n", 230 );
    return( 0 );
}

struct command	commands[] = {
    { "NOOP",		f_noop },
    { "QUIT",		f_quit },
    { "HELP",		f_help },
    { "AUTH",		f_auth },
    { "PAGE",		f_page },	/* set page recipient */
    { "DATA",		f_data },	/* text of the page */
#ifdef notdef
    { "CONFIRM",	f_confirm },	/* confirmation address */
    { "QUEUE",	f_queue },	/* list paging queue's contents */
#endif notdef
};

int		ncommands = sizeof( commands ) / sizeof( commands[ 0 ] );
char		hostname[ MAXHOSTNAMELEN ];

    int
cmdloop( fd, max_queue_size )
    int		fd, max_queue_size;
{
    SNET		*sn;
    int			ac, i;
    char		**av, *line;
    struct timeval	tv;
    extern char		*version;

    if (( sn = snet_attach( fd, 1024 * 1024 )) == NULL ) {
	syslog( LOG_ERR, "snet_attach: %m" );
	exit( 1 );
    }

    if ( gethostname( hostname, MAXHOSTNAMELEN ) < 0 ) {
	syslog( LOG_ERR, "gethostname: %m" );
	exit( 1 );
    }
    /* Do a queue Check. Are we maxxed out? */
    if ( max_queue_size != 0 ) {
	if ( queue_count() >= max_queue_size) {
	    snet_writef( sn, "%d TPP 1 %s %s TAP network interface queue maxxed\r\n",
		502, hostname, version );
	    syslog( LOG_WARNING, "Queue exceeds maximum queue size" );
	    return( 0 );
	}
    }
    syslog( LOG_INFO, "Queue size: %d pages queued", queue_count() );

    snet_writef( sn, "%d TPP 1 %s %s TAP network interface\r\n", 200,
	    hostname, version );

    tv.tv_sec = 60 * 10;	/* 10 minutes */
    tv.tv_usec = 60 * 10;
    while (( line = snet_getline( sn, &tv )) != NULL ) {
	tv.tv_sec = 60 * 10;
	tv.tv_usec = 60 * 10;
	if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argcargv: %m" );
	    return( 1 );
	}

	if ( ac ) {
	    for ( i = 0; i < ncommands; i++ ) {
		if ( strcasecmp( av[ 0 ], commands[ i ].c_name ) == 0 ) {
		    if ( (*(commands[ i ].c_func))( sn, ac, av ) < 0 ) {
			return( 1 );
		    }
		    break;
		}
	    }
	    if ( i >= ncommands ) {
		snet_writef( sn, "%d Command %s unrecognized\r\n", 500,
			av[ 0 ] );
	    }
	} else {
	    snet_writef( sn, "%d Illegal null command\r\n", 501 );
	}
    }

    return( 0 );
}
