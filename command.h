/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef __STDC__
#define ___P(x)		x
#else
#define ___P(x)		()
#endif /* __STDC__ */

int		cmdloop ___P(( int, int ));
int		argcargv ___P(( char *, char **[] ));
