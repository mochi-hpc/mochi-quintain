#!/bin/bash

set -e
set -o pipefail

if [ -z $srcdir ]; then
    echo srcdir variable not set.
    exit 1
fi

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$PWD/src/.libs"
bedrock -c $srcdir/tests/mochi-quintain-provider.yaml na+sm:// &
BEDROCK_PID=$!

sleep 2

src/quintain-benchmark -g quintain.ssg -j $srcdir/tests/quintain-benchmark-example.json

kill $BEDROCK_PID
wait
