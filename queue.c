/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __STDC__
#include <stdarg.h>
#else __STDC__
#include <varargs.h>
#endif __STDC__

#include <net.h>

#include "compat.h"

#include "queue.h"
#include "config.h"
#include "modem.h"
#include "path.h"
#include "sendmail.h"
#include "compress.h"


int		queue_seq ___P(( char * ));
struct page	*queue_read ___P(( char * ));
void		queue_free ___P(( struct page * ));

    int 
queue_count()
{
    struct srvdb	*first, *s;
    DIR			*dirp;
    int			i = 0;
    struct dirent	*dp;

    s = first = srvdb_next();

    do {
    	if ( ( dirp = opendir( s->s_name )) == NULL ) {
	    printf( "opendir: %s:", s->s_name );
	    continue;
	}

	while ( ( dp = readdir( dirp ) ) != NULL ) {
	    if ( *dp->d_name == 'P' ) {
	        i++;
	    }
	}
	closedir( dirp );

    } while ( ( s = srvdb_next() ) != first );

    return( i );
}

    void
#ifdef __STDC__
queue_printf( struct pqueue *pq, char *format, ... ) 
#else __STDC__
queue_printf( pq, format, va_alist )
    struct pqueue *pq;
    char *format;
#endif __STDC__
{
    va_list val;
    struct quser *qu;

#ifdef __STDC__
    va_start( val, format );
#else __STDC__
    va_start( val );
#endif __STDC__

    for ( qu = pq->q_users; qu != NULL; qu = qu->qu_next ) {
        vfprintf( qu->qu_fp, format, val );
    }
    va_end( val );
}


    struct pqueue *
