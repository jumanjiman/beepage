/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <stdio.h>
#include <syslog.h>

#include "path.h"

extern char		*maildomain;

char			*sendmailargv[] = { "sendmail", "-ftppd", "-odb", 0, 0 };

sendmail( to, subject, message )
    char		*to;
    char		*subject;
    char		*message;
{
    char		buf[ 256 ];
    FILE		*fp;
    int			fd[ 2 ], c;

    if ( pipe( fd ) < 0 ) {
	syslog( LOG_ERR, "sendmail: pipe: %m" );
	return( -1 );
    }

    if ( maildomain != NULL ) {
	sprintf( buf, "%s@%s", to, maildomain );
    } else {
	sprintf( buf, "%s", to );
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
	sendmailargv[ 3 ] = buf;
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
	fprintf( fp, "To: %s\n", buf );
	fprintf( fp, "From: Text-Page-Confirmed:;\n" );
	fprintf( fp, "Subject: %s\n\n", subject );
	fprintf( fp, "%s\n", message );
	fclose( fp );
	syslog( LOG_INFO, "sendmail confirmation to %s", buf );
    }

    return( 0 );
}
