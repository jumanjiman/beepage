/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

struct quser {
    struct usrdb	*qu_user;
    struct quser	*qu_next;
    int			qu_seq;
    FILE		*qu_fp;
};

struct queue {
    char		*q_sender;
    struct quser	*q_users;
};

struct queue *queue_init();
