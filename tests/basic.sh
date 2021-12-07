#!/bin/bash

set -e
set -o pipefail

if [ -z $srcdir ]; then
    echo srcdir variable not set.
    exit 1
fi

bedrock -c $srcdir/tests/mochi-quintain-provider.yaml na+sm:// &
BEDROCK_PID=$!

sleep 2

src/quintain-benchmark

kill $BEDROCK_PID
wait
