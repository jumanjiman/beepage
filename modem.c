/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <syslog.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <snet.h>

#include "config.h"
#include "modem.h"

struct modem		*modems = NULL;

char			*connectstr = "CONNECT";
char			*banner = "ID=";

char			*tap_cksum ___P(( char * ));

    int
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

    void
modem_checkin( pid )
    pid_t	pid;
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

    void
modem_checkout( modem, pid )
    struct modem	*modem;
    pid_t		pid;
{
    modem->m_pid = pid;
    return;
}

    int
modem_disconnect( modem )
    struct modem	*modem;
{
    struct timeval	tv;
    char		*resp;
    int			i;

    if ( snet_writef( modem->m_net, "\r" ) < 0 ) {
	syslog( LOG_ERR, "snet_writef: %m" );
	return( -1 );
    }
    for (;;) {
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: %m" );
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

    for ( i = 0; i < 3; i++ ) {
	/* LLL */ syslog( LOG_DEBUG, ">>> +++" );
	if ( snet_writef( modem->m_net, "+++" ) < 0 ) {
	    syslog( LOG_ERR, "snet_writef: %m" );
	    return( -1 );
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: reset: %m" );
	    continue;
	}
	/* LLL */ syslog( LOG_DEBUG, "<<< %s", resp );
	if ( strcmp( resp, "OK" ) == 0 ) {
	    break;
	}
    }

    /* LLL */ syslog( LOG_DEBUG, ">>> ATH" );
    if ( snet_writef( modem->m_net, "ATH\r" ) < 0 ) {
	syslog( LOG_ERR, "snet_writef: %m" );
	return( -1 );
    }

    for ( i = 0; i < 3; i++ ) {
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: hangup: %m" );
	    return( -1 );
	}
	/* LLL */ syslog( LOG_DEBUG, "<<< %s", resp );
	if ( strcmp( resp, "OK" ) == 0 ) {
	    break;
	}
    }
    if ( i == 3 ) {
	syslog( LOG_ERR, "modem hangup failed" );
    }

    if ( snet_close( modem->m_net ) < 0 ) {
	syslog( LOG_ERR, "snet_close: %m" );
	return( -1 );
    }
    return( 0 );
}

    int
modem_connect( modem, service )
    struct modem	*modem;
    struct srvdb	*service;
{
    struct termios	tc;
    struct timeval	tv;
    char		*resp, buf[ 1024 ];
    int			fd, i;
    unsigned		len, speed;
    int			cc, flags;

    if (( fd = open( modem->m_path, O_RDWR | O_NONBLOCK, 0 )) < 0 ) {
	syslog( LOG_ERR, "open: %s: %m", modem->m_path );
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

    if (( flags = fcntl( fd, F_GETFL, 0 )) < 0 ) {
	syslog( LOG_ERR, "getfl: %m" );
	return( -1 );
    }
    flags &= ~O_NONBLOCK;
    if ( fcntl( fd, F_SETFL, flags ) < 0 ) {
	syslog( LOG_ERR, "setfl: %m" );
	return( -1 );
    }

    if (( modem->m_net = snet_attach( fd, 1024 * 1024 )) == NULL ) {
	syslog( LOG_ERR, "snet_attach: %m" );
	return( -1 );
    }

    for ( i = 0; i < 2; i++ ) {
	/* LLL */ syslog( LOG_DEBUG, ">>> +++" );
	if ( snet_writef( modem->m_net, "+++" ) < 0 ) {
	    syslog( LOG_ERR, "snet_writef: %m" );
	    return( -1 );
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: reset: %m" );
	    continue;
	}
	/* LLL */ syslog( LOG_DEBUG, "<<< %s", resp );
	if ( strcmp( resp, "OK" ) == 0 ) {
	    break;
	}
    }

    /* LLL */ syslog( LOG_DEBUG, ">>> ATHM0E0" );
    if ( snet_writef( modem->m_net, "ATHM0E0\r" ) < 0 ) {
	syslog( LOG_ERR, "snet_writef: %m" );
	return( -1 );
    }

    for ( i = 0; i < 3; i++ ) {
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: reset: %m" );
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
    if ( snet_writef( modem->m_net, "ATDT%s\r", service->s_phone ) < 0 ) {
	syslog( LOG_ERR, "snet_writef: %m" );
	return( -1 );
    }
    for ( i = 0; i < 3; i++ ) {
	tv.tv_sec = 5 * 60;
	tv.tv_usec = 0;
	while (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: connect: %m" );
	    return( -1 );
	}
	/* LLL */ syslog( LOG_DEBUG, "<<< %s", resp );
	if ( strncmp( resp, connectstr, strlen( connectstr )) == 0 ) {
	    break;
	}
    }
    if ( i == 3 ) {
	syslog( LOG_ERR, "modem failed to connect to %s", service->s_name );
	return( -1 );
    }

    for ( i = 0; i < 3; i++ ) {
	if ( snet_writef( modem->m_net, "\r" ) < 0 ) {
	    syslog( LOG_ERR, "snet_writef: %m" );
	    return( -1 );
	}

	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if (( cc = snet_read( modem->m_net, buf, sizeof( buf ), &tv )) < 0 ) {
	    syslog( LOG_ERR, "snet_read: %m" );
	    continue;
	}
	/* LLL */syslog( LOG_DEBUG, "<<< %.*s", cc, buf );

	len = strlen( banner );
	if (( cc >= len ) && ( strncmp( buf, banner, len ) == 0 )) {
	    break;
	}
    }
    if ( i == 3 ) {
	syslog( LOG_ERR, "tap banner missing from %s", service->s_name );
	return( -1 );
    }

    /* LLL */ syslog( LOG_DEBUG, ">>> PG1" );
    if ( snet_writef( modem->m_net, "PG1\r" ) < 0 ) {
	syslog( LOG_ERR, "snet_writef: %m" );
	return( -1 );
    }
    for (;;) {
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: login: %m" );
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
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: go ahead: %m" );
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

    int
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

    if (( rc = snet_writef( modem->m_net, "%s%s\r", buf,
	    tap_cksum( buf ))) < 0 ) {
	syslog( LOG_ERR, "snet_writef: %m" );
	return( -1 );
    }

    for (;;) {
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if (( resp = snet_getline( modem->m_net, &tv )) == NULL ) {
	    syslog( LOG_ERR, "snet_getline: %m" );
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
