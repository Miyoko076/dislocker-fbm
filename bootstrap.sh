#!/bin/sh
# Regenerate the Autotools build system after a fresh checkout.
# Requires: autoconf, automake, libtool, pkg-config.
set -e
autoreconf --install --force
echo "Done. Now run: ./configure && make"
