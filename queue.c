/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/param.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <net.h>

#include "compat.h"

#include "queue.h"
#include "config.h"
#include "modem.h"
#include "path.h"
#include "sendmail.h"

struct page {
    char	*p_from;
    char	*p_to;
    int		p_flags;
    char	*p_message;
};

int		queue_seq ___P(( char * ));
struct page	*queue_read ___P(( char * ));
void		queue_free ___P(( struct page * ));

    struct pqueue *
queue_init( sender, flags )
    char		*sender;
    int			flags;
{
    struct pqueue	*q;

    /* XXX check deny list */

    if (( q = (struct pqueue *)malloc( sizeof( struct pqueue ))) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	return( NULL );
    }
    if (( q->q_sender = (char *)malloc( strlen( sender ) + 1 )) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	free( q );
	return( NULL );
    }
    strcpy( q->q_sender, sender );
    q->q_flags = flags;
    q->q_users = NULL;

    return( q );
}

/*
 * Error codes:
 *	0	ok
 *	-1	unknown
 *	1	user unknown
 *	2	kerberos
 */
    int
queue_recipient( q, user )
    struct pqueue	*q;
    char		*user;
{
    struct usrdb	*u;
    struct quser	*qu;

    /* XXX check deny list */

    if (( u = usrdb_find( user )) == NULL ) {
	return( 1 );
    }
    if (( u->u_flags & U_KERBEROS ) && ( q->q_flags & Q_KERBEROS ) == 0 ) {
	return( 2 );
    }

    if (( qu = (struct quser *)malloc( sizeof( struct quser ))) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	return( -1 );
    }

    qu->qu_user = u;
    qu->qu_seq = 0;
    qu->qu_len = TAP_OVERHEAD + strlen( u->u_pin ) +
	    strlen( q->q_sender ) + 1;			/* 1 is the ':' */
    qu->qu_fp = NULL;
    qu->qu_next = q->q_users;
    q->q_users = qu;

    return( 0 );
}

/* get sequence number from ".seq" file */
    int
queue_seq( service )
    char		*service;
{
    char		*p, buf[ MAXPATHLEN ];
    int			fd;
    unsigned		len;
    int			seq;

    sprintf( buf, "%s/.seq", service );
    if (( fd = open( buf, O_RDWR|O_CREAT, 0600 )) < 0 ) {
	return( -1 );
    }
    if ( flock( fd, LOCK_EX ) < 0 ) {
	return( -1 );
    }

    seq = 0;
    if (( len = read( fd, buf, sizeof( buf ))) < 0 ) {
	return( -1 );
    }
    for ( p = buf; len > 0; len--, p++ ) {
	if ( *p < '0' || *p > '9' ) {
	    break;
	}
	seq = seq * 10 + ( *p - '0' );
    }

    seq += 1;
    sprintf( buf, "%d\n", seq );
    if ( lseek( fd, (off_t)0, SEEK_SET ) < 0 ) {
	return( -1 );
    }
    len = strlen( buf );
    if ( write( fd, buf, len ) != len ) {
	return( -1 );
    }
    close( fd );
    return( seq );
}

/* make queue files */
    int
queue_create( q )
    struct pqueue	*q;
{
    struct quser	*qu;
    char		*service;
    char		buf[ MAXPATHLEN ];
    int			fd;

    if ( q->q_users == NULL ) {
	return( -1 );
    }

    for ( qu = q->q_users; qu != NULL; qu = qu->qu_next ) {
	service = qu->qu_user->u_service->s_name;
	if (( qu->qu_seq = queue_seq( service )) < 0 ) {
	    syslog( LOG_ERR, "%s: can't get sequence number: %m", service );
	    return( -1 );
	}
	sprintf( buf, "%s/T%d", service, qu->qu_seq );
	if (( fd = open( buf, O_EXCL|O_WRONLY|O_CREAT, 0600 )) < 0 ) {
	    syslog( LOG_ERR, "%s: %m", buf );
	    return( -1 );
	}
	if (( qu->qu_fp = fdopen( fd, "w" )) == NULL ) {
	    syslog( LOG_ERR, "fdopen: %m" );
	    return( -1 );
	}

	/* XXX need a flag for CONFIRM and QUIET */

	fprintf( qu->qu_fp, "F%s\nT%s\nM\n%s:", q->q_sender,
		qu->qu_user->u_name, q->q_sender );
    }

    return( 0 );
}

    int
queue_line( q, line )
    struct pqueue	*q;
    char		*line;
{
    struct quser	*qu;
    int			rc = 0;

    for ( qu = q->q_users; qu != NULL; qu = qu->qu_next ) {
	fprintf( qu->qu_fp, "%s\n", line );
	/*
	 * Maybe a better strategy would be to accept a page of any length,
	 * and truncate it to an appropriate length on the send.  This way,
	 * the client doesn't have to be careful when sending, and the page
	 * will arrive via email anyway.
	 */
	qu->qu_len += strlen( line ) + 1;
	if ( qu->qu_len > TAP_MAXLEN ) {	/* XXX should be per-user? */
	    rc = -1;
	}
    }
    return( rc );
}

