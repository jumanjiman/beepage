/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

char		hextab[] = "0123456789ABCDEF";
bin2hex( bin, hex, len )
    char	*bin;
    char	*hex;
    int		len;
{
    int		i;

    for ( i = 0; i < len; i++ ) {
	*hex++ = hextab[ ( bin[ i ] >> 4 ) & 0x0f ];
	*hex++ = hextab[ ( bin[ i ] ) & 0x0f ];
    }
    *hex = '\0';

    return( 0 );
}

int		bintab[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
    int
hex2bin( hex, bin )
    char	*hex;
    char	*bin;
{
    int		len, i;

    len = strlen( hex );
    if ( len & 1 ) {			/* is len odd */
	return( -1 );
    }
    len /= 2;

    for ( i = 0; i < len; i++ ) {
	if ( !isxdigit( hex[ i * 2 ] ) || !isxdigit( hex[ i * 2 + 1 ] )) {
	    return( -1 );
	}
	bin[ i ] = bintab[ hex[ i * 2 ]] << 4 | bintab[ hex[ i * 2 + 1 ]];
    }
    return( len );
}
