# Process this file with autoconf to produce a configure script.
AC_INIT(beepage, VERSION, beepage@umich.edu)
AC_PREREQ(2.52)
AC_COPYRIGHT([Copyright (c) 1998 Regents of The University of Michigan. All Rights Reserved.])
AC_CONFIG_SRCDIR([argcargv.c])
#AC_CONFIG_HEADER([config.h])

# Variables
#AC_ARG_WITH(kerberos, AC_HELP_STRING([--with-kerberos], [default: no kerberos support]))

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB([nsl], [inet_ntoa])
AC_CHECK_LIB([socket], [socket])
#AC_CHECK_LIB([krb], [gssapi])

CHECK_SNET

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h errno.h fcntl.h inttypes.h netdb.h netinet/in.h stdlib.h string.h strings.h sys/file.h sys/param.h sys/socket.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([dup2 ftruncate gethostbyaddr gethostbyname gethostname gettimeofday inet_ntoa isascii memset mkdir select socket strcasecmp strchr strdup strncasecmp strrchr])

AC_CONFIG_FILES([Makefile
                 libsnet/Makefile])
AC_OUTPUT
