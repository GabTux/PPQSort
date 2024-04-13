[![Standalone](https://github.com/GabTux/ppqsort_suite/actions/workflows/standalone.yml/badge.svg)](https://github.com/GabTux/ppqsort_suite/actions/workflows/standalone.yml)
[![Install](https://github.com/GabTux/ppqsort_suite/actions/workflows/install.yml/badge.svg)](https://github.com/GabTux/ppqsort_suite/actions/workflows/install.yml)
[![Tests](https://github.com/GabTux/ppqsort_suite/actions/workflows/tests.yml/badge.svg)](https://github.com/GabTux/ppqsort_suite/actions/workflows/tests.yml)
[![codecov](https://codecov.io/gh/GabTux/ppqsort_suite/graph/badge.svg?token=K7UVUZ4N1N)](https://codecov.io/gh/GabTux/ppqsort_suite)

# PPQSort (Parallel Pattern QuickSort)
Parallel Pattern Quicksort (PPQSort) is a **efficient implementation of parallel quicksort algorithm**, written by using **only** the C++20 features without using third party libraries (such as Intel TBB). PPQSort draws inspiration from [pdqsort](https://github.com/orlp/pdqsort), [BlockQuicksort](https://github.com/weissan/BlockQuicksort) and [cpp11sort](https://gitlab.com/daniel.langr/cpp11sort) and adds some further optimizations.

* **Focus on ease of use:** Using only C++20 features, header only implementation, user-friendly API.
* **Comprehensive test suite:** Ensures correctness and robustness through extensive testing. 
* **Benchmarks shows great performance:** Achieves impressive sorting times on various platforms.

# Usage
Add to existing CMake project using [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake):
```CMake
include(cmake/CPM.cmake)
CPMAddPackage(
NAME PPQSort
GITHUB_REPOSITORY GabTux/PPQSort
VERSION 1.0.1
)
target_link_libraries(${PROJECT_NAME} PPQSort::PPQSort)
```
Alternatively use FetchContent or just checkout the repository and add the include directory to the linker flags.

PPQSort has similiar API as std::sort, you can use `ppqsort::execution::<policy>` policies to specify how the sort should run.
```cpp
// run parallel
ppqsort::sort(ppqsort::execution::par, input.begin(), input.end());

// Specify number of threads
ppqsort::sort(ppqsort::execution::par, input.begin(), input.end(), 16);

// provide custom comparator
ppqsort::sort(ppqsort::execution::par, input.begin(), input.end(), cmp);

// force branchless variant
ppqsort::sort(ppqsort::execution::par_force_branchless, input_str.begin(), input_str.end(), cmp);
```

PPQSort will by default use C++ threads, but if you prefer, you can link it with OpenMP and it will use OpenMP as a parallel backend. However you can still enforce C++ threads parallel backend even if linked with OpenMP:
```cpp
#define FORCE_CPP
#include <ppqsort/ppqsort.h>
// ... rest of the code ...
```

# Benchmark



# Running Tests and Benchmarks
Bash script for running or building specific components:
```bash
@cf-frontend03-prod: ̃$ scripts/build.sh all
...
@cf-frontend03-prod: ̃$ scripts/run.sh standalone
...
```
Note that the benchmark's CMake file will by default download sparse matrices (around 26GB).

# Implementation
A detailed research paper exploring PPQSort's design, implementation, and performance evaluation will be available soon.
