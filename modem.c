/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <termios.h>
#include <syslog.h>
#include <stdio.h>
#include <fcntl.h>

#include <net.h>

#include "modem.h"
#include "config.h"

struct modem		*modems = NULL;

char			*connect = "CONNECT";
char			*banner = "ID=";

modem_add( path )
    char		*path;
{
    struct modem	*m;

    if (( m = (struct modem *)malloc( sizeof( struct modem ))) == NULL ) {
	return( -1 );
    }

    if (( m->m_path = (char *)malloc( strlen( path ) + 1 )) == NULL ) {
	return( -1 );
    }
    strcpy( m->m_path, path );
    m->m_pid = 0;
    m->m_net = NULL;

    m->m_next = modems;
    modems = m;

    return( 0 );
}

    struct modem *
modem_find()
{
    struct modem	*m;

    for ( m = modems; m != NULL; m = m->m_next ) {
	if ( m->m_pid == 0 ) {
	    break;
	}
    }
    return( m );
}

modem_checkin( pid )
    uid_t	pid;
{
    struct modem	*m;

    for ( m = modems; m != NULL; m = m->m_next ) {
	if ( m->m_pid == pid ) {
	    m->m_pid = 0;
	    break;
	}
    }
    return;
}

modem_checkout( modem, pid )
    struct modem	*modem;
    uid_t		pid;
{
    modem->m_pid = pid;
}

modem_disconnect( modem )
    struct modem	*modem;
{
    struct timeval	tv;
    char		*resp;

    if ( net_writef( modem->m_net, "\r" ) < 0 ) {
	syslog( LOG_ERR, "net_writef: %m" );
	return( -1 );
    }
    for (;;) {
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (( resp = net_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "net_getline: %m" );
	    return( -1 );
	}
	switch ( *resp ) {
	case '' :	/* RS, ok */
	    syslog( LOG_ERR, "tap message unacceptible" );
	    return( -1 );

	case '' :	/* ESC */
	    break;

	default :
	    syslog( LOG_INFO, "<<< %s", resp );
	    continue;
	}
	break;
    }

    if ( net_close( modem->m_net ) < 0 ) {
	syslog( LOG_ERR, "net_close: %m" );
	return( -1 );
    }
    return( 0 );
}

modem_connect( modem, service )
    struct modem	*modem;
    struct srvdb	*service;
{
    struct termios	tc;
    struct timeval	tv;
    char		*resp, buf[ 1024 ];
    int			fd, speed, i;
    int			len, cc;

    if (( fd = open( modem->m_path, O_RDWR, 0 )) < 0 ) {
	syslog( LOG_ERR, "open: %s: %m", modem->m_path );
	return( -1 );
    }
    if ( ioctl( fd, TIOCSSOFTCAR, 0 ) < 0 ) {
	syslog( LOG_ERR, "ioctl: setsoftcarrier: %m" );
	return( -1 );
    }
    if ( tcgetattr( fd, &tc ) < 0 ) {
	syslog( LOG_ERR, "tcgetattr: %m" );
	return( -1 );
    }
    tc.c_iflag = BRKINT | INPCK | ISTRIP | IXON;
    tc.c_oflag = 0;

    speed = cfgetospeed( &tc );
    tc.c_cflag = CS7 | PARENB | CREAD | HUPCL | CRTSCTS;
    cfsetospeed( &tc, speed );
    cfsetispeed( &tc, speed );

    tc.c_lflag = 0;
    for ( i = 0; i < NCCS; i++ ) {
	tc.c_cc[ i ] = 0;
    }
    tc.c_cc[ VMIN ] = 255;
    tc.c_cc[ VTIME ] = 1 * 10; /* one second */

    if ( tcsetattr( fd, TCSANOW, &tc ) < 0 ) {
	syslog( LOG_ERR, "tcsetattr: %m" );
	return( -1 );
    }

    if (( modem->m_net = net_attach( fd )) == NULL ) {
	syslog( LOG_ERR, "net_attach: %m" );
	return( -1 );
    }

    /* LLL */ syslog( LOG_DEBUG, ">>> ATE0&D3" );
    if ( net_writef( modem->m_net, "ATE0&D3\r" ) < 0 ) {
	syslog( LOG_ERR, "net_writef: %m" );
	return( -1 );
    }

    for ( i = 0; i < 3; i++ ) {
	if (( resp = net_getline( modem->m_net, NULL )) == NULL ) {
	    syslog( LOG_ERR, "net_getline: reset: %m" );
	    return( -1 );
	}
	/* LLL */ syslog( LOG_DEBUG, "<<< %s", resp );
	if ( strcmp( resp, "OK" ) == 0 ) {
	    break;
	}
    }
    if ( i == 3 ) {
	syslog( LOG_ERR, "modem reset failed" );
	return( -1 );
    }

