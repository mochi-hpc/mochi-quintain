#!/bin/bash

set -e
set -o pipefail

if [ -z $srcdir ]; then
    echo srcdir variable not set.
    exit 1
fi

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$PWD/src/.libs"

bedrock -c $srcdir/tests/mochi-quintain-provider-2svr-A.json na+sm:// &
sleep 2
bedrock -c $srcdir/tests/mochi-quintain-provider-2svr-B.json na+sm:// &
sleep 2

mpiexec -n 2 src/quintain-benchmark -g quintain.flock.json -j $srcdir/tests/quintain-benchmark-example.json -o test-output

bedrock-shutdown -f quintain.flock.json na+sm://
