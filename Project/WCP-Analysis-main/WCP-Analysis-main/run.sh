#!/bin/bash

#Set Pin root directory here
DIR=~/pin-external-3.31-98869-gfa6f126a8-gcc-linux

if [ $# -eq 0 ]; then
    echo "Usage: $0 <executable> [-o <output>] [-t <maxThreads>]"
    exit 1
elif [ $# -eq 1 ]; then
    exec=$1
    args=""
else
    exec=$1
    shift
    args="$@"
fi

cmd="$DIR/pin -t $DIR/source/tools/WCPPinTool/obj-intel64/WCPPinTool.so $args -- $exec"
eval "$cmd"