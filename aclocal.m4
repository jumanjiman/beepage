AC_DEFUN([CHECK_KERBEROS],
[
    AC_MSG_CHECKING(whether to include kerberos support)
    if test "x$use_kerberos" = "xyes" ; then
        AC_MSG_RESULT(yes)
	AC_MSG_CHECKING(for kerberos 5)
	krbdirs="/usr /usr/local /usr/kerberos /usr/kerberosv4 \
		/usr/kerberosv5 /usr/local/kerberos" 
	AC_ARG_WITH(kerberos,
		AC_HELP_STRING([--with-kerberos=DIR], [path to kerberos]),
		krbdirs="$withval")
	for dir in $krbdirs; do
	    krbdir="$dir"
	    if test -f "$dir/include/kerberos/krb5.h"; then
		found_krb="yes";
    		CFLAGS="$CFLAGS -I$krbdir/include/kerberosIV";
		LDFLAGS="$LDFLAGS -L$krbdir/lib";
		LIBS="$LIBS -lkrb4 -lkrb5 -lk5crypto -ldes425 -lcom_err"
		break;
	    fi
	    if test -f "$dir/include/krb5.h"; then
		found_krb="yes";
    		CFLAGS="$CFLAGS -I$krbdir/include -I$krbdir/include/kerberosIV";
		LDFLAGS="$LDFLAGS -L$krbdir/lib";
		LIBS="$LIBS -lkrb4 -lkrb5 -lk5crypto -ldes425 -lcom_err"
		break;
	    fi
	done
	if test x_$found_krb != x_yes; then
	    AC_MSG_RESULT(no)
	    AC_MSG_CHECKING(for kerberos 4)
	    for dir in $krbdirs; do
		krbdir="$dir";
		if test -f "$dir/include/kerberosv4/krb.h"; then
		    found_krb="yes";
		    CFLAGS="$CFLAGS -I$krbdir/include/kerberosv4";
		    LDFLAGS="$LDFLAGS -L$krbdir/lib";	
		    LIBS="$LIBS -lkrb -ldes"
		    break;
		fi
		if test -f "$dir/include/krb.h"; then
		    found_krb="yes";
		    CFLAGS="$CFLAGS -I$krbdir/include";
		    LDFLAGS="$LDFLAGS -L$krbdir/lib";	
		    LIBS="$LIBS -lkrb -ldes";
		    break;
		fi
	    done
	fi
	if test x_$found_krb != x_yes; then
	    AC_MSG_RESULT(no)
	    AC_MSG_CHECKING(for solaris kerberos 4)
	    for dir in $krbdirs; do
		krbdir="$dir";
		if test -f "$dir/include/kerberosv4/krb.h"; then
		    found_krb="yes";
		    CFLAGS="$CFLAGS -I$krbdir/include/kerberosv4";
                    LDFLAGS="$LDFLAGS -L$krbdir/lib";
                    LIBS="$LIBS -lkrb";
		fi
		if test -f "$dir/include/krb.h"; then
		    found_krb="yes";
		    CFLAGS="$CFLAGS -I$krbdir/include/kerberosv4";
                    LDFLAGS="$LDFLAGS -L$krbdir/lib";
                    LIBS="$LIBS -lkrb";
		fi
	    done
	fi
	if test x_$found_krb != x_yes; then
	    AC_MSG_ERROR(cannot find kerberos)
	else
	    kdefs="$KDEFS -DKRB";
	    ksrc="$KSRC binhex.c";
	    kobj="$KOBJ binhex.o";
	    HAVE_KRB=yes
	fi
	AC_SUBST(LIBS)
	AC_SUBST(LDFLAGS)
	AC_SUBST(KDEFS)
	AC_SUBST(KSRC)
	AC_SUBST(KOBJ)
	AC_SUBST(HAVE_KRB)
	AC_MSG_RESULT(yes)
    else
	AC_MSG_RESULT(no)
    fi
])
