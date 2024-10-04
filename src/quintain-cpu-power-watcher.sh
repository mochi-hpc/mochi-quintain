#!/bin/bash

# report power usage in watts over 1 second intervals for each powercap rapl
# zone

# derived from example at https://www.reddit.com/r/linuxquestions/comments/1dp81bq/hardware_power_consumption/`
# changes:
# - some bug fixes in the watts conversion
# - track each powercap/rapl zone separately, and report its name
# - use floating point timestamps and calculations

# you can temporarily enable access to the required counters (i.e. on a
# development laptop) by running the following:
#
# sudo chmod -R a+r /sys/class/powercap/intel-rapl

declare -A zones
declare -A old_zones

old_time=0
while :; do
    # figure out how many zones are available; store current microjoules value
    # for each in an associative array
    for i in `ls /sys/class/powercap/intel-rapl/ |grep intel-rapl:`; do
        name=`cat /sys/class/powercap/intel-rapl/$i/name`
        energy_uj=`cat /sys/class/powercap/intel-rapl/$i/energy_uj`
        zones[$name]=$energy_uj
    done

    # current timestamp in seconds and nanoseconds
    time=$(date +%s.%N)

    if [ ${#old_zones[@]} -ne 0 ]; then
        # normal case.  calculate power in watts for each zone based on
        # elapsed time and difference in reported microjoules
        echo -n "$time"
        for i in "${!zones[@]}"; do
            watts=`echo "(${zones[$i]} - ${old_zones[$i]}) / (($time - $old_time) * 10^6)" | bc -l`
            echo -e -n "\t$watts"
        done
        echo -e -n "\n"
    else
        # first loop iteration.  we don't have enough data yet to report
        # watts, but we use this opportunity to print a header row
        echo -n "# <timestamp>"
        for i in "${!zones[@]}"; do
            echo -e -n "\t<$i>"
        done
        echo -e -n "\n"
    fi

    # save old time and microjoules
    old_time=$time
    for i in "${!zones[@]}"; do
        old_zones[$i]=${zones[$i]}
    done

    # one second interval
    sleep 1
done

