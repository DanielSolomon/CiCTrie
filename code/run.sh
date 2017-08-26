#!/bin/bash
ulimit -c unlimited
echo "Compiling..."
make
count=1
while true
do
    echo "Running $count:..."
    count=$((count + 1))
    ./CiCTrie scripts/sample.bin > output.txt && continue
    echo "Failed!" 
    exit 1
done
