/*
* Copyright (c) 2002 Regents of The University of Michigan.
* All Rights Reserved.  See COPYRIGHT.
*/

#ifdef __STDC__
#define ___P(x)         x
#else __STDC__
#define ___P(x)         ()
#endif __STDC__

int	parse_content_type ___P(( char *, char **, char **, char **, char **, 
				  NET *net ));
