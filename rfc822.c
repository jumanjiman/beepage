#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>

#include <net.h>
#include "queue.h"
#include "rfc2045.h"
#include "rfc822.h"

struct ih   integral_headers[ ] = { 
    { "From:", IH_FROM }, 
    { "Subject:", IH_SUBJ }, 
    { "To:", IH_TO }, 
    { "Cc:", IH_TO },
    { "Bcc:", IH_TO } 
};

    int
parse_header( line, keyheaders )
    char 	*line;
    int		*keyheaders; 
{
    unsigned char 	*i;
    char		a;
    int			ih_a;

    if ( isspace( (int)*line ) ) {
	if ( ! ( *keyheaders & FIRST ) ) {
	    return( 0 );
	} else {
	    *keyheaders |= FIRST;
	    return( -1 );
	}
    }
    /* look at each character, looking for a : before I get a ' ', then, check
       if the header I got is in the integral_headers array */
	 
    /*RFC822 "a  line  beginning a [header] field starts with a printable 
		character which is not a colon." */

    if ( ( *line == ':' ) || ( isprint( ( int )*line ) == 0 ) ) {
	return( -1 );
    }
    for ( i = (unsigned char *)line; i != '\0'; i++ ) {
	/* rfc822 Section B.1. no cntrl chars in a field-name */ 
	if ( iscntrl( *i ) != 0 ) {
	    return( -1 );
	} else if ( *i == ':' ) {
	    if ( ( *(i + 1)  != '\0' ) && ( isspace( *(i + 1) ) == 0 ) ) {
	        continue;
	    } else {
	        a = *(i + 1);
	        *(i + 1) = '\0';
		for ( ih_a = 0; ih_a < 5; ih_a++ ) {
		    if ( strcasecmp( integral_headers[ ih_a ].ih_name, line ) 
									== 0 ) {
			*keyheaders |= integral_headers[ ih_a ].ih_bit;
			break;
		    }
		}
		*(i + 1) = a;
		return( 0 );
	    }
	} else if ( isspace( *i ) != 0 ) {
	    return( -1 );
	}
    }
    return( -1 );
}

    struct datalines *
dl_alloc( line ) 
    char *line;
{

    struct datalines *i;

    if (( i = (struct datalines *)malloc(sizeof(struct datalines))) == NULL ) {
	perror( "malloc" );
	return( NULL );
    }

    if (( i->d_line = (char *)malloc( strlen( line ) + 1 )) == NULL ) {
	perror( "malloc" );
	return( NULL );
    }
    strcpy(i->d_line, line);

    return( i );
}
    

    int
dl_append( line, d_head, d_tail )
    char	*line;
    struct datalines **d_head;
    struct datalines **d_tail;
{
    struct datalines *i;

    if ( ( i = dl_alloc( line ) ) == NULL ) {
        return( -1 );
    }
    i->d_next = NULL;
     
    if ( *d_head == NULL ) {
	*d_head = *d_tail = i;
    } else {
	(*d_tail)->d_next = i;
	*d_tail = i;
    }
    return( 1 );
}

    int
dl_prepend( line, d_head )
    char	*line;
    struct datalines **d_head;
{
    struct datalines *t;

    if ( ( t = dl_alloc( line ) ) == NULL ) {
        return( -1 );
    }

    t->d_next = *d_head;
    *d_head = t;
    return( 0 );
}

    void
dl_output( d_head, pq )
    struct datalines	*d_head;
    struct pqueue	*pq;
{
    struct datalines *i;

    for ( i = d_head; i != NULL; i = i->d_next ) {
	queue_line( pq, i->d_line );
    }
}
						     
    void
dl_free( d_head )
    struct datalines	**d_head;
{
    struct datalines *i, *j;
    i = *d_head;
    while ( i != NULL ) {
        free( i->d_line );
	j = i->d_next;
	free( i );
	i = j;
    }
    *d_head = NULL;
    return;
}

/* read an rfc822 compliant message and return from, subject, and MIME info
 *
 *		-1 on system error
 * Returns	0 on MIME error
 *		1 on sucessful MIME extraction
 *
 */
    int
read_headers( net, from, subj, type, subtype, attribute, value, mime )
    NET		*net;
    char	**from, **subj, **type, **subtype, **attribute, **value;
    int		*mime;
{

    char	*line, *at;

    *mime = 0;

    while ( ( line = net_getline( net, NULL )) != NULL ) {

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
                if ( ( *from = (char *)malloc( strlen( line ) + 1 ) ) == NULL) {
                    syslog( LOG_ERR, "malloc: %m" );
                    return( -1 );
                }
            } else {
                /* cut off @... */
                if ( ( at = strchr( line, '@' ) ) != NULL ) {
                    *at = '\0';
                }
                /* sizeof the line - "from: " + null char */
                if ( ( *from = (char *)malloc( strlen( line ) - 5 ) ) == NULL) {
                    syslog( LOG_ERR, "malloc: %m" );
                    return( -1 );
                }
                line += 6;
            }
            sprintf( *from, "%s", line );
        }
        if ( strncasecmp( "Subject:", line, 8 ) == 0 ) {
            if ( ( strcasecmp( line, "Subject: page sent" ) != 0 ) &&
                     ( strcmp( line, "Subject:" ) != 0 ) ) {
                /* sizeof the line - "subject: " + null char */
                if ( ( *subj = (char *)malloc( strlen( line ) - 8 ) ) == NULL) {
                    syslog( LOG_ERR, "malloc: %m" );
                    return( -1 );
                }
                line += 9;
                sprintf( *subj, "%s", line );
            }
        }
#ifdef notdef
	/*  This is required for the first header, but not required 
	 *  for subsequent parts of multipart messages...
	 *  I think we should let it slide...
	 */
        if ( strncmp( "MIME-Version:", line, 13 ) == 0 ) {
            *mime = 1;
            continue;
        }
#endif
        if ( strncmp( "Content-Type:", line, 13 ) == 0 ) {
	    *mime = 1;
            if ( parse_content_type( line, &type, &subtype,
                                    &attribute, &value, net ) < 0 ) {
                return( 0 );
            }

        }
    }
    return( 1 );
}
