#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>

#include "rfc2045.h"

/* Content-Type: type/subtype; attribute=value; 
 * returns positive when type, subtype, attibute and value all accounted for.
 * returns zero if only some values are parsed
 * returns negative on error
 */

    int 
parse_content_type( line, n_line, type, subtype, attribute, value, len )
    char 	**line, *n_line, **type, **subtype, **attribute, **value;
    int		*len;
{

    char	*i, *j, *newline;
    char	val_prev = ';';
    int		addlen;

    /* do we have some information already? */
    if ( *line != NULL ) {
        /* we have part of the content type already
	 */
	if ( !isspace( *n_line ) ) {
	    /* bad info */
	    return( -1 );
	} 
	
	for ( ; isspace( *n_line ); n_line++ ) {
	    /* skip all but one whitespace */
	}
	n_line--;

	addlen = strlen( n_line );
	if (( newline = (char *)malloc( *len + addlen ) ) == NULL ) {
	    syslog( LOG_ERR, "parse_content-type: malloc: %m" );
	    return( -2 );
	}
	strcpy( newline, *line );
	strcat( newline, n_line );
	free( *line );
	*line = newline;
	n_line = *line;
    } else {
        *line = strdup( n_line );
	*len = strlen( n_line );
    }


    for ( j = n_line+13; isspace( *j ); j++ ) {
	/* skip all whitespace */
    }

    if ( ( i = strchr( j, '/' ) ) == NULL ) {
	return( 0 );
    }

    if ( *type == NULL ) {
	*i = '\0';
	*type = strdup( j );
	*i = '/';
    }
    i++;

    if ( ( j = strchr( i, ';' ) ) == NULL ) {
	return( 0 );
    }


    if ( *subtype == NULL ) {
        *j = '\0';
	*subtype = strdup( i );
	*j = ';';
    }
    j++;

    for ( ; isspace( *j ); j++ ) {
	/*skip whitespaces */
    }
    if ( ( i = strchr( j, '=' ) ) == NULL ) {
        return( 0 );
    }

    if ( *attribute == NULL ) {
	*i = '\0';
	*attribute = strdup( j );
	*i = '=';
    }
    i++;
    if ( *i == '"' ) {
        i++;
    }

    if ( ( j = strchr( i, ';' ) ) == NULL ) {
	return( 0 );
    }

    if ( *value == NULL ) {
        if ( *(j - 1) == '"' ) {
	    val_prev = '"';
	    j--;
	}

	*j = '\0';
	*value = strdup( i );
	*j = val_prev;
    }
    return( 1 );
}