/* remove queue files */
    int
queue_cleanup( q )
    struct pqueue	*q;
{
    struct quser	*qu;
    char		buf[ MAXPATHLEN ];
    char		*service;

    for ( qu = q->q_users; qu != NULL; qu = qu->qu_next ) {
	if ( fclose( qu->qu_fp ) != 0 ) {
	    syslog( LOG_ERR, "fclose: %m" );
	}
	service = qu->qu_user->u_service->s_name;
	sprintf( buf, "%s/T%d", service, qu->qu_seq );
	if ( unlink( buf ) < 0 ) {
	    syslog( LOG_ERR, "unlink %s: %m", buf );
	}
    }

    return( 0 );
}

/* rename queue files to unlocked, valid names */
    int
queue_done( q )
    struct pqueue	*q;
{
    struct quser	*qu;
    char		b1[ MAXPATHLEN ], b2[ MAXPATHLEN ];
    char		*service;

    for ( qu = q->q_users; qu != NULL; qu = qu->qu_next ) {
	if ( fclose( qu->qu_fp ) != 0 ) {
	    syslog( LOG_ERR, "fclose: %m" );
	    return( -1 );
	}
	service = qu->qu_user->u_service->s_name;
	sprintf( b1, "%s/T%d", service, qu->qu_seq );
	sprintf( b2, "%s/P%d", service, qu->qu_seq );
	if ( rename( b1, b2 ) < 0 ) {
	    syslog( LOG_ERR, "rename %s to %s: %m", b1, b2 );
	    return( -1 );
	}
	syslog( LOG_INFO, "queued %s/P%d from %s to %s",
		service, qu->qu_seq, q->q_sender, qu->qu_user->u_name );
    }

    return( 0 );
}

    struct page *
queue_read( file )
    char		*file;
{
    struct page		*page;
    FILE		*fp;
    char		buf[ 256 ], *p;
    int			len;

    if (( page = (struct page *)malloc( sizeof( struct page ))) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	exit( 1 );
    }
    page->p_flags = 0;

    if (( fp = fopen( file, "r" )) == NULL ) {
	syslog( LOG_ERR, "fopen: %s: %m", file );
	return( NULL );
    }

    if ( fgets( buf, sizeof( buf ), fp ) == NULL ) {
	syslog( LOG_ERR, "fgets: %m" );
	return( NULL );
    }
    len = strlen( buf );
    if ( *buf != 'F' || *( buf + len - 1 ) != '\n' ) {
	syslog( LOG_ERR, "error in queue file!" );
	return( NULL );
    }
    *( buf + len - 1 ) = '\0';
    p = buf + 1;

    if (( page->p_from = (char *)malloc( strlen( p ) + 1 )) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	exit( 1 );
    }
    strcpy( page->p_from, p );

    if ( fgets( buf, sizeof( buf ), fp ) == NULL ) {
	syslog( LOG_ERR, "fgets: %m" );
	return( NULL );
    }
    len = strlen( buf );
    if ( *buf != 'T' || *( buf + len - 1 ) != '\n' ) {
	syslog( LOG_ERR, "error in queue file!" );
	return( NULL );
    }
    *( buf + len - 1 ) = '\0';
    p = buf + 1;

    if (( page->p_to = (char *)malloc( strlen( p ) + 1 )) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	exit( 1 );
    }
    strcpy( page->p_to, p );

    if ( fgets( buf, sizeof( buf ), fp ) == NULL ) {
	syslog( LOG_ERR, "fgets: %m" );
	return( NULL );
    }
    if ( strcmp( buf, "M\n" ) != 0 ) {
	syslog( LOG_ERR, "error in queue file!" );
	return( NULL );
    }

    page->p_message = NULL;
    while ( fgets( buf, sizeof( buf ), fp ) != NULL ) {
	if ( page->p_message == NULL ) {
	    if (( page->p_message =
		    (char *)malloc( strlen( buf ) + 1 )) == NULL ) {
		syslog( LOG_ERR, "malloc: %m" );
		exit( 1 );
	    }
	    strcpy( page->p_message, buf );
	} else {
	    len = strlen( page->p_message );
	    if (( page->p_message = (char *)realloc( page->p_message,
		    len + strlen( buf ) + 1 )) == NULL ) {
		syslog( LOG_ERR, "realloc: %m" );
		exit( 1 );
	    }
	    strcpy( page->p_message + len, buf );
	}
    }
    *( page->p_message + strlen( page->p_message ) - 1 ) = '\0'; /*trailing NL*/

    fclose( fp );
    return( page );
}

    void
queue_free( page )
    struct page	*page;
{
    free( page->p_from );
    free( page->p_to );
    free( page->p_message );
    free( page );
    return;
}

    void