queue_init( sender, flags, net )
    char		*sender;
    int			flags;
    NET			*net;
{
    struct pqueue	*q;
    int			sinlen;

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

    memset( &q->q_sin, 0, sizeof( struct sockaddr_in ));
    sinlen = sizeof( struct sockaddr );
    if ( getpeername( net->nh_fd, (struct sockaddr *)&q->q_sin, &sinlen ) < 0) {
	syslog( LOG_ERR, "getsockname: %m" );
	free( q->q_sender );
	free( q );
	return( NULL );
    }
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
queue_recipient( q, name )
    struct pqueue	*q;
    char		*name;
{
    NET			*net;
    int 		ret;
    char 		*line;
    struct usrdb	*u;
    struct quser	*qu;
    struct grpdb	*g;
    struct grpdb_mem	*gm;

    /* XXX check deny list */
    if (( g = grpdb_find( name )) != NULL ) {
	if ( g->g_flags & G_DONE ) {
            return( 0 );
	}
	g->g_flags |= G_DONE;

	for ( gm = g->g_mbr; gm != NULL; gm = gm->gm_next ) {
	    if ( gm->gm_flags & G_FILENAME ) {
		if (( net = net_open( gm->gm_name, O_RDONLY, 0, 0 )) == NULL ) {
		    syslog( LOG_NOTICE, "%s: %m", gm->gm_name );
		    return( -1 );
		}
		while (( line = net_getline( net, NULL )) != NULL ) {
		    if (( line[ 0 ] == '#' ) || ( line[ 0 ] == '\0' )) {
			continue;
		    }
		    if  (( ret = queue_recipient( q, line )) != 0 ) {
			return( ret );
		    }
		}
		if ( net_close( net ) < 0 ) {
		    syslog( LOG_ERR, "net_close: %m" );
		    return( -1 );
	   	}
	    } else {
		if (( ret = queue_recipient( q, gm->gm_name )) != 0 ) {
		    return( ret );
		}
	    }
	}

    } else {
	if (( u = usrdb_find( name )) == NULL ) {
	    return( 1 );
	}
	if (( u->u_flags & U_KERBEROS ) && ( q->q_flags & Q_KERBEROS ) == 0 ) {
	    return( 2 );
	}
	if ( u->u_flags & U_DONE ) {
	    return( 0 );
	}
	u->u_flags |= U_DONE;

	if (( qu = (struct quser *)malloc( sizeof( struct quser ))) == NULL ) {
	    syslog( LOG_ERR, "malloc: %m" );
	    return( -1 );
	}

	qu->qu_user = u;
	qu->qu_seq = 0;
	qu->qu_len = TAP_OVERHEAD + strlen( u->u_pin ) +
		strlen( q->q_sender ) + 1;		/* 1 is the ':' */
	qu->qu_fp = NULL;
	qu->qu_next = q->q_users;
	q->q_users = qu;
    }
    return( 0 );
}

/* get sequence number from ".seq" file */
    int
queue_seq( service )
    char		*service;
{

    char		*p, buf[ 20 ], tmpfile[ MAXPATHLEN ];
    char		lockfile[ MAXPATHLEN ], seqfile[ MAXPATHLEN ];
    int			fd, lockfd;
    unsigned		len;
    int			seq, pid, i;
    int			rc = 0;

    pid = getpid();

    sprintf( lockfile, "%s/.seq.lock", service );

    sprintf( tmpfile, "%s/.seq.lock.%d", service, pid );
    sprintf( seqfile, "%s/.seq", service );

    if (( lockfd = open( tmpfile, O_WRONLY|O_CREAT|O_EXCL, 0600 )) < 0 ) {
	syslog( LOG_ERR, "queue_seq, open: %m");
	return( -1 );
    }

    for ( i = 0; i < 10; i++ ) {
	if ( ( rc = link( tmpfile, lockfile ) ) == 0 ) {
	    break;
	}
	syslog( LOG_INFO, "queue_seq: link: %m");
	sleep( 1 );
    }

    if ( unlink( tmpfile ) < 0 ) {
	syslog( LOG_WARNING, "queue_seq: unlink: %s: %m", tmpfile );
    }

    if ( rc < 0 ) {
	syslog( LOG_ERR, "queue_seq: cannot get lock after 10 attempts: %m");
	if ( close( lockfd ) < 0 ) {
	    syslog( LOG_ERR, "queue_seq: close: %m" );
	}
	return( -1 );
    }

    if (( fd = open( seqfile, O_RDWR|O_CREAT, 0600 )) < 0 ) {
	syslog( LOG_ERR, "queue_seq, open: %m");
	goto cleanup2;
    }

    seq = 0;
    if (( len = read( fd, buf, sizeof( buf ))) < 0 ) {
	syslog( LOG_ERR, "queue_seq, read: %m");
	goto cleanup3;
    }
 
    if ( close( fd ) < 0 ) {
	syslog( LOG_ERR, "queue_seq: close: %m" );
        goto cleanup2;
    }

    for ( p = buf; len > 0; len--, p++ ) {
	if ( *p < '0' || *p > '9' ) {
	    break;
	}
	seq = seq * 10 + ( *p - '0' );
    }

    seq += 1;

    sprintf( buf, "%d\n", seq );

    len = strlen( buf );
    if ( write( lockfd, buf, len ) != len ) {
	syslog( LOG_ERR, "queue_seq: write: %m" );
	goto cleanup2;
    }

    if ( close( lockfd ) < 0 ) {
	syslog( LOG_ERR, "queue_seq: close: %m" );
        goto cleanup1;
    }

    if ( rename( lockfile, seqfile ) < 0 ) {
	syslog( LOG_ERR, "queue_seq: rename: %m" );
        goto cleanup1;
    }

    return( seq );


cleanup3:
    if ( close( fd ) < 0 ) {
        syslog( LOG_ERR, "queue_seq: close: %m" );
    }

cleanup2:
    if ( close( lockfd ) < 0 ) {
        syslog( LOG_ERR, "queue_seq: close: %m" );
    }

cleanup1:
    if ( unlink( lockfile ) < 0 ) {
	syslog( LOG_ERR, "queue_seq: unlink: %s: %m", lockfile );
    }

    return( -1 );
}

/* make queue files */
    int
queue_create( q )
    struct pqueue	*q;
{
    struct quser	*qu;
    char		*service;
    char		buf[ MAXPATHLEN ], hostname[ MAXHOSTNAMELEN ];
    int			fd;
    time_t		now;
    struct tm		*tm;
    char		timestamp[ 50 ];
    struct hostent	*host;
    extern char		*version;

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

	fprintf( qu->qu_fp, "F%s\nT%s\nM\n", q->q_sender,
		qu->qu_user->u_name );

        if ( gethostname( hostname, MAXHOSTNAMELEN ) < 0 ) {
		syslog( LOG_ERR, "gethostname: %m" );
		strcpy( hostname, "localhost" );
	}

	/* XXX need a flag for CONFIRM and QUIET */

	now = time( NULL );
	tm = localtime( &now );

	strftime( timestamp, 100, "%a, %e %b %Y %R:%S (%Z)", tm );

        if (( host = gethostbyaddr( ( char *)&q->q_sin.sin_addr,
	    sizeof( struct in_addr ), AF_INET )) == NULL ) {
	    syslog( LOG_ERR, "gethostbyaddr failed for: %s", 
		inet_ntoa( q->q_sin.sin_addr ) );
	}

	fprintf( qu->qu_fp, "Received: from %s (%s)\n",
		( ( host != NULL ) ? host->h_name : "unknown" ),
	    	inet_ntoa( q->q_sin.sin_addr ) );

	fprintf( qu->qu_fp, "\tby %s (%s) with TPP\n", hostname, version );
	
	fprintf( qu->qu_fp, "\tid %s/%d for %s; %s\n",
		(usrdb_find(qu->qu_user->u_name))->u_service->s_name,
		qu->qu_seq, qu->qu_user->u_name, timestamp );
    }
    return( 0 );
}

    void
