#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>

#include "net.h"
#include "rfc2045.h"

/* Content-Type: type/subtype; attribute=value; 
 * returns zero when type, subtype, attibute and value all accounted for.
 * returns negative on error
 */

    int
parse_content_type( line, type, subtype, attribute, value, net )
    char        *line, **type, **subtype, **attribute, **value;
    NET         *net;
{
    char	*nextline = NULL;
    char	*j, *k;
    char	val_prev = ';';


    /*
     *		Parse out the subtype
     */
    for ( j = line+13; isspace( *j ); j++ ) {
	/* skip ahead over the "Content-Type:" and any white space */
    }
    
    if ( ( k = strchr( j, '/' ) ) == NULL ) {
	return( -1 );
    }

    *k = '\0';
    *type = strdup( j );
    *k = '/';
    k++;





    /*
     *		Parse out the subtype
     */
    if ( ( j = strchr( k, ';' ) ) == NULL ) {
	return( -1 );
    }
    *j = '\0';
    *subtype = strdup( k );
    *j = ';';
    j++;
    /* 
     * How does strchr behave if it's searching forward from a null char?
     * check on this for a possible bad pointer problem.
     */







    /*
     *		Parse out the attribute
     */

    if ( ( k = strchr( j, '=' ) ) == NULL ) {
	/* Now we need to read the next line, since the attribute and 
	 * value might be on the next line. This is the only place where
	 * it's legal to break to the next line, since you can't do so 
	 * in the middle of type/subtype or attribute=value.
	 */
	nextline = net_getline( net, NULL );
	if ( ! isspace( *nextline ) ) {
	    return( -1 );
	}

	j = nextline;
	if ( ( k = strchr( j, '=' ) ) == NULL ) {
	    return( -1 );
	}

    }

    for ( ; isspace( *j ); j++ ) {
	/* skip spaces */
    }

    *k = '\0';
    *attribute = strdup( j );
    *k = '=';
    k++;





    /* 
     *    parse out the value 
     */

    if ( ( j = strchr( k, ';' ) ) == NULL ) {
	return( -1 );
    }
    if ( *(j - 1) == '"' ) {
	val_prev = '"';
	j--;
    }
    if ( *k == '"' ) {
	k++;
    }
    
    *j = '\0';
    *value = strdup( k );
    *j = val_prev;


    return( 0 );
}
