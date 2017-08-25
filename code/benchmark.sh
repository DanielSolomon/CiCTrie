#!/bin/bash

num_of_threads=(1 2 4 8 16 32 44 66 88)

if [[ $# < 3 ]]
then
    echo "Usage: $0 <benchmark-name> <iterations> <args-for-CiCTrie>"
    exit 1
fi

echo "benchmark $1"
bench_dir="benchmark_results/$1"
mkdir -p "$bench_dir"
shift

iterations="$1"
shift

for i in ${num_of_threads[@]}
do
    echo "#threads: $i"
    sed -i.bak -r "s/NUM_OF_THREADS=([0-9]+)/NUM_OF_THREADS=$i/g" makefile
    make
    thread_dir="$bench_dir/$i"
    mkdir -p "$thread_dir"
    for (( j=1; j<=iterations; j++))
    do
        echo "iteration: $j"
        ./CiCTrie $@ > "$thread_dir/result_$j.txt"
    done
    make clean
done
