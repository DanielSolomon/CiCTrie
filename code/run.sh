#!/bin/bash
ulimit -c unlimited
while true; do
    echo "Running..."
    ./CiCTrie scripts/sample.bin > output.txt || echo "Failed!" && exit 1
done
