#define FORCE_CPP

#include <benchmark/benchmark.h>
// #include <boost/sort/sort.hpp>
#include <chrono>
#include <parallel/algorithm>
#include <ppqsort.h>
#include <vector>
// #include <oneapi/tbb.h>
// #include <poolstl/poolstl.hpp>
#undef THRUST_DEVICE_SYSTEM
#define THRUST_DEVICE_SYSTEM THRUST_DEVICE_SYSTEM_CPP
#undef THRUST_HOST_SYSTEM
#define THRUST_HOST_SYSTEM_OMP
// #include <thrust/sort.h>
// #include <thrust/system/omp/execution_policy.h>
// #include <cpp11sort.h>
// #include <mpqsort.h>
// #include <aqsort.h>
// #include <ips4o.hpp>
//
#include <papi.h>
#include "fixtures.h"

// macros for registering benchmarks, to avoid code duplication
// Inspired by https://github.com/voronond/MPQsort/blob/master/benchmark/source/mpqsort.cpp

#define str(X) #X

void handle_error(int retval) { printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval)); exit(1); }

#define NUM_EVENTS 2
int EventSet = PAPI_NULL;
int retval;
long long values[NUM_EVENTS];
const char *event_names_L1[NUM_EVENTS] = {"MEM_LOAD_RETIRED:L1_HIT", "MEM_LOAD_RETIRED:L1_MISS"};
const char *event_names_L2[NUM_EVENTS] = {"MEM_LOAD_RETIRED:L2_HIT", "MEM_LOAD_RETIRED:L2_MISS"};
const char *event_names_L3[NUM_EVENTS] = {"MEM_LOAD_RETIRED:L3_HIT", "MEM_LOAD_RETIRED:L3_MISS"};


#define register_templated_benchmark(fixture, name, sort_signature, prepare_code)                             \
    {                                                                                                         \
        retval = PAPI_library_init(PAPI_VER_CURRENT);                                                         \
        if ((retval != PAPI_VER_CURRENT && retval > 0) || (retval < 0)) handle_error(retval);                 \
        retval = PAPI_create_eventset(&EventSet);                                                             \
        if (retval != PAPI_OK) handle_error(retval);                                                          \
        for (int i = 0; i < NUM_EVENTS; i++) {                                                                \
            retval = PAPI_add_named_event(EventSet, event_names_L1[i]);                                       \
            if (retval != PAPI_OK) {                                                                          \
                printf("Could not add event %s\n", event_names_L1[i]);                                        \
                handle_error(retval);                                                                         \
            }                                                                                                 \
        }                                                                                                     \
        for (auto _: state) {                                                                                 \
            prepare();                                                                                        \
            prepare_code;                                                                                     \
            auto start = std::chrono::high_resolution_clock::now();                                           \
            retval = PAPI_start(EventSet);                                                                    \
            if (retval != PAPI_OK) handle_error(retval);                                                      \
            sort_signature;                                                                                   \
            retval = PAPI_stop(EventSet, values);                                                             \
            if (retval != PAPI_OK) handle_error(retval);                                                      \
            auto end = std::chrono::high_resolution_clock::now();                                             \
            deallocate();                                                                                     \
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);    \
            state.SetIterationTime(elapsed_seconds.count());                                                  \
            long long data_hits = values[0];                                                                  \
            long long data_misses = values[1];                                                                \
            long long total_data_accesses = data_hits + data_misses;                                          \
            double data_miss_rate = 0.0;                                                                      \
            if (total_data_accesses > 0) data_miss_rate = (double) data_misses / total_data_accesses * 100.0; \
            printf("Data Cache Hits: %lld\n", data_hits);                                                     \
            printf("Data Cache Misses: %lld\n", data_misses);                                                 \
            printf("Total Data Cache Accesses: %lld\n", total_data_accesses);                                 \
            printf("Data Cache Miss Rate: %.2f%%\n\n", data_miss_rate);                                       \
        }                                                                                                     \
    }                                                                                                         \
    BENCHMARK_REGISTER_F(fixture, name)->UseManualTime()->Name(str(name));

