/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

struct quser {
    struct usrdb	*qu_user;
    struct quser	*qu_next;
    int			qu_seq;
    int			qu_len;
    FILE		*qu_fp;
};

struct queue {
    char		*q_sender;
    int			q_flags;
    struct quser	*q_users;
};

#define Q_KERBEROS	(1<<0)

struct queue *queue_init();
