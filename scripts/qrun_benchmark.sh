#!/bin/bash

# This is a specialized bash script designed for executing benchmarks using 'qrun'.
# It's a valuable tool for efficient benchmarking in our cluster environment.


if [ $# -eq 0 ]; then
  echo "Usage: $0 <benchmark_name>"
  exit 1
fi

set -e

. scripts/build.sh --source-only
build_benchmark

cd build/benchmark
cp /home/mpi/serial_job.sh .
sed -i '$d' serial_job.sh
echo "export OMP_NESTED=true" >> serial_job.sh
echo "./PPQSortBenchmark --benchmark_time_unit=ms" >> serial_job.sh
echo "unset OMP_NESTED" >> serial_job.sh

mv serial_job.sh "$1".sh
qrun 20c 1 pdp_serial "$1".sh
