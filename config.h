/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#define U_KERBEROS	(1<<0)
#define U_SENDMAIL	(1<<1)

struct srvdb {
    char		*s_name;
    struct srvdb	*s_next;
    char		*s_phone;
    pid_t		s_pid;
    int			s_maxlen;
};

struct usrdb {
    char		*u_name;
    struct usrdb	*u_next;
    int			u_flags;
    struct srvdb	*u_service;
    char		*u_pin;
};

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

struct usrdb		*usrdb_find ___P(( char * ));
int			usrdb_read ___P(( char * ));
struct srvdb		*srvdb_next ___P(( void ));
int			srvdb_read ___P(( char * ));
void			srvdb_checkin ___P(( pid_t ));
void			srvdb_checkout ___P(( struct srvdb *, pid_t ));
struct srvdb		*srvdb_find ___P(( char	* ));
