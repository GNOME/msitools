#!/bin/sh

test -n "$srcdir" || srcdir=$(dirname "$0")
test -n "$srcdir" || srcdir=.
(
  cd "$srcdir" &&
  mkdir -p m4 &&
  AUTOPOINT='intltoolize --automake --copy' autoreconf -fiv -Wall
) || exit
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
