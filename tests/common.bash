#!/usr/bin/bash

BUILDDIR=${BUILDDIR:=`pwd`}
SRCDIR=${SRCDIR:=`pwd`/..}

testdatabase="$BUILDDIR/tests/testdatabase$EXT"
testrecord="$BUILDDIR/tests/testrecord$EXT"
testsuminfo="$BUILDDIR/tests/testsuminfo$EXT"
msibuild="$BUILDDIR/tools/msibuild$EXT"
msiinfo="$BUILDDIR/tools/msiinfo$EXT"
wixl="$BUILDDIR/tools/wixl/wixl$EXT"
wixlheat="$BUILDDIR/tools/wixl/wixl-heat$EXT"

setup() {
    export TEST_SUITE_TMPDIR=`mktemp -d`
    cp -R "$SRCDIR/tests/data"/* "$TEST_SUITE_TMPDIR"
    cd "$TEST_SUITE_TMPDIR"
}

teardown() {
    if [ -n "$TEST_SUITE_TMPDIR" ]; then
        echo rm -rf "$TEST_SUITE_TMPDIR"
    fi
}
