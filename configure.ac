# Dislocker -- Autotools build (ported from the upstream CMake build).
# Copyright (C) 2012-2013  Romain Coltel, Hervé Schauer Consultants
# GNU GPL v2 or later.

AC_PREREQ([2.69])
AC_INIT([dislocker], [0.7.3], [], [dislocker])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([1.16 foreign subdir-objects -Wall -Werror])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/dislocker.c])
AC_CONFIG_HEADERS([config.h])

VERSION_MAJOR=0
VERSION_MINOR=7
VERSION_RELEASE=3
AC_SUBST([VERSION_MAJOR])
AC_SUBST([VERSION_MINOR])
AC_SUBST([VERSION_RELEASE])

# The sources use PROGNAME/AUTHOR/VERSION/__OS/__ARCH/_HAVE_RUBY as string
# literals and bare tokens but do NOT include <config.h>; CMake supplied them
# as -D flags, so we inject them the same way via CPPFLAGS in src/Makefile.am.
# Only raw values are substituted here.
DIS_VERSION="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_RELEASE"
AC_SUBST([DIS_VERSION])

# Record whether the user set CFLAGS before AC_PROG_CC injects its default
# "-g -O2". The build controls optimization via HARDEN_FLAGS (-O1); leaving
# the default -O2 in place would override it and change codegen.
dis_user_cflags_set=${CFLAGS+yes}

AC_PROG_CC

# gnulib early initialization (must come right after AC_PROG_CC).
gl_EARLY
AC_PROG_LN_S
AM_PROG_AR

if test "x$dis_user_cflags_set" != xyes; then
    CFLAGS=""
fi
LT_INIT([disable-static])
AC_CANONICAL_HOST
PKG_PROG_PKG_CONFIG

# VERSION_DBG: git branch:commit, best-effort (absent in release tarballs).
AC_CHECK_PROG([GIT], [git], [git])
if test -n "$GIT" && test -d "$srcdir/.git"; then
    GIT_BRANCH=`cd "$srcdir" && $GIT rev-parse --abbrev-ref HEAD 2>/dev/null`
    GIT_COMMIT=`cd "$srcdir" && $GIT log -n 1 --pretty=format:%t 2>/dev/null`
    if test -n "$GIT_COMMIT"; then
        AC_DEFINE_UNQUOTED([VERSION_DBG], ["$GIT_BRANCH:$GIT_COMMIT"],
                           [git branch:commit at build time])
    fi
fi

# Host OS / arch -> the -D__<OS> / -D__ARCH* tokens the sources expect.
case "$host_os" in
  linux*)   DIS_OS="Linux";   DIS_OS_MACRO="__LINUX"   ;;
  darwin*)  DIS_OS="Darwin";  DIS_OS_MACRO="__DARWIN"  ;;
  freebsd*) DIS_OS="FreeBSD"; DIS_OS_MACRO="__FREEBSD" ;;
  mingw*|msys*|cygwin*)
            DIS_OS="Windows"; DIS_OS_MACRO="__WINDOWS" ;;
  *)        DIS_OS="$host_os"; DIS_OS_MACRO="" ;;
esac
AC_SUBST([DIS_OS])
AC_SUBST([DIS_OS_MACRO])

AM_CONDITIONAL([OS_DARWIN],  [test "x$DIS_OS" = xDarwin])
AM_CONDITIONAL([OS_FREEBSD], [test "x$DIS_OS" = xFreeBSD])
AM_CONDITIONAL([OS_LINUX],   [test "x$DIS_OS" = xLinux])
AM_CONDITIONAL([OS_WINDOWS], [test "x$DIS_OS" = xWindows])

AC_CHECK_SIZEOF([void *])
if test "$ac_cv_sizeof_void_p" -eq 8; then
    DIS_ARCH="x86_64"; DIS_ARCH_MACRO="__ARCH_X86_64"
else
    DIS_ARCH="x86";    DIS_ARCH_MACRO="__ARCH_X86"
