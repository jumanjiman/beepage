/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "path.h"
#include "sendmail.h"

extern char		*maildomain;

char			*sendmailargv[] = { "sendmail", "-fbeepaged", "-odb", 0, 0 };

    int
sendmail( to, from, subject, file )
    char		*to;
    char		*from;
    char		*subject;
    char		*file;
{
    char		tobuf[ 256 ], frombuf[ 256 ], mbuf[ 8192 ];
    char		*p, *end, *mailstart;
    FILE		*fp;
    int			fd[ 2 ], c, qfd;
    unsigned int	readlen;

    if ( pipe( fd ) < 0 ) {
	syslog( LOG_ERR, "sendmail: pipe: %m" );
	return( -1 );
    }

    if ( maildomain != NULL ) {
	if ( strchr( to, '@' ) != NULL ) {
	    sprintf( tobuf, "%s", to );
	} else {
	    sprintf( tobuf, "%s@%s", to, maildomain );
	}
	sprintf( frombuf, "%s@%s", from, maildomain );
    } else {
	sprintf( tobuf, "%s", to );
	sprintf( frombuf, "%s", from );
    }

    switch ( c = fork()) {
    case -1 :
	syslog( LOG_ERR, "sendmail: fork: %m" );
	close( fd[ 0 ] );
	close( fd[ 1 ] );
	return( -1 );

    case 0 :
	if ( close( fd[ 1 ] ) < 0 ) {
	    syslog( LOG_ERR, "sendmail: close: %m" );
	    return( -1 );
	}
	if ( dup2( fd[ 0 ], 0 ) < 0 ) {
	    syslog( LOG_ERR, "sendmail: dup2: %m" );
	    return( -1 );
	}
	if ( close( fd[ 0 ] ) < 0 ) {
	    syslog( LOG_ERR, "sendmail: close: %m" );
	    return( -1 );
	}
	sendmailargv[ 3 ] = tobuf;
	execv( _PATH_SENDMAIL, sendmailargv );
	exit( 1 );

    default :
	if ( close( fd[ 0 ] ) < 0 ) {
	    syslog( LOG_ERR, "sendmail: close: %m" );
	    return( -1 );
	}

	if (( fp = fdopen( fd[ 1 ], "w" )) == NULL ) {
	    syslog( LOG_ERR, "sendmail: fdopen: %m" );
	    return( -1 );
	}

	if (( qfd = open( file, O_RDONLY, 0 )) < 0 ) {
	    perror( "queue file" );
	    fclose( fp );
	    return( -1 );
	}

	if ( ( readlen = read( qfd, mbuf, sizeof( mbuf ) ) ) < 0 ) {
	    perror( "read" );
	    fclose( fp );
	    close( qfd );
	    return( -1 );
	}

	mailstart = NULL;
	/* Assume the Message will start within 8K */
	for ( p = mbuf, end = mbuf + readlen; p+2 != end; p++ ) {
	    if ( ( *p == '\n' ) && ( *(p+1) == 'M' ) && ( *(p+2) == '\n' ) ) {
		mailstart = p + 3;
	        break;
	    }
	}

	if ( mailstart == NULL ) {
	    syslog( LOG_ERR, "Queue file is defunct!" );
	    fclose( fp );
	    close( qfd );
	    return( -1 );
	}

	readlen = readlen - ( mailstart - mbuf );
	do {
	    if ( write( fd[ 1 ], mailstart, readlen )  < 0  ) {
		perror( "write" );
		fclose( fp );
		close( qfd );
		return( -1 );
	    }
	    mailstart = mbuf;
	} while ( ( readlen = read( qfd, mbuf, sizeof( mbuf ) ) ) != 0 );

	fclose( fp );
	close( qfd );
	syslog( LOG_INFO, "sendmail confirmation to %s", tobuf );
    }

    return( 0 );
}
