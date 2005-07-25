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
	    AC_MSG_CHECKING(for heimdal kerberos 5)
	    for dir in $krbdirs; do
		krbdir="$dir";
	        if test -f "$dir/include/kerberosV/krb5.h"; then
		    found_krb="yes";
		    CFLAGS="$CFLAGS -I$dir/include/kerberosV -I$dir/include/kerberosIV";
		    LDFLAGS="$LDFLAGS -L$krbdir/lib";
		    LIBS="$LIBS -lkrb -lkrb5 -lcom_err -lacl1"
		    break;
	        fi
	    done
	fi
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
AC_DEFUN([CHECK_SSL],
[
    AC_MSG_CHECKING(for ssl)
    ssldirs="/usr/local/openssl /usr/lib/openssl /usr/openssl \
            /usr/local/ssl /usr/lib/ssl /usr/ssl \
            /usr/pkg /usr/local /usr"
    AC_ARG_WITH(ssl,
            AC_HELP_STRING([--with-ssl=DIR], [path to ssl]),
            ssldirs="$withval")
    for dir in $ssldirs; do
        ssldir="$dir"
        if test -f "$dir/include/openssl/ssl.h"; then
            found_ssl="yes";
            CPPFLAGS="$CPPFLAGS -I$ssldir/include";
            break;
        fi
        if test -f "$dir/include/ssl.h"; then
            found_ssl="yes";
            CPPFLAGS="$CPPFLAGS -I$ssldir/include";
            break
        fi
    done
    if test x_$found_ssl != x_yes; then
        AC_MSG_ERROR(cannot find ssl libraries)
    else
        AC_DEFINE(HAVE_LIBSSL)
        LIBS="$LIBS -lssl -lcrypto";
        LDFLAGS="$LDFLAGS -L$ssldir/lib";
    fi
    AC_MSG_RESULT(yes)
])

AC_DEFUN([CHECK_SASL],
[
    AC_MSG_CHECKING(for sasl)
    sasldirs="/usr/local/sasl2 /usr/lib/sasl2 /usr/sasl2 \
            /usr/pkg /usr/local /usr"
    AC_ARG_WITH(sasl,
            AC_HELP_STRING([--with-sasl=DIR], [path to sasl]),
            sasldirs="$withval")
    if test x_$withval != x_no; then
        for dir in $sasldirs; do
            sasldir="$dir"
            if test -f "$dir/include/sasl/sasl.h"; then
                found_sasl="yes";
                CPPFLAGS="$CPPFLAGS -I$sasldir/include";
                break;
            fi
            if test -f "$dir/include/sasl.h"; then
                found_sasl="yes";
                CPPFLAGS="$CPPFLAGS -I$sasldir/include";
                break
            fi
        done
        if test x_$found_sasl == x_yes; then
            AC_DEFINE(HAVE_LIBSASL)
            LIBS="$LIBS -lsasl2";
            LDFLAGS="$LDFLAGS -L$sasldir/lib";
            AC_MSG_RESULT(yes)
        else
            AC_MSG_RESULT(no)
        fi
    else
        AC_MSG_RESULT(no)
    fi
])
