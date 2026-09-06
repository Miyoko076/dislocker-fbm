# Dislocker -- top level (ported from CMake).

ACLOCAL_AMFLAGS = -I m4

# lib (gnulib) must build before src, which links against it.
SUBDIRS = lib src test

EXTRA_DIST = \
	m4/gnulib-cache.m4 \
	include \
	man \
	README.md \
	LICENSE.txt

# Smoke-test each built binary's help output (CMake's travis-test target).
.PHONY: travis-test
travis-test:
	src/dislocker-fuse -h
	src/dislocker-file -h
	src/dislocker-metadata -h
	src/dislocker-bek -h
	man -w dislocker
