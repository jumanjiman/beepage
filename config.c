/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>

#include <net.h>

#include "config.h"
#include "modem.h"

struct srvdb	*srvdb = NULL;
struct usrdb	*usrdb = NULL;

static struct srvdb	*s_next = NULL;

    struct srvdb *
srvdb_next()
{
    if ( s_next == NULL || s_next->s_next == NULL ) {
	s_next = srvdb;
    } else {
	s_next = s_next->s_next;
    }
    return( s_next );
}

srvdb_free()
{
    struct srvdb	*s, *t;

    s = srvdb;
    while ( s != NULL ) {
	free( s->s_phone );
	free( s->s_name );
	t = s->s_next;
	free( s );
	s = t;
    }

    srvdb = s_next = NULL;
    return( 0 );
}

srvdb_read( path )
    char	*path;
{
    NET			*net;
    struct srvdb	*s;
    char		**av, *line;
    int			ac, lino = 0;

    if ( srvdb != NULL ) {
	srvdb_free();
    }

    if (( net = net_open( path, O_RDONLY, 0 )) == NULL ) {
	return( -1 );
    }

    while (( line = net_getline( net, NULL )) != NULL ) {
	lino++;
	if (( ac = argcargv( line, &av )) < 0 ) {
	    perror( "argcargv" );
	    return( -1 );
	}
	if ( ac == 0 || *av[ 0 ] == '#' ) {
	    continue;
	}

	if ( ac < 2 || ac > 3 ) {
	    fprintf( stderr, "%s: line %d, wrong number of arguments %d\n",
		    path, lino, ac );
	    return( -1 );
	}
	if (( s = (struct srvdb *)malloc( sizeof( struct srvdb ))) == NULL ) {
	    perror( "malloc" );
	    return( -1 );
	}
	if (( s->s_name = (char *)malloc( strlen( av[ 0 ] ) + 1 )) == NULL ) {
	    perror( "malloc" );
	    return( -1 );
	}
	strcpy( s->s_name , av[ 0 ] );
	if (( s->s_phone = (char *)malloc( strlen( av[ 1 ] ) + 1 )) == NULL ) {
	    perror( "malloc" );
	    return( -1 );
	}
	strcpy( s->s_phone , av[ 1 ] );
	s->s_pid = 0;

	if ( ac == 3 ) {
	    s->s_maxlen = atoi( av[ 2 ] );
	} else {
	    s->s_maxlen = TAP_MAXLEN;
	}
	if ( s->s_maxlen > TAP_MAXLEN || s->s_maxlen <= TAP_OVERHEAD ) {
	    fprintf( stderr, "%s: line %d, invalid length %d\n",
		    path, lino, s->s_maxlen );
	    return( -1 );
	}

	s->s_next = srvdb;
	srvdb = s;
    }

    /* check for net_error */
    return( 0 );
}

srvdb_checkin( pid )
    uid_t		pid;
{
    struct srvdb	*s;

    for ( s = srvdb; s != NULL; s = s->s_next ) {
	if ( s->s_pid == pid ) {
	    s->s_pid = 0;
	    break;
	}
    }
    return;
}

srvdb_checkout( s, pid )
    struct srvdb	*s;
    uid_t		pid;
{
    s->s_pid = pid;
    return;
}

    struct srvdb *
srvdb_find( service )
    char	*service;
{
    struct srvdb	*s;

    for ( s = srvdb; s != NULL; s = s->s_next ) {
	if ( strcmp( s->s_name, service ) == 0 ) {
	    break;
	}
    }
    return( s );
}

usrdb_flags( f )
    char	*f;
{
    int		flags = 0;

    for ( ; *f; f++ ) {
	switch ( *f ) {
	case 'K' :
	    flags |= U_KERBEROS;
	    break;

	case 'M' :
	    flags |= U_SENDMAIL;
	    break;

	case '-' :
	    break;

	default :
	    return( -1 );
	}
    }
    return( flags );
}

    struct usrdb *
usrdb_find( name )
    char		*name;
{
    struct usrdb	*u;

    for ( u = usrdb; u != NULL; u = u->u_next ) {
	if ( strcmp( u->u_name, name ) == 0 ) {
	    break;
	}
    }
    return( u );
}

usrdb_free()
{
    struct usrdb	*u, *t;

    u = usrdb;
    while ( u != NULL ) {
	free( u->u_name );
	free( u->u_pin );
	t = u->u_next;
	free( u );
	u = t;
    }

    usrdb = NULL;
    return( 0 );
}

/*
 * File format is
 *	name flags service pin
 */
usrdb_read( path )
    char	*path;
{
    NET			*net;
    struct usrdb	*u;
    struct srvdb	*s;
    char		**av, *line;
    int			ac;
    int			flags, lino = 0;

    if ( usrdb != NULL ) {
	usrdb_free();
    }

    if (( net = net_open( path, O_RDONLY, 0 )) == NULL ) {
	perror( path );
	return( -1 );
    }

    while (( line = net_getline( net, NULL )) != NULL ) {
	lino++;
	if (( ac = argcargv( line, &av )) < 0 ) {
	    perror( "argcargv" );
	    return( -1 );
	}
	if ( ac == 0 || *av[ 0 ] == '#' ) {
	    continue;
	}

	if ( ac != 4 ) {
	    fprintf( stderr, "%s: line %d, wrong number of arguments\n",
		    path, lino );
	    return( -1 );
	}

	if (( flags = usrdb_flags( av[ 1 ] )) < 0 ) {
	    fprintf( stderr, "%s: line %d, illegal flags '%s'\n",
		    path, lino, av[ 1 ] );
	    return( -1 );
	}
	if (( s = srvdb_find( av[ 2 ] )) == NULL ) {
	    fprintf( stderr, "%s: line %d, unknown service '%s'\n",
		    path, lino, av[ 2 ] );
	    return( -1 );
	}

	if (( u = (struct usrdb *)malloc( sizeof( struct usrdb ))) == NULL ) {
	    perror( "malloc" );
	    return( -1 );
	}
	if (( u->u_name = (char *)malloc( strlen( av[ 0 ] ) + 1 )) == NULL ) {
	    perror( "malloc" );
	    return( -1 );
	}
	strcpy( u->u_name, av[ 0 ] );
	if (( u->u_pin = (char *)malloc( strlen( av[ 3 ] ) + 1 )) == NULL ) {
	    perror( "malloc" );
	    return( -1 );
	}
	strcpy( u->u_pin, av[ 3 ] );

	u->u_flags = flags;
	u->u_service = s;

	u->u_next = usrdb;
	usrdb = u;
    }

    /* check for net_error */
    return( 0 );
}
