/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

struct modem {
    char		*m_path;
    struct modem	*m_next;
    pid_t		m_pid;
    SNET		*m_net;
};

#define TAP_OVERHEAD	8
#define TAP_MAXLEN	256

#ifdef __STDC__
#define ___P(x)		x
#else __STDC__
#define ___P(x)		()
#endif __STDC__

struct modem	*modem_find ___P(( void ));
void		modem_checkin ___P(( pid_t ));
int		modem_add ___P(( char * ));
int		modem_connect ___P(( struct modem *, struct srvdb * ));
int		modem_disconnect ___P(( struct modem * ));
int		modem_send ___P(( struct modem *, char *, char *, int ));
void		modem_checkout ___P(( struct modem *, pid_t ));
