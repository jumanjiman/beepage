/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#define U_MUSTAUTH	(1<<0)
#define U_SENDMAIL	(1<<1)

struct srvdb {
    char		*s_name;
    struct srvdb	*s_next;
    char		*s_phone;
    uid_t		s_pid;
};

struct usrdb {
    char		*u_name;
    struct usrdb	*u_next;
    int			u_flags;
    struct srvdb	*u_service;
    char		*u_pin;
};

struct usrdb		*usrdb_find();
struct srvdb		*srvdb_next();