#define register_benchmark_default(sort_signature, sort_name, bench_type, type, bench_setup, prepare_code)          \
    BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture, BM_##sort_name##_##bench_type##_##type##_##bench_setup,  \
                                type)                                                                               \
    (benchmark::State & state) register_templated_benchmark(bench_type##VectorFixture,                              \
                                                            BM_##sort_name##_##bench_type##_##type##_##bench_setup, \
                                                            sort_signature, prepare_code)

#define register_benchmark_size(sort_signature, sort_name, bench_type, type, bench_setup, size, prepare_code)       \
    BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture, BM_##sort_name##_##bench_type##_##type##_##bench_setup,  \
                                type, size)                                                                         \
    (benchmark::State & state) register_templated_benchmark(bench_type##VectorFixture,                              \
                                                            BM_##sort_name##_##bench_type##_##type##_##bench_setup, \
                                                            sort_signature, prepare_code)

#define register_benchmark_range(sort_signature, sort_name, bench_type, type, bench_setup, min, max, prepare_code)  \
    BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture, BM_##sort_name##_##bench_type##_##type##_##bench_setup,  \
                                type, ELEMENTS_VECTOR_DEFAULT, min, max)                                            \
    (benchmark::State & state) register_templated_benchmark(bench_type##VectorFixture,                              \
                                                            BM_##sort_name##_##bench_type##_##type##_##bench_setup, \
                                                            sort_signature, prepare_code)

#define register_string_benchmark(sort_signature, sort_name, bench_type, bench_setup, prepended, prepare_code)         \
    BENCHMARK_TEMPLATE_DEFINE_F(bench_type##StringVectorFixture, BM_##sort_name##_##bench_type##_string_##bench_setup, \
                                prepended)                                                                             \
    (benchmark::State & state) register_templated_benchmark(bench_type##StringVectorFixture,                           \
                                                            BM_##sort_name##_##bench_type##_string_##bench_setup,      \
                                                            sort_signature, prepare_code)

