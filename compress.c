#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <signal.h>
#include <net.h>
#include <sys/types.h>
#include <netinet/in.h>


#include "queue.h"
#include "compress.h"

    int
page_compress( page, offset, state, line, buflen )
    struct page *page;
    int 	*state, *offset, buflen;
    char 	*line;
{
    unsigned char 		*p;

    for ( p = (unsigned char *)line; *p != '\0'; p++ ) {
	if ( *offset + 1 == buflen ) {
	    page->p_message[ *offset ] = '\0';
	    return( -1 );
	}

	if ( ( ! isascii( *p ) ) || ( iscntrl( *p ) ) || ( isspace( *p ) ) ) {
	    if ( *state != PC_SPACE ) {
	        *state = PC_SPACE;
		page->p_message[ (*offset)++ ] = ' ';
	    }
	} else if ( *p == '+' ) {
	    if ( *state != PC_PLUS2 ) {
	        if ( *state == PC_PLUS1 ) {
		    *state = PC_PLUS2;
		} else {
		    *state = PC_PLUS1;
		}
		page->p_message[ (*offset)++ ] = *p;
	    }
	} else {
	    *state = PC_NONE;
	    page->p_message[ (*offset)++ ] = *p;
	} 
    }
    page->p_message[ *offset ] = '\0';
    return( 0 );
}
