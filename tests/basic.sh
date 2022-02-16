#!/bin/bash

command -v bedrock-shutdown >& /dev/null
HAS_SHUTDOWN=$?

set -e
set -o pipefail

if [ -z $srcdir ]; then
    echo srcdir variable not set.
    exit 1
fi

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$PWD/src/.libs"
bedrock -c $srcdir/tests/mochi-quintain-provider.json na+sm:// &
BEDROCK_PID=$!

sleep 2

src/quintain-benchmark -g quintain.ssg -j $srcdir/tests/quintain-benchmark-example.json -o test-output

# if the bedrock-shutdown utility is available then use that to gracefully
# shut down the daemon (which makes things easier for memory debuggers like
# address-sanitizer)
if [ $HAS_SHUTDOWN -eq 0 ]; then
    bedrock-shutdown -s quintain.ssg na+sm://
else
    kill $BEDROCK_PID
    wait
fi
