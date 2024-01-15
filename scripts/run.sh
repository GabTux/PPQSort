#!/bin/bash

# This script provides easy approach to building and running different components of the PPQSort project.

if [ $# -eq 0 ]; then
  echo "Usage: $0 <command>"
  exit 1
fi

function build_standalone() {
    cmake -S standalone -B build/standalone -DCMAKE_BUILD_TYPE=Release
    cmake --build build/standalone
}

function build_test() {
    cmake -S test -B build/test -DENABLE_COMPILER_WARNINGS=ON
    cmake --build build/test -j"$(nproc)"
}

function build_benchmark() {
    cmake -S benchmark -B build/benchmark -DCMAKE_BUILD_TYPE=Release
    cmake --build build/benchmark --config Release -j"$(nproc)"
}

function build_all() {
    cmake -S all -B build
    cmake --build build -j"$(nproc)"
}

function main() {
    if [ "$1" == "all" ]; then
        build_all
        ./build/standalone/PPQSort
        ./build/test/PPQSortTests
    elif [ "$1" == "standalone" ]; then
        build_standalone
        ./build/standalone/PPQSort
    elif [ "$1" == "tests" ]; then
        build_test
        ./build/test/PPQSortTests
    elif [ "$1" == "benchmark" ]; then
       build_benchmark
       build/benchmark/PPQSortBenchmark --benchmark_time_unit=ms
    elif [ "$1" == "clean" ]; then
           rm -rf build
    else
        echo "Invalid command. Command: all, standalone, tests, benchmark, clean"
        exit 1
    fi
}

if [ "${1}" != "--source-only" ]; then
    main "${@}"
fi
