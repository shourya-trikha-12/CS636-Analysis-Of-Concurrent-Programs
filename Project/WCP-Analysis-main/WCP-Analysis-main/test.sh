#!/bin/bash

mkdir -p ./output

# run norace tests
for ((i = 1; i <= 7; i++)); do
    echo "##### TEST norace$i.cpp Started #####"
    g++ test/norace$i.cpp -o a.out
    ./run.sh ./a.out -o output/norace$i.out
    echo "##### TEST norace$i.cpp Ended #####"
    echo ""
done

# run racy tests
for ((i = 1; i <= 8; i++)); do
    echo "##### TEST racy$i.cpp Started #####"
    g++ test/racy$i.cpp -o a.out
    ./run.sh ./a.out -o output/racy$i.out
    echo "##### TEST racy$i.cpp Ended #####"
    echo ""
done