/*
 * Copyright (c) 1997 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

struct modem {
    char		*m_path;
    struct modem	*m_next;
    uid_t		m_pid;
    NET			*m_net;
};

struct modem	*modem_find();
