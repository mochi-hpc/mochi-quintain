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

# all latencies, single column
zcat $1 |grep sample_trace |grep -v elapsed |cut -f 5 > $1.latency.dat

# all latencies, csv with start time
echo "start,latency" > $1.latency-scatter.dat
zcat $1 |grep sample_trace |grep -v elapsed | awk '{print $3 "," $5}' >> $1.latency-scatter.dat
