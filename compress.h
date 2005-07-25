/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
*/


#ifdef __STDC__
#define ___P(x)         x
#else
#define ___P(x)         ()
#endif /* __STDC__ */

#define PC_NONE         0
#define PC_SPACE        1
#define PC_PLUS1        2
#define PC_PLUS2        3


int	page_compress ___P(( struct page *, int *range, 
				    int *state, char *line, int ));