fi
AC_SUBST([DIS_ARCH])
AC_SUBST([DIS_ARCH_MACRO])

AC_SYS_LARGEFILE

# Warning + hardening flags (overridable by packagers).
AC_ARG_VAR([WARN_FLAGS],   [compiler warning flags])
AC_ARG_VAR([HARDEN_FLAGS], [compiler hardening flags])
: ${WARN_FLAGS="-Wall -Wextra"}
: ${HARDEN_FLAGS="-fstack-protector -fstrict-aliasing -D_FORTIFY_SOURCE=2 -O1"}
if test "x$DIS_OS" != xDarwin; then
    WARN_FLAGS="$WARN_FLAGS -Wconversion"
fi
AC_SUBST([WARN_FLAGS])
AC_SUBST([HARDEN_FLAGS])

DIS_HARDEN_LDFLAGS=""
if test "x$GCC" = xyes; then
    if test "x$DIS_OS" = xWindows; then
        # PE/Windows equivalents of ELF's -z now / -z relro:
        # --dynamicbase enables ASLR, --nxcompat enables DEP (NX).
        # (--high-entropy-va is omitted: it needs an image-base above 4GB
        #  to be fully effective, which is out of scope here.)
        DIS_HARDEN_LDFLAGS="-Wl,--dynamicbase -Wl,--nxcompat"
    else
        # ELF hardening (GNU ld): immediate binding + read-only relocations.
        DIS_HARDEN_LDFLAGS="-Wl,-z,now -Wl,-z,relro"
    fi
fi
AC_SUBST([DIS_HARDEN_LDFLAGS])

DIS_OS_LIBS=""
DIS_OS_LDFLAGS=""
if test "x$DIS_OS" = xDarwin; then
    DIS_OS_LDFLAGS="-Wl,-bind_at_load"
    DIS_OS_LIBS="-liconv"
fi
if test "x$DIS_OS" = xWindows; then
    # PE/DLL targets cannot have unresolved symbols; libtool refuses to build
    # a shared library on mingw/msys/cygwin without this flag.
    DIS_OS_LDFLAGS="-no-undefined"
    # libiconv is a separate library on Windows (ntfs/encoding.c uses it).
    DIS_OS_LIBS="-liconv"
fi
AC_SUBST([DIS_OS_LIBS])
AC_SUBST([DIS_OS_LDFLAGS])

PTHREAD_LIBS=""
if test "x$DIS_OS" != xWindows; then
    AC_CHECK_LIB([pthread], [pthread_create],
        [PTHREAD_LIBS="-lpthread"],
        [AC_MSG_WARN([libpthread not found; continuing without it])])
fi
AC_SUBST([PTHREAD_LIBS])

# Crypto backend: mbedtls (default) or openssl.
AC_ARG_WITH([crypto],
    [AS_HELP_STRING([--with-crypto=@<:@mbedtls|openssl@:>@],
                    [crypto library backend (default: mbedtls)])],
    [crypto_backend=$withval], [crypto_backend=mbedtls])

case "$crypto_backend" in
  mbedtls)
    PKG_CHECK_MODULES([CRYPTO], [mbedcrypto], [have_crypto=yes], [have_crypto=no])
    if test "x$have_crypto" = xno; then
        AC_CHECK_HEADER([mbedtls/aes.h], [],
            [AC_MSG_ERROR([mbedtls headers not found; install libmbedtls-dev or use --with-crypto=openssl])])
        AC_CHECK_LIB([mbedcrypto], [mbedtls_aes_setkey_enc],
            [CRYPTO_LIBS="-lmbedcrypto"; CRYPTO_CFLAGS=""],
            [AC_MSG_ERROR([libmbedcrypto not found])])
        AC_SUBST([CRYPTO_CFLAGS])
        AC_SUBST([CRYPTO_LIBS])
    fi
    AC_DEFINE([DISLOCKER_CRYPTO_MBEDTLS], [1], [Use mbedTLS crypto backend])
    ;;
  openssl)
    PKG_CHECK_MODULES([CRYPTO], [libcrypto >= 1.1.0], [have_crypto=yes], [have_crypto=no])
    if test "x$have_crypto" = xno; then
        AC_CHECK_HEADER([openssl/aes.h], [],
            [AC_MSG_ERROR([openssl headers not found; install libssl-dev])])
        AC_CHECK_LIB([crypto], [AES_set_encrypt_key],
            [CRYPTO_LIBS="-lcrypto"; CRYPTO_CFLAGS=""],
            [AC_MSG_ERROR([libcrypto not found])])
        AC_SUBST([CRYPTO_CFLAGS])
        AC_SUBST([CRYPTO_LIBS])
    fi
    AC_DEFINE([DISLOCKER_CRYPTO_OPENSSL], [1], [Use OpenSSL crypto backend])
    ;;
  *)
    AC_MSG_ERROR([unknown --with-crypto=$crypto_backend (mbedtls|openssl)])
    ;;
