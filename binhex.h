/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

int             bin2hex ___P(( char *, char *, int ));
int             hex2bin ___P(( char *, char * ));
