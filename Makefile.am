# Dislocker -- top level (ported from CMake).

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test

EXTRA_DIST = \
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
