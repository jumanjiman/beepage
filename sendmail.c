/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "path.h"
#include "sendmail.h"

extern char		*maildomain;

char			*sendmailargv[] = { "sendmail", "-ftppd", "-odb", 0, 0 };

    int
sendmail( to, from, subject, message )
    char		*to;
    char		*from;
    char		*subject;
    char		*message;
{
    char		tobuf[ 256 ], frombuf[ 256 ];
    FILE		*fp;
    int			fd[ 2 ], c;

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
	fprintf( fp, "To: %s\n", tobuf );
	fprintf( fp, "From: %s\n", frombuf );
	fprintf( fp, "Subject: %s\n\n", subject );
	fprintf( fp, "%s\n", message );
	fclose( fp );
	syslog( LOG_INFO, "sendmail confirmation to %s", tobuf );
    }

    return( 0 );
}
