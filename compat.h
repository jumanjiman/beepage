/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

# if defined( sun ) && defined( __svr4__ )

#define LOCK_SH		(1<<0)
#define LOCK_EX		(1<<1)
#define LOCK_NB		(1<<2)
#define LOCK_UN		(1<<4)

#ifdef __STDC__
#define ___P(x)		x
#else
#define ___P(x)		()
#endif /* __STDC__ */

int	flock ___P(( int, int ));

# endif /* sun __svr4__ */
