/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

struct quser {
    struct usrdb	*qu_user;
    struct quser	*qu_next;
    int			qu_seq;
    int			qu_len;
    FILE		*qu_fp;
};

struct pqueue {
    char		*q_sender;
    int			q_flags;
    struct quser	*q_users;
};

#define Q_KERBEROS	(1<<0)

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

struct pqueue	*queue_init ___P(( char *, int ));
void		queue_check ___P(( struct sigaction *, struct sigaction * ));
int		queue_recipient ___P(( struct pqueue *, char * ));
int		queue_create ___P(( struct pqueue * ));
int		queue_line ___P(( struct pqueue *, char * ));
int		queue_cleanup ___P(( struct pqueue * ));
int		queue_done ___P(( struct pqueue * ));