    /* LLL */ syslog( LOG_DEBUG, ">>> ATDT%s", service->s_phone );
    if ( net_writef( modem->m_net, "ATDT%s\r", service->s_phone ) < 0 ) {
	syslog( LOG_ERR, "net_writef: %m" );
	return( -1 );
    }
    for ( i = 0; i < 2; i++ ) {
	if (( resp = net_getline( modem->m_net, NULL )) == NULL ) {
	    syslog( LOG_ERR, "net_getline: connect: %m" );
	    return( -1 );
	}
	/* LLL */ syslog( LOG_DEBUG, "<<< %s", resp );
	if ( strncmp( resp, connect, strlen( connect )) == 0 ) {
	    break;
	}
    }
    if ( i == 2 ) {
	syslog( LOG_ERR, "modem failed to connect to %s", service->s_name );
	return( -1 );
    }

    for ( i = 0; i < 3; i++ ) {
	if ( net_writef( modem->m_net, "\r" ) < 0 ) {
	    syslog( LOG_ERR, "net_writef: %m" );
	    return( -1 );
	}

	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if (( cc = net_read( modem->m_net, buf, sizeof( buf ), &tv )) < 0 ) {
	    syslog( LOG_ERR, "net_read: %m" );
	    continue;
	}
	/* LLL */ syslog( LOG_DEBUG, "<<< %.*s", cc, buf );

	len = strlen( banner );
	if ( cc >= len && strncmp( buf, banner, len ) == 0 ) {
	    break;
	}
    }
    if ( i == 3 ) {
	syslog( LOG_ERR, "tap banner missing from %s", service->s_name );
	return( -1 );
    }

    /* LLL */ syslog( LOG_DEBUG, ">>> PG1" );
    if ( net_writef( modem->m_net, "PG1\r" ) < 0 ) {
	syslog( LOG_ERR, "net_writef: %m" );
	return( -1 );
    }
    for (;;) {
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (( resp = net_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "net_getline: login: %m" );
	    return( -1 );
	}
	switch ( *resp ) {
	case '' :	/* ACK, ok */
	    break;

	case '' :	/* NAK, failed */
	case '' :	/* ESC */
	    syslog( LOG_ERR, "tap login failed" );
	    return( -1 );

	default :
	    syslog( LOG_INFO, "<<< %s", resp );
	    continue;
	}
	break;
    }

    for (;;) {
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (( resp = net_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "net_getline: go ahead: %m" );
	    return( -1 );
	}
	switch ( *resp ) {
	case '' :	/* ESC */
	    if ( strcmp( resp, "[p" ) != 0 ) {
		syslog( LOG_ERR, "tap bad go ahead" );
		return( -1 );
	    }
	    break;

	default :
	    syslog( LOG_INFO, "<<< %s", resp );
	    continue;
	}
	break;
    }

    return( 0 );
}

    char *
tap_cksum( s )
    char	*s;
{
    static char	buf[ 4 ];
    char	*p;
    int		i;

    i = 0;
    for ( p = s; *p != '\0'; p++ ) {
	i += *p;
    }

    buf[ 0 ] = '0' + (( i >> 8 ) & 0x0f );
    buf[ 1 ] = '0' + (( i >> 4 ) & 0x0f );
    buf[ 2 ] = '0' + (( i ) & 0x0f );
    buf[ 3 ] = '\0';

    return( buf );
}

modem_send( modem, pin, message, maxlen )
    struct modem	*modem;
    char		*pin;
    char		*message;
    int			maxlen;
{
    struct timeval	tv;
    int			len;
    int			rc;
    char		buf[ TAP_MAXLEN ], *resp;

    len = maxlen - ( strlen( pin ) + TAP_OVERHEAD );

    /* LLL */ syslog( LOG_DEBUG, ">>> %s", pin );
    /* LLL */ syslog( LOG_DEBUG, ">>> %.*s", len, message );
    sprintf( buf, "%s\r%.*s\r", pin, len, message );
    /* LLL */ syslog( LOG_DEBUG, ">>> %s", tap_cksum( buf ));

    if (( rc =
	    net_writef( modem->m_net, "%s%s\r", buf, tap_cksum( buf ))) < 0 ) {
	syslog( LOG_ERR, "net_writef: %m" );
	return( -1 );
    }

    for (;;) {
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (( resp = net_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "net_getline: %m" );
	    return( -1 );
	}
	switch ( *resp ) {
	case '' :	/* ACK, ok */
	    break;

	case '' :	/* NAK, failed */
	    syslog( LOG_ERR, "tap checksum or transmission error" );
	    return( -1 );

	case '' :	/* RS, invalid pager id */
	    syslog( LOG_ERR, "tap page invalid (pin or length)" );
	    return( 1 );

	case '' :	/* ESC */
	    syslog( LOG_ERR, "tap connection closed" );
	    return( -1 );

	default :
	    syslog( LOG_INFO, "<<< %s", resp );
	    continue;
	}
	break;
    }

    return( 0 );
}
