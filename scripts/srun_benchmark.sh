#!/bin/bash

# This is a specialized bash script designed for executing benchmarks using 'srun'.


if [ $# -eq 0 ]; then
  echo "Usage: $0 <benchmark_name>"
  exit 1
fi

set -e

. scripts/build.sh --source-only
build_benchmark

JOB_FILE="serial_job_arm.sh"

cd build/benchmark
cp ~/"$JOB_FILE" .
sed -i 's/REPLACE_HERE/'"$1"'/g' $JOB_FILE
sbatch $JOB_FILE
