#include <benchmark/benchmark.h>
#include <boost/sort/sort.hpp>
#include <vector>
#include <ppqsort.h>
#include <chrono>
#include <parallel/algorithm>
#include <oneapi/tbb.h>
#include <poolstl/poolstl.hpp>
#undef THRUST_DEVICE_SYSTEM
#define THRUST_DEVICE_SYSTEM THRUST_DEVICE_SYSTEM_CPP
#undef THRUST_HOST_SYSTEM
#define THRUST_HOST_SYSTEM_TBB
#include <thrust/sort.h>
#include <thrust/execution_policy.h>
#include <cpp11sort.h>
#include <mpqsort.h>
#include <ips4o.hpp>

#include "fixtures.h"

#define str(X) #X

#define register_templated_benchmark(fixture, name, sort_signature)                                                    \
{                                                                                                                      \
    for (auto _ : state) {                                                                                             \
        prepare();                                                                                                     \
        auto start = std::chrono::high_resolution_clock::now();                                                        \
        sort_signature;                                                                                                \
        auto end = std::chrono::high_resolution_clock::now();                                                          \
        deallocate();                                                                                                  \
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);                 \
        state.SetIterationTime(elapsed_seconds.count());                                                               \
    }                                                                                                                  \
}                                                                                                                      \
BENCHMARK_REGISTER_F(fixture, name)                                                                                    \
                    ->UseManualTime()                                                                                  \
                    ->Name(str(name));

#define register_benchmark_default(sort_signature, sort_name, bench_type, type, bench_setup)                           \
BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture,                                                                 \
                            BM_##sort_name##_##bench_type##_##type##_##bench_setup,                                    \
                            type)(benchmark::State& state)                                                             \
register_templated_benchmark(bench_type##VectorFixture,                                                                \
                             BM_##sort_name##_##bench_type##_##type##_##bench_setup, sort_signature)

#define register_benchmark_size(sort_signature, sort_name, bench_type, type, bench_setup, size)                        \
BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture,                                                                 \
                            BM_##sort_name##_##bench_type##_##type##_##bench_setup,                                    \
                            type, size)(benchmark::State& state)                                                       \
register_templated_benchmark(bench_type##VectorFixture,                                                                \
                             BM_##sort_name##_##bench_type##_##type##_##bench_setup, sort_signature)

#define register_benchmark_range(sort_signature, sort_name, bench_type, type, bench_setup, min, max)                   \
BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture,                                                                 \
                            BM_##sort_name##_##bench_type##_##type##_##bench_setup,                                    \
                            type, ELEMENTS_VECTOR_DEFAULT, min, max)(benchmark::State& state)                          \