queue_line( q, line )
    struct pqueue	*q;
    char		*line;
{
    struct quser	*qu;

    for ( qu = q->q_users; qu != NULL; qu = qu->qu_next ) {
	fprintf( qu->qu_fp, "%s\n", line );
    }
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

    usrdb_clear();

    return( 0 );
}

    struct page *
queue_read( file )
    char		*file;
{
    struct page		*page;
    char		*a, *b, *line, *at, *from, *subj;
    NET			*net;
    int 		offset, state, once;
    int 		mime = 0;

    if (( page = (struct page *)malloc( sizeof( struct page ))) == NULL ) {
	syslog( LOG_ERR, "malloc: %m" );
	exit( 1 );
    }
    page->p_flags = 0;

    if (( net = net_open( file, O_RDONLY, 0, 0 )) == NULL ) {
        perror( "net_open" );
	return( NULL );
    }

    if ( ( line = net_getline( net, NULL )) != NULL ) {
        if ( *line != 'F' ) {
	    syslog( LOG_ERR, "error in queue file!" );
	    return( NULL );
	}
	line++;
	if (( page->p_from = (char *)malloc( strlen( line ) + 1 )) == NULL ) {
	    syslog( LOG_ERR, "malloc: %m" );
	    exit( 1 );
	}
	strcpy( page->p_from, line );
    } else {
	return( NULL );
    }
    if ( ( line = net_getline( net, NULL )) != NULL ) {
        if ( *line != 'T' ) {
	    syslog( LOG_ERR, "error in queue file!" );
	    return( NULL );
	}
	line++;
	if (( page->p_to = (char *)malloc( strlen( line ) + 1 )) == NULL ) {
	    syslog( LOG_ERR, "malloc: %m" );
	    exit( 1 );
	}
	strcpy( page->p_to, line );
    } else {
	return( NULL );
    }

    if ( ( line = net_getline( net, NULL )) != NULL ) {
        if ( *line != 'M' ) {
	    syslog( LOG_ERR, "error in queue file!" );
	    return( NULL );
	}
    }

    from = subj = NULL;

    while ( ( line = net_getline( net, NULL )) != NULL ) {
/*
 * check if content type flag and if first char is a whitespace
 * if so, pass this line into the function that will get content type
 * info
 */
        if ( *line == '\0' ) {
	    break;
	}
	if ( strncasecmp( "from:", line, 5 ) == 0 ) {
	    /* cut before < */
	    if ( ( at = strchr( line, '<' ) ) != NULL ) {
	        line = at + 1;
		/* cut off @... */
		if ( ( at = strchr( line, '@' ) ) != NULL ) {
		    *at = '\0';
		}
		if ( ( from = (char *)malloc( strlen( line ) + 1 ) ) == NULL ) {
		    syslog( LOG_ERR, "malloc: %m" );
		    exit( 1 );
		}
	    } else {
		/* cut off @... */
		if ( ( at = strchr( line, '@' ) ) != NULL ) {
		    *at = '\0';
		}
		/* sizeof the line - "from: " + null char */
		if ( ( from = (char *)malloc( strlen( line ) - 5 ) ) == NULL ) {
		    syslog( LOG_ERR, "malloc: %m" );
		    exit( 1 );
		}
		line += 6;
	    }
	    sprintf( from, "%s", line );
	}
	if ( strncasecmp( "Subject:", line, 8 ) == 0 ) {
	    if ( ( strcasecmp( line, "Subject: page sent" ) != 0 ) && 
		     ( strcmp( line, "Subject:" ) != 0 ) ) {
		/* sizeof the line - "subject: " + null char */
		if ( ( subj = (char *)malloc( strlen( line ) - 8 ) ) == NULL ) {
		    syslog( LOG_ERR, "malloc: %m" );
		    exit( 1 );
		}
		line += 9;
		sprintf( subj, "%s", line );
	    }
	}
        if ( strncmp( "MIME-Version:", line, 13 ) == 0 ) {
	    mime = 1;
	    continue;
	}
	if ( strncmp( "Content-Type:", line, 13 ) == 0 ) {
/* flag that I got a content type header, 
 * pass the line I just read into a function 
 * that will collect the important mime info
 */
	    if (!mime) {
		/*malformatted mail*/
	    }
	    if ( ( a = strchr( line, '/' ) ) == NULL ) {
		/*mailformatted mail*/
	    } 
	    *a = '\0';
	    b = line;
	    for ( b += 13 ; isspace( *b ); b++ ) {
	    }
	    type = strdup( b );
	    *a++ = '/';
	    if ( ( b = strchr( line, ';' ) ) == NULL ) {
		/*mailformatted mail*/
		/*probably*/
	    } 
	    *b = '\0';
	    subtype = strdup( a );
	    
	}
    }
    offset = state = 0;
    if ( subj != NULL ) {
        page_compress( page, &offset, &state, from, TAP_MAXLEN );
        page_compress( page, &offset, &state, ":", TAP_MAXLEN );

        page_compress( page, &offset, &state, subj, TAP_MAXLEN );
        page_compress( page, &offset, &state, ":", TAP_MAXLEN );

	free( subj );
    } else {
        page_compress( page, &offset, &state, from, TAP_MAXLEN );
        page_compress( page, &offset, &state, ": ", TAP_MAXLEN );
    }
    free( from );
    once = 0;
    while ( ( line = net_getline( net, NULL )) != NULL ) {
        if ( once != 0 ) {
	    if ( page_compress( page, &offset, &state, " ", TAP_MAXLEN ) < 0 ) {
		break;
	    }
	} else {
	    once = 1;
	}
	if ( page_compress( page, &offset, &state, line, TAP_MAXLEN ) < 0 ) {
	    break;
	}
    }
        
    net_close( net );
    return( page );
}

    void
