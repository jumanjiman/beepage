/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/param.h>
#include <sys/file.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdio.h>

#include <net.h>

#include "queue.h"
#include "config.h"
#include "modem.h"
#include "path.h"

    struct queue *
queue_init( sender )
    char		*sender;
{
    struct queue	*q;

    /* XXX check deny list */

    if (( q = (struct queue *)malloc( sizeof( struct queue ))) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	return( NULL );
    }

    if (( q->q_sender = (char *)malloc( strlen( sender ) + 1 )) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	free( q );
	return( NULL );
    }
    strcpy( q->q_sender, sender );

    q->q_users = NULL;
    /* XXX more inits */

    return( q );
}

queue_recipient( q, user )
    struct queue	*q;
    char		*user;
{
    struct usrdb	*u;
    struct quser	*qu;

    /* XXX check deny list */

    if (( u = usrdb_find( user )) == NULL ) {
	return( -1 );
    }

    if (( qu = (struct quser *)malloc( sizeof( struct quser ))) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	return( -1 );
    }

    qu->qu_user = u;
    qu->qu_next = q->q_users;
    q->q_users = qu;

    return( 0 );
}

/* get sequence number from ".seq" file */
queue_seq( service )
    char		*service;
{
    char		*p, buf[ MAXPATHLEN ];
    int			fd, len;
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
    if ( lseek( fd, 0L, SEEK_SET ) < 0 ) {
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
queue_create( q )
    struct queue	*q;
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

	/* XXX flags? */

	fprintf( qu->qu_fp, "F%s\nT%s\nM\n%s:", q->q_sender,
		qu->qu_user->u_name, q->q_sender );
    }

    return( 0 );
}

queue_line( q, line )
    struct queue	*q;
    char		*line;
{
    struct quser	*qu;

    for ( qu = q->q_users; qu != NULL; qu = qu->qu_next ) {
	fprintf( qu->qu_fp, "%s\n", line );
    }
    return( 0 );
}

/* remove queue files */
queue_cleanup( q )
    struct queue	*q;
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
queue_done( q )
    struct queue	*q;
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
	syslog( LOG_INFO, "page from %s to %s queued", q->q_sender,
		qu->qu_user->u_name );
    }

    return( 0 );
}

queue_check()
{
    struct modem	*modem;
    struct srvdb	*first, *s;
    DIR			*dirp;
    struct dirent	*dp;
    struct usrdb	*u;
    char		from[ 256 ], to[ 256 ], mess[ 256 ];
    int			pid, cnt;

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

			    if ( queue_read( dp->d_name, from, to, mess ) < 0 ){
				syslog( LOG_ERR, "queue_read: %s failed",
					dp->d_name );
				exit( 1 );
			    }
			    if (( u = usrdb_find( to )) == NULL ) {
				syslog( LOG_ERR, "usrdb_find: %s failed", to );
				exit( 1 );
			    }

			    if ( modem_send( modem, u->u_pin, mess ) < 0 ) {
				syslog( LOG_ERR, "modem_send: failed" );
				exit( 1 );
			    }
			    syslog( LOG_INFO, "page from %s to %s sent",
				    from, to );
			    if ( unlink( dp->d_name ) < 0 ) {
				syslog( LOG_ERR, "unlink: %s: %m", dp->d_name );
				exit( 1 );
			    }
			    if ( u->u_flags & U_SENDMAIL &&
				    sendmail( from, to, mess ) < 0 ) {
				syslog( LOG_ERR, "sendmail: failed" );
			    }
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

queue_read( file, from, to, message )
    char		*file;
    char		*from;
    char		*to;
    char		*message;
{
    FILE		*fp;
    char		buf[ 256 ], *p;
    int			len;

    if (( fp = fopen( file, "r" )) == NULL ) {
	syslog( LOG_ERR, "fopen: %s: %m", file );
	return( -1 );
    }

    if ( fgets( buf, sizeof( buf ), fp ) == NULL ) {
	syslog( LOG_ERR, "fgets: %m" );
	return( -1 );
    }
    len = strlen( buf );
    if ( *buf != 'F' || *( buf + len - 1 ) != '\n' ) {
	syslog( LOG_ERR, "error in queue file!" );
	return( -1 );
    }
    *( buf + len - 1 ) = '\0';
    p = buf + 1;
    strcpy( from, p );

    if ( fgets( buf, sizeof( buf ), fp ) == NULL ) {
	syslog( LOG_ERR, "fgets: %m" );
	return( -1 );
    }
    len = strlen( buf );
    if ( *buf != 'T' || *( buf + len - 1 ) != '\n' ) {
	syslog( LOG_ERR, "error in queue file!" );
	return( -1 );
    }
    *( buf + len - 1 ) = '\0';
    p = buf + 1;
    strcpy( to, p );

    if ( fgets( buf, sizeof( buf ), fp ) == NULL ) {
	syslog( LOG_ERR, "fgets: %m" );
	return( -1 );
    }
    if ( strcmp( buf, "M\n" ) != 0 ) {
	syslog( LOG_ERR, "error in queue file!" );
	return( -1 );
    }

    p = buf;
    while ( fgets( p, sizeof( buf ) - ( p - buf ), fp ) != NULL ) {
	p += strlen( p );
    }
    *( buf + strlen( buf ) - 1 ) = '\0'; /*trailing NL*/
    strcpy( message, buf );

    fclose( fp );
    return( 0 );
}
