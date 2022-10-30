#!/bin/sh

set -e
export LC_ALL=C

num_bytes="${1:-4096}"

grep -aoi '[a-z]' /dev/urandom | paste -sd '' |
    head -c "$num_bytes" |
    if [ -t 1 ]; then cat - | paste -sd '\n'; else cat -; fi