esac
AM_CONDITIONAL([CRYPTO_MBEDTLS], [test "x$crypto_backend" = xmbedtls])
AM_CONDITIONAL([CRYPTO_OPENSSL], [test "x$crypto_backend" = xopenssl])
AC_MSG_NOTICE([crypto backend: $crypto_backend])

# FUSE (fuse3), required for mounting unless disabled.
AC_ARG_WITH([fuse],
    [AS_HELP_STRING([--without-fuse],
                    [disable FUSE support (no volume mounting)])],
    [with_fuse=$withval], [with_fuse=yes])

have_fuse=no
if test "x$with_fuse" != xno; then
    PKG_CHECK_MODULES([FUSE], [fuse3], [have_fuse=yes],
        [AC_MSG_ERROR([FUSE (fuse3) not found. Install libfuse3-dev or configure with --without-fuse.])])
fi
AM_CONDITIONAL([HAVE_FUSE], [test "x$have_fuse" = xyes])
if test "x$have_fuse" = xno; then
    AC_MSG_WARN([FUSE disabled: dislocker will be unable to mount BitLocker volumes])
fi

# Ruby bindings: yes | no | auto (default auto).
AC_ARG_WITH([ruby],
    [AS_HELP_STRING([--with-ruby=@<:@yes|no|auto@:>@],
                    [Ruby bindings (default: auto)])],
    [with_ruby=$withval], [with_ruby=auto])

have_ruby=no
RUBY_VERSION_STR=""
if test "x$with_ruby" != xno; then
    PKG_CHECK_MODULES([RUBY], [ruby],
        [have_ruby=yes
         RUBY_VERSION_STR=`$PKG_CONFIG --modversion ruby`],
        [have_ruby=no])
    if test "x$have_ruby" = xno; then
        if test "x$with_ruby" = xyes; then
            AC_MSG_ERROR([Ruby bindings requested but Ruby not found])
        else
            AC_MSG_NOTICE([Ruby not found, Ruby bindings disabled])
        fi
    fi
fi
AM_CONDITIONAL([HAVE_RUBY], [test "x$have_ruby" = xyes])
AC_SUBST([RUBY_VERSION_STR])

AC_CHECK_PROG([RUBY_PROG], [ruby], [ruby])
AC_SUBST([RUBY_PROG])

DIS_MANSRC="linux"
case "$DIS_OS" in
  Darwin)  DIS_MANSRC="darwin" ;;
  FreeBSD) DIS_MANSRC="freebsd" ;;
esac
AC_SUBST([DIS_MANSRC])

# gnulib module initialization (getline, pread, pwrite + dependencies).
gl_INIT

AC_CONFIG_FILES([
    Makefile
    lib/Makefile
    src/Makefile
    test/Makefile
    src/dislocker-find.rb
])
AC_OUTPUT

echo ""
echo "  dislocker $VERSION_MAJOR.$VERSION_MINOR.$VERSION_RELEASE configured:"
echo "    host:           $host"
echo "    crypto backend: $crypto_backend"
echo "    FUSE support:   $have_fuse"
echo "    Ruby bindings:  $have_ruby"
echo ""