#define register_matrix_benchmark(sort_signature, sort_name, bench_type, filename, type, prepare_code)                 \
    BENCHMARK_TEMPLATE_DEFINE_F(bench_type##VectorFixture, BM_##sort_name##_##bench_type##_##filename, filename, type) \
    (benchmark::State & state) register_templated_benchmark(                                                           \
            bench_type##VectorFixture, BM_##sort_name##_##bench_type##_##filename, sort_signature, prepare_code)

#define default_benchmark(sort_signature, sort_name, prepare_code)                                    \
    register_benchmark_default(sort_signature, sort_name, Random, short, default, prepare_code);      \
    register_benchmark_default(sort_signature, sort_name, Random, int, default, prepare_code);        \
    register_benchmark_default(sort_signature, sort_name, Random, double, default, prepare_code);     \
    register_benchmark_default(sort_signature, sort_name, Ascending, short, default, prepare_code);   \
    register_benchmark_default(sort_signature, sort_name, Ascending, int, default, prepare_code);     \
    register_benchmark_default(sort_signature, sort_name, Ascending, double, default, prepare_code);  \
    register_benchmark_default(sort_signature, sort_name, Descending, short, default, prepare_code);  \
    register_benchmark_default(sort_signature, sort_name, Descending, int, default, prepare_code);    \
    register_benchmark_default(sort_signature, sort_name, Descending, double, default, prepare_code); \
    register_benchmark_default(sort_signature, sort_name, OrganPipe, short, default, prepare_code);   \
    register_benchmark_default(sort_signature, sort_name, OrganPipe, int, default, prepare_code);     \
    register_benchmark_default(sort_signature, sort_name, OrganPipe, double, default, prepare_code);  \
    register_benchmark_default(sort_signature, sort_name, Rotated, short, default, prepare_code);     \
    register_benchmark_default(sort_signature, sort_name, Rotated, int, default, prepare_code);       \
    register_benchmark_default(sort_signature, sort_name, Rotated, double, default, prepare_code);    \
    register_benchmark_default(sort_signature, sort_name, Heap, short, default, prepare_code);        \
    register_benchmark_default(sort_signature, sort_name, Heap, int, default, prepare_code);          \
    register_benchmark_default(sort_signature, sort_name, Heap, double, default, prepare_code);

#define small_range_benchmark(sort_signature, sort_name, prepare_code)                                          \
    register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##1, 0, 0, prepare_code);    \
    register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##3, 0, 2, prepare_code);    \
    register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##10, 0, 9, prepare_code);   \
    register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##100, 0, 99, prepare_code); \
    register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##1e4, 0, int(1e4 - 1),      \
                             prepare_code);                                                                     \
    register_benchmark_range(sort_signature, sort_name, Random, int, small_range##_##1e5, 0, int(1e5 - 1),      \
                             prepare_code);

#define small_size_benchmark(sort_signature, sort_name, prepare_code)                                            \
    register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##1e4, int(1e4), prepare_code); \
    register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##1e5, int(1e5), prepare_code); \
    register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##1e6, int(1e6), prepare_code); \
    register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##1e7, int(1e7), prepare_code); \
    register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##5e7, int(5e7), prepare_code); \
    register_benchmark_size(sort_signature, sort_name, Random, int, small_size##_##1e8, int(1e8), prepare_code);

#define string_benchmark(sort_signature, sort_name, prepare_code)                                                   \
    register_string_benchmark(sort_signature, sort_name, Random, prepended, true, prepare_code)                     \
            register_string_benchmark(sort_signature, sort_name, Ascending, prepended, true, prepare_code)          \
                    register_string_benchmark(sort_signature, sort_name, Descending, prepended, true, prepare_code) \
                            register_string_benchmark(sort_signature, sort_name, OrganPipe, prepended, true,        \
                                                      prepare_code)                                                 \
                                    register_string_benchmark(sort_signature, sort_name, Rotated, prepended, true,  \
                                                              prepare_code)                                         \
                                            register_string_benchmark(sort_signature, sort_name, Heap, prepended,   \
                                                                      true, prepare_code)


#define matrix_benchmark_AoS(sort_signature, sort_name, prepare_code)                                                 \
    register_matrix_benchmark(sort_signature, sort_name, SparseMatrixAoS, mawi_201512020330, matrix_element_t<int>,   \
                              prepare_code)                                                                           \
            register_matrix_benchmark(sort_signature, sort_name, SparseMatrixAoS, uk_2005, matrix_element_t<uint8_t>, \
                                      prepare_code)                                                                   \
                    register_matrix_benchmark(sort_signature, sort_name, SparseMatrixAoS, dielFilterV3clx,            \
                                              matrix_element_t<std::complex<double>>, prepare_code)                   \
                            register_matrix_benchmark(sort_signature, sort_name, SparseMatrixAoS, Queen_4147,         \
                                                      matrix_element_t<double>, prepare_code)


#define matrix_benchmark_SoA(sort_signature, sort_name, prepare_code)                                                 \
    register_matrix_benchmark(sort_signature, sort_name, SparseMatrixSoA, mawi_201512020330, int, prepare_code)       \
            register_matrix_benchmark(sort_signature, sort_name, SparseMatrixSoA, uk_2005, uint8_t, prepare_code)     \
                    register_matrix_benchmark(sort_signature, sort_name, SparseMatrixSoA, dielFilterV3clx,            \
                                              std::complex<double>, prepare_code)                                     \
                            register_matrix_benchmark(sort_signature, sort_name, SparseMatrixSoA, Queen_4147, double, \
                                                      prepare_code)

#define complete_benchmark_set(sort_signature, sort_name, prepare_code)               \
    default_benchmark(sort_signature, sort_name, prepare_code)                        \
            small_range_benchmark(sort_signature, sort_name, prepare_code)            \
                    small_size_benchmark(sort_signature, sort_name, prepare_code)     \
                            string_benchmark(sort_signature, sort_name, prepare_code) \
                                    matrix_benchmark_AoS(sort_signature, sort_name, prepare_code)

#define complete_benchmark_set_aq(sort_signature, sort_name, prepare_code)                                        \
    default_benchmark(sort_signature, sort_name, prepare_code)                                                    \
            small_range_benchmark(sort_signature, sort_name, prepare_code)                                        \
                    small_size_benchmark(sort_signature, sort_name, prepare_code)                                 \
                            string_benchmark(sort_signature, sort_name, prepare_code) matrix_benchmark_SoA(       \
                                    sort_signature, sort_name,                                                    \
                                    auto cmp =                                                                    \
                                            [&](std::size_t i, std::size_t j) {                                   \
                                                if (rows_[i] < rows_[j]) return true;                             \
                                                if ((rows_[i] == rows_[j]) && (cols_[i] < cols_[j])) return true; \
                                                return false;                                                     \
                                            };                                                                    \
                                    auto sw =                                                                     \
                                            [&](std::size_t i, std::size_t j) {                                   \
                                                std::swap(rows_[i], rows_[j]);                                    \
                                                std::swap(cols_[i], cols_[j]);                                    \
                                                std::swap(data_[i], data_[j]);                                    \
                                            };)

#define ppqsort_parameters_tuning                                                                                    \
    register_benchmark_size(ppqsort::sort(ppqsort::execution::par, data_.begin(), data_.end()), ppqsort_par, Random, \
                            short, tuning, 1000000000, "");                                                          \
    register_benchmark_size(ppqsort::sort(ppqsort::execution::par, data_.begin(), data_.end()), ppqsort_par, Random, \
                            int, tuning, 1000000000, "");                                                            \
    register_benchmark_size(ppqsort::sort(ppqsort::execution::par, data_.begin(), data_.end()), ppqsort_par, Random, \
                            double, tuning, 1000000000, "");

#define ppqsort_tuning_partition                                                                                  \
    string_benchmark(ppqsort::sort(ppqsort::execution::par, data_.begin(), data_.end()), ppqsort_par_classic, "") \
            string_benchmark(ppqsort::sort(ppqsort::execution::par_force_branchless, data_.begin(), data_.end()), \
                             ppqsort_par_branchless, "")


#define cache_benchmark(sort_signature, sort_name, prepare_code) \
    register_benchmark_size(sort_signature, sort_name##_cache, Random, int, cache, 1000000000, "");

// Used during implementation to find best configuration
// ppqsort_tuning_partition
// ppqsort_parameters_tuning


cache_benchmark(ppqsort::sort(ppqsort::execution::seq, data_.begin(), data_.end()), ppqsort_par_cpp, "");

// complete sets to compare sorts against each other
/*complete_benchmark_set(ppqsort::sort(ppqsort::execution::par, data_.begin(), data_.end()), ppqsort_par, "");
complete_benchmark_set(__gnu_parallel::sort(data_.begin(), data_.end(), __gnu_parallel::balanced_quicksort_tag()), bqs,
                       "");
complete_benchmark_set(__gnu_parallel::sort(data_.begin(), data_.end(), __gnu_parallel::quicksort_tag()), gnu_qs, "");
complete_benchmark_set(boost::sort::block_indirect_sort(data_.begin(), data_.end()), block_indirect_sort, "");
complete_benchmark_set(std::sort(poolstl::par, data_.begin(), data_.end()), poolstl_sort_par, "");
complete_benchmark_set(tbb::parallel_sort(data_.begin(), data_.end()), tbb_parallel_sort, "");
complete_benchmark_set(thrust::sort(thrust::omp::par, data_.begin(), data_.end()), thrust_omp_sort, "");
complete_benchmark_set(cpp11sort::sort(data_.begin(), data_.end()), cpp11sort_par, "");
complete_benchmark_set_aq(
        aqsort::sort(data_.size(), &cmp, &sw), aqsort,
        auto sw = [&](std::size_t i, std::size_t j) { std::swap(data_[i], data_[j]); };
        auto cmp = [&](std::size_t i, std::size_t j) { return data_[i] < data_[j]; });
complete_benchmark_set(mpqsort::sort(mpqsort::execution::par, data_.begin(), data_.end()), mpqsort_par, "");
complete_benchmark_set(ips4o::parallel::sort(data_.begin(), data_.end()), ips4o, "");
*/