queue_check( osachld, osahup )
    struct sigaction	*osachld, *osahup;
{
    struct modem	*modem;
    struct srvdb	*first, *s;
    DIR			*dirp;
    struct dirent	*dp;
    struct usrdb	*u;
    struct page		*page;
    char		*subject;
    int			pid, cnt, rc;

    while (( modem = modem_find()) != NULL ) {
	s = first = srvdb_next();

	do {
	    if ( s->s_pid != 0 ) {
		continue;
	    }

	    /* check for any jobs */
	    if (( dirp = opendir( s->s_name )) == NULL ) {
		syslog( LOG_ERR, "opendir: %s: %m", s->s_name );
		continue;
	    }
	    while (( dp = readdir( dirp )) != NULL ) {
		if ( *dp->d_name == 'P' ) {
		    break;
		}
	    }
	    closedir( dirp );

	    if ( dp != NULL ) {
		switch ( pid = fork()) {
		case 0 :
		    /* reset CHLD and HUP */
		    if ( sigaction( SIGCHLD, osachld, 0 ) < 0 ) {
			syslog( LOG_ERR, "sigaction: %m" );
			exit( 1 );
		    }
		    if ( sigaction( SIGHUP, osahup, 0 ) < 0 ) {
			syslog( LOG_ERR, "sigaction: %m" );
			exit( 1 );
		    }

		    /* read spool and send */
		    if ( chdir( s->s_name ) < 0 ) {
			syslog( LOG_ERR, "chdir: %s: %m", s->s_name );
			exit( 1 );
		    }
		    if ( modem_connect( modem, s ) < 0 ) {
			syslog( LOG_ERR, "modem_connect: %s: %m", s->s_name );
			exit( 1 );
		    }
		    for (;;) {
			cnt = 0;
			if (( dirp = opendir( "." )) == NULL ) {
			    syslog( LOG_ERR, "opendir: %s: %m", s->s_name );
			    exit( 1 );
			}
			while (( dp = readdir( dirp )) != NULL ) {
			    if ( *dp->d_name != 'P' ) {
				continue;
			    }

			    if (( page = queue_read( dp->d_name )) == NULL ) {
				syslog( LOG_ERR, "queue_read: %s failed",
					dp->d_name );
				exit( 1 );
			    }

			    if (( u = usrdb_find( page->p_to )) == NULL ) {
				syslog( LOG_ERR, "usrdb_find: %s failed",
					page->p_to );
				exit( 1 );
			    }

			    switch ( rc = modem_send( modem, u->u_pin,
				    page->p_message, u->u_service->s_maxlen )) {
			    case -1 :	/* retry */
				syslog( LOG_ERR, "retry %s/%s from %s to %s",
					s->s_name, dp->d_name,
					page->p_from, page->p_to );
				exit( 1 );

			    case 0 :	/* OK */
				syslog( LOG_INFO, "sent %s/%s from %s to %s",
					s->s_name, dp->d_name,
					page->p_from, page->p_to );
				subject = "page sent";
				break;

			    default :	/* invalid, dump it */
				syslog( LOG_ERR, "invalid %s/%s from %s to %s",
					s->s_name, dp->d_name,
					page->p_from, page->p_to );
				subject = "page failed";
				break;
			    }
			    if ( unlink( dp->d_name ) < 0 ) {
				syslog( LOG_ERR, "unlink: %s: %m", dp->d_name );
				exit( 1 );
			    }

			    /*
			     * There are two subjects, each of which
			     * may be sent to the originator and receiver.
			     * The subject is selected above, message are
			     * sent to everyone here.
			     */
			    if (( u->u_flags & U_SENDMAIL ) &&
				    sendmail( page->p_to, page->p_from, subject,
				    page->p_message ) < 0 ) {
				syslog( LOG_ERR, "sendmail: failed" );
			    }

#ifdef notdef
			    if ((( page->p_flags & P_CONFIRM ) || ( rc > 0 &&
				    ( page->p_flags & P_QUIET ) == 0 )) &&
				    sendmail( page->p_from, NULL, subject,
				    page->p_message ) < 0 ) {
				syslog( LOG_ERR, "sendmail: failed" );
			    }
#endif notdef

			    queue_free( page );
			    cnt++;
			}
			closedir( dirp );
			if ( cnt == 0 ) {
			    break;
			}
		    }
		    if ( modem_disconnect( modem ) < 0 ) {
			syslog( LOG_ERR, "modem_connect: %s: %m", s->s_name );
			exit( 1 );
		    }
		    exit( 0 );

		case -1 :
		    syslog( LOG_ERR, "fork: %m" );
		    sleep( 10 );
		    return;

		default :
		    syslog( LOG_INFO, "child %d for %s on %s", pid, s->s_name,
			    modem->m_path );
		    modem_checkout( modem, pid );
		    srvdb_checkout( s, pid );
		}
		break;
	    }
	} while (( s = srvdb_next()) != first );

	if ( s == first ) {	/* nothing to do */
	    return;
	}
    }
    return;
}