queue_free( page )
    struct page	*page;
{
    free( page->p_from );
    free( page->p_to );
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
    struct stat		qf_buf;
    time_t		now;
    int			deltime, hour, min, sec;


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
#ifndef NOMODEM
		    if ( modem_connect( modem, s ) < 0 ) {
			syslog( LOG_ERR, "modem_connect: %s: %m", s->s_name );
			exit( 1 );
		    }
#endif NOMODEM
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

#ifndef NOMODEM
			    switch ( rc = modem_send( modem, u->u_pin,
				    page->p_message, u->u_service->s_maxlen )) {
			    case -1 :	/* retry */
				syslog( LOG_ERR, "retry %s/%s from %s to %s",
					s->s_name, dp->d_name,
					page->p_from, page->p_to );
				exit( 1 );

			    case 0 :	/* OK */
				now = time( NULL );

				if ( lstat( dp->d_name, &qf_buf ) < 0 ) {
				    perror( "lstat" );
				    /*syslog?*/
				    hour = min = sec = 0;
				} else {
				    deltime = now - qf_buf.st_mtime;
				    hour = ( deltime / 3600 );
				    min = ( ( deltime % 3600 ) / 60 );
				    sec = ( deltime % 60 );
				}    
				syslog( LOG_INFO, 
				 "sent %s/%s from %s to %s in %02d:%02d:%02d",
				    s->s_name, dp->d_name,
				    page->p_from, page->p_to, hour, min, sec );
				subject = "page sent";
				break;

			    default :	/* invalid, dump it */
				syslog( LOG_ERR, "invalid %s/%s from %s to %s",
					s->s_name, dp->d_name,
					page->p_from, page->p_to );
				subject = "page failed";
				break;
			    }
#endif NOMODEM

#ifdef NOMODEM
			    printf( "%s\n", page->p_message );
#endif NOMODEM

			    /*
			     * There are two subjects, each of which
			     * may be sent to the originator and receiver.
			     * The subject is selected above, message are
			     * sent to everyone here.
			     *
			     * If the user has a different email addr in
			     * the users file, tell sendmail() here
			     */

			    if ( u->u_flags & U_SENDMAIL ) {
				if ( sendmail((( u->u_email != NULL ) ?
					u->u_email : page->p_to ),
					page->p_from, subject,
					 dp->d_name ) < 0 ) {
				    syslog( LOG_ERR, "sendmail: failed" );
				}
			    }
			    if ( unlink( dp->d_name ) < 0 ) {
				syslog( LOG_ERR, "unlink: %s: %m", dp->d_name );
				exit( 1 );
			    }

			    queue_free( page );
			    cnt++;
			}
			closedir( dirp );
			if ( cnt == 0 ) {
			    break;
			}
		    }
#ifndef NOMODEM
		    if ( modem_disconnect( modem ) < 0 ) {
			syslog( LOG_ERR, "modem_disconnect: %s: %m",
				s->s_name );
			exit( 1 );
		    }
#endif NOMODEM
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
