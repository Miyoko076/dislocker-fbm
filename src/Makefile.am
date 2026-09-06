# Dislocker -- src/ (ported from src/CMakeLists.txt).

# The sources use these as string literals / bare tokens but don't include
# <config.h>; CMake passed them as -D flags, so we must too. AUTHOR has a
# space, hence the single-quoted -D.
DIS_DEFINES = \
	-DPROGNAME=\"dislocker\" \
	'-DAUTHOR="Romain Coltel"' \
	-DVERSION=\"@DIS_VERSION@\" \
	-D__OS=\"@DIS_OS@\" \
	-D@DIS_OS_MACRO@ \
	-D__ARCH=\"@DIS_ARCH@\" \
	-D@DIS_ARCH_MACRO@ \
	-D_FILE_OFFSET_BITS=64

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/include \
	$(DIS_DEFINES) \
	@CRYPTO_CFLAGS@ @FUSE_CFLAGS@ @RUBY_CFLAGS@

# CMake's add_definitions/include_directories were directory-wide, so
# -D_HAVE_RUBY and the ruby -I must reach every target, not just ruby.c
# (ruby headers redefine snprintf -> ruby_snprintf).
if HAVE_RUBY
AM_CPPFLAGS += -D_HAVE_RUBY=\"@RUBY_VERSION_STR@\"
endif

if CRYPTO_MBEDTLS
AM_CPPFLAGS += -I$(top_srcdir)/include/dislocker/encryption/mbedtls
endif
if CRYPTO_OPENSSL
AM_CPPFLAGS += -I$(top_srcdir)/include/dislocker/encryption/openssl
endif

AM_CFLAGS = -fPIC @WARN_FLAGS@ @HARDEN_FLAGS@

lib_LTLIBRARIES = libdislocker.la

libdislocker_la_SOURCES = \
	dislocker.c common.c config.c \
	xstd/xstdio.c xstd/xstdlib.c \
	metadata/datums.c metadata/metadata.c metadata/vmk.c \
	metadata/fvek.c metadata/extended_info.c \
	metadata/guid.c metadata/print_metadata.c \
	accesses/stretch_key.c accesses/accesses.c \
	accesses/rp/recovery_password.c \
	accesses/user_pass/user_pass.c accesses/bek/bekfile.c \
	encryption/encommon.c encryption/decrypt.c encryption/encrypt.c \
	encryption/diffuser.c encryption/crc32.c encryption/aes-xts.c \
	ntfs/clock.c ntfs/encoding.c \
	inouts/inouts.c inouts/prepare.c inouts/sectors.c

if HAVE_RUBY
libdislocker_la_SOURCES += ruby.c
endif

# -version-number yields libdislocker.so.0.7.3 (matches CMake / the distro
# package) instead of libtool's interface-numbering scheme.
libdislocker_la_LDFLAGS = \
	-version-number $(VERSION_MAJOR):$(VERSION_MINOR):$(VERSION_RELEASE) \
	@DIS_OS_LDFLAGS@ @DIS_HARDEN_LDFLAGS@
libdislocker_la_LIBADD = \
	$(top_builddir)/lib/libgnu.la \
	@PTHREAD_LIBS@ @CRYPTO_LIBS@ @DIS_OS_LIBS@ @RUBY_LIBS@

bin_PROGRAMS = dislocker-file dislocker-metadata dislocker-bek
if HAVE_FUSE
bin_PROGRAMS += dislocker-fuse
endif

dislocker_common_ldflags = -pie -fPIE @DIS_HARDEN_LDFLAGS@
dislocker_common_libs    = libdislocker.la @RUBY_LIBS@

dislocker_file_SOURCES     = dislocker-file.c
dislocker_file_LDFLAGS     = $(dislocker_common_ldflags)
dislocker_file_LDADD       = $(dislocker_common_libs)

dislocker_metadata_SOURCES = dislocker-metadata.c
dislocker_metadata_LDFLAGS = $(dislocker_common_ldflags)
dislocker_metadata_LDADD   = $(dislocker_common_libs)

dislocker_bek_SOURCES      = dislocker-bek.c
dislocker_bek_LDFLAGS      = $(dislocker_common_ldflags)
dislocker_bek_LDADD        = $(dislocker_common_libs)

dislocker_fuse_SOURCES     = dislocker-fuse.c
dislocker_fuse_CPPFLAGS    = $(AM_CPPFLAGS) -DFUSE_USE_VERSION=314
dislocker_fuse_LDFLAGS     = $(dislocker_common_ldflags)
dislocker_fuse_LDADD       = $(dislocker_common_libs) @FUSE_LIBS@

if HAVE_RUBY
bin_SCRIPTS = dislocker-find

dislocker-find: dislocker-find.rb
	$(AM_V_GEN)cp "$<" "$@" && chmod +x "$@"

CLEANFILES = dislocker-find
endif

# man pages are gzipped at build time; installed by hand because automake's
# man_MANS is for uncompressed pages. Upstream ships fuse/file/find only.
DIS_MAN = $(top_srcdir)/man/@DIS_MANSRC@

man_gz = dislocker-file.1.gz
if HAVE_FUSE
man_gz += dislocker-fuse.1.gz
endif
if HAVE_RUBY
man_gz += dislocker-find.1.gz
endif

noinst_DATA = $(man_gz)

dislocker-file.1.gz: $(DIS_MAN)/dislocker-file.1
	$(AM_V_GEN)gzip -c "$<" > "$@"
dislocker-fuse.1.gz: $(DIS_MAN)/dislocker-fuse.1
	$(AM_V_GEN)gzip -c "$<" > "$@"
dislocker-find.1.gz: $(DIS_MAN)/dislocker-find.1
	$(AM_V_GEN)gzip -c "$<" > "$@"

disman1dir = $(mandir)/man1
disman1_DATA = $(man_gz)

MOSTLYCLEANFILES = $(man_gz)

# dislocker -> dislocker-fuse symlinks (binary + man page), as CMake made.
if HAVE_FUSE
install-exec-hook:
	cd "$(DESTDIR)$(bindir)" && \
	  rm -f dislocker && \
	  $(LN_S) dislocker-fuse dislocker

install-data-hook:
	cd "$(DESTDIR)$(disman1dir)" && \
	  rm -f dislocker.1.gz && \
	  $(LN_S) dislocker-fuse.1.gz dislocker.1.gz

uninstall-hook:
	rm -f "$(DESTDIR)$(bindir)/dislocker" \
	      "$(DESTDIR)$(disman1dir)/dislocker.1.gz"
endif
