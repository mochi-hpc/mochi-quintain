#!/bin/bash

set -e
set -o pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: quaintain-benchmark-parse.sh <datafile.gz>"
    exit 1
fi

# summary statistics
duration=`zcat $1 |grep duration_seconds |cut -f 2 -d : |cut -f 1 -d ,`
ops=`zcat $1|grep sample_trace |wc -l`
ops_s=`echo $ops/$duration | bc -l`

echo "# aggregate statistics:" > $1.summary.txt
echo "$ops_s ops/s" >> $1.summary.txt
