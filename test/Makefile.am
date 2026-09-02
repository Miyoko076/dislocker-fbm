# Dislocker -- test/ (ported from test/CMakeLists.txt).

# -I$(srcdir) lets VPATH/dist builds find test.h, test_vectors.h.
AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir)/include \
	@CRYPTO_CFLAGS@

if CRYPTO_MBEDTLS
AM_CPPFLAGS += -I$(top_srcdir)/include/dislocker/encryption/mbedtls
endif
if CRYPTO_OPENSSL
AM_CPPFLAGS += -I$(top_srcdir)/include/dislocker/encryption/openssl
endif

check_PROGRAMS = crypto_tests
crypto_tests_SOURCES = test-crypto.c
crypto_tests_LDADD = $(top_builddir)/src/libdislocker.la @CRYPTO_LIBS@

TESTS = crypto_tests

EXTRA_DIST = test.h test_vectors.h