register_templated_benchmark(bench_type##VectorFixture,                                                                \
                             BM_##sort_name##_##bench_type##_##type##_##bench_setup, sort_signature)

#define register_string_benchmark(sort_signature, sort_name, bench_type, bench_setup, prepended)                       \
BENCHMARK_TEMPLATE_DEFINE_F(bench_type##StringVectorFixture,                                                           \
                            BM_##sort_name##_##bench_type##_string_##bench_setup,                                      \
                            prepended)(benchmark::State& state)                                                        \
register_templated_benchmark(bench_type##StringVectorFixture,                                                          \
                             BM_##sort_name##_##bench_type##_string_##bench_setup, sort_signature)

#define register_matrix_benchmark(sort_signature, sort_name, bench_type, filename, type)                               \
BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture,                                                                 \
BM_##sort_name##_##bench_type##_##filename,                                                                            \
filename, type)(benchmark::State& state)                                                                               \
register_templated_benchmark(bench_type##VectorFixture,                                                                \
BM_##sort_name##_##bench_type##_##filename, sort_signature)



#define default_benchmark(sort_signature, sort_name)                                 \
register_benchmark_default(sort_signature, sort_name, Random, short, default);       \
register_benchmark_default(sort_signature, sort_name, Random, int, default);         \
register_benchmark_default(sort_signature, sort_name, Random, double, default);      \
register_benchmark_default(sort_signature, sort_name, Ascending, short, default);    \
register_benchmark_default(sort_signature, sort_name, Ascending, int, default);      \
register_benchmark_default(sort_signature, sort_name, Ascending, double, default);   \
register_benchmark_default(sort_signature, sort_name, Descending, short, default);   \
register_benchmark_default(sort_signature, sort_name, Descending, int, default);     \
register_benchmark_default(sort_signature, sort_name, Descending, double, default);  \
register_benchmark_default(sort_signature, sort_name, OrganPipe, short, default);    \
register_benchmark_default(sort_signature, sort_name, OrganPipe, int, default);      \
register_benchmark_default(sort_signature, sort_name, OrganPipe, double, default);   \
register_benchmark_default(sort_signature, sort_name, Rotated, short, default);      \
register_benchmark_default(sort_signature, sort_name, Rotated, int, default);        \
register_benchmark_default(sort_signature, sort_name, Rotated, double, default);     \
register_benchmark_default(sort_signature, sort_name, Heap, short, default);         \
register_benchmark_default(sort_signature, sort_name, Heap, int, default);           \
register_benchmark_default(sort_signature, sort_name, Heap, double, default);

#define small_range_benchmark(sort_signature, sort_name)                                                               \
register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##1, 0, 0);                             \
register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##3, 0, 2);                             \
register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##10, 0, 9);                            \
register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##100, 0, 99);                          \
register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##10000, 0, int(1e4-1));                \
register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##100000, 0, int(1e5-1));

#define small_size_benchmark(sort_signature, sort_name)                                                                \
register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##10000, 10000);                          \
register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##100000, 100000);                        \
register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##1000000, 1000000);                      \
register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##10000000, 10000000);                    \
register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##50000000, 50000000);                    \
register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##100000000, 100000000);

#define string_benchmark(sort_signature, sort_name)                                                                    \
register_string_benchmark(sort_signature, sort_name, Random, prepended, true)                                          \
register_string_benchmark(sort_signature, sort_name, Ascending, prepended, true)                                       \
register_string_benchmark(sort_signature, sort_name, Descending, prepended, true)                                      \
register_string_benchmark(sort_signature, sort_name, OrganPipe, prepended, true)                                       \
register_string_benchmark(sort_signature, sort_name, Rotated, prepended, true)                                         \
register_string_benchmark(sort_signature, sort_name, Heap, prepended, true)


#define matrix_benchmark(sort_signature, sort_name)                                                                    \
register_matrix_benchmark(sort_signature, sort_name, SparseMatrix, af_shell10, false)                                  \
register_matrix_benchmark(sort_signature, sort_name, SparseMatrix, cage15, false)                                      \
register_matrix_benchmark(sort_signature, sort_name, SparseMatrix, fem_hifreq_circuit, true)

#define complete_benchmark_set(sort_signature, sort_name)                           \
default_benchmark(sort_signature, sort_name)                                        \
register_benchmark_default(sort_signature, sort_name, Adversary, int, default);     \
small_range_benchmark(sort_signature, sort_name)                                    \
small_size_benchmark(sort_signature, sort_name)                                     \
string_benchmark(sort_signature, sort_name)                                         \
matrix_benchmark(sort_signature, sort_name)

#define ppqsort_parameters_tuning                                                                                                       \
register_benchmark_size(ppqsort::sort(ppqsort::execution::par, data.begin(), data.end()), ppqsort_par, Random, short, tuning, 1000000000);  \
register_benchmark_size(ppqsort::sort(ppqsort::execution::par, data.begin(), data.end()), ppqsort_par, Random, int, tuning, 1000000000);    \
register_benchmark_size(ppqsort::sort(ppqsort::execution::par, data.begin(), data.end()), ppqsort_par, Random, double, tuning, 1000000000);

#define ppqsort_tuning_partition \
string_benchmark(ppqsort::sort(ppqsort::execution::par, data.begin(), data.end()), ppqsort_par_classic) \
string_benchmark(ppqsort::sort(ppqsort::execution::par_force_branchless, data.begin(), data.end()), ppqsort_par_branchless)

// sets used during implementation to find best configuration
//ppqsort_tuning_partition
//ppqsort_parameters_tuning

// complete sets to compare sorts against each other
complete_benchmark_set(ppqsort::sort(ppqsort::execution::par, data_.begin(), data_.end()), ppqsort_par);
complete_benchmark_set(__gnu_parallel::sort(data_.begin(), data_.end(), __gnu_parallel::balanced_quicksort_tag()), bqs);
complete_benchmark_set(boost::sort::block_indirect_sort(data_.begin(), data_.end()), block_indirect_sort);
complete_benchmark_set(std::sort(std::execution::par, data_.begin(), data_.end()), std_sort_par);
complete_benchmark_set(std::sort(poolstl::par, data_.begin(), data_.end()), poolstl_sort_par);
complete_benchmark_set(tbb::parallel_sort(data_.begin(), data_.end()), tbb_parallel_sort);
complete_benchmark_set(thrust::sort(thrust::host, data_.begin(), data_.end()), thrust_device_sort);
//yet buggy: complete_benchmark_set(cpp11sort::sort(data_.begin(), data_.end()), cpp11sort_par);
complete_benchmark_set(ips4o::parallel::sort(data_.begin(), data_.end()), ips4o);