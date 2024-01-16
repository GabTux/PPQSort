#pragma once

#include <cstdint>
#include <execution>

//#define MEASURE
#define TIME_MEASURE

#ifndef NDEBUG
#include <bitset>
   #define TO_S(val)                               \
       std::to_string(val)
   #define PRINT_ITERS(lp, rp, msg)                \
       std::cout << msg << ": ";                   \
       for (auto i = lp; i != (rp-1); ++i)         \
           std::cout << *i << ", ";                \
       std::cout << *(rp-1) << std::endl
   #define PRINT(str)                              \
       std::cout << str << std::endl
   #define PRINT_ATOMIC(id, str)                   \
        _Pragma("omp critical") {                  \
            std::cout << "my_id: "+std::to_string(id)+" "+str << std::endl << std::flush; \
        }
    #define DO_SINGLE(func)                      \
        _Pragma("omp barrier")                   \
        _Pragma("omp single") {                  \
            func; \
        }
   #define PRINT_VAL(str, val)                     \
       (std::cout << str << ": " <<                \
       std::to_string(val) << std::endl)
   #define PRINT_BIN(str, val)                     \
            (std::cout << str << ": " <<           \
       std::bitset<64>(val) << std::endl)

#else
    #    define TO_S(val)
    #    define PRINT_ITERS(lp, rp, msg)
    #    define PRINT(str)
    #    define PRINT_ATOMIC(id, str)
    #    define PRINT_VAL(str, val)
    #    define PRINT_BIN(str, val)
    #    define DO_SINGLE(func)
#endif


#ifdef MEASURE
size_t NUM_SWAP = 0;
size_t NUM_COMP = 0;
   #    define MEASURE_INIT() \
       NUM_SWAP = 0;      \
       NUM_COMP = 0
   #    define MEASURE_SWAP() ++NUM_SWAP
   #    define MEASURE_SWAP_N(n) NUM_SWAP += n
   #    define MEASURE_COMP() ++NUM_COMP
   #    define MEASURE_COMP_N(n) NUM_COMP += n
   #    define MEASURE_RESULTS(msg)  \
       std::cout << msg << ": "; \
       std::cout << "SWAP: " << NUM_SWAP << " COMP: " << NUM_COMP << "\n"
#else
    #    define MEASURE_INIT()
    #    define MEASURE_SWAP()
    #    define MEASURE_SWAP_N(n)
    #    define MEASURE_COMP()
    #    define MEASURE_COMP_N(n)
    #    define MEASURE_RESULTS(msg)
#endif

#ifdef TIME_MEASURE
    #include <chrono>
    #    define TIME_MEASURE_START(name)                                                              \
       std::chrono::steady_clock::time_point name = std::chrono::steady_clock::now();
    #    define TIME_MEASURE_END(name, msg)                                                           \
       std::chrono::steady_clock::time_point end_##name = std::chrono::steady_clock::now();             \
       std::cout << msg << " "                                                                   \
                 << std::chrono::duration_cast<std::chrono::milliseconds>(end_##name - name)            \
                        .count()                                                                 \
                 << " ms" << std::endl
#else
#    define TIME_MEASURE_START(name)
   #    define TIME_MEASURE_END(name_start, name_end, msg)
#endif

namespace ppqsort::parameters {
    constexpr int insertion_threshold_primitive = 32;
    constexpr int insertion_threshold = 12;
    constexpr int partial_insertion_threshold = 8;
    constexpr int median_threshold = 128;
    constexpr int partition_ratio = 8;
    constexpr int cacheline_size = 64;
    // 1536 elements from input blocks --> 24,5KB for doubles
    // offsets arrays 6 KB --> 30,5KB total should fit in 32 KB L1 cache
    constexpr int buffer_size = 24 * cacheline_size;
    constexpr int par_thr_div = 10;
    constexpr int par_partition_block_size = 1 << 14;
}

namespace ppqsort::execution {

    class sequenced_policy {};
    class parallel_policy {};
    class sequenced_policy_force_branchless {};
    class parallel_policy_force_branchless {};

    inline constexpr sequenced_policy seq{};
    inline constexpr parallel_policy par{};
    inline constexpr sequenced_policy_force_branchless seq_force_branchless{};
    inline constexpr parallel_policy_force_branchless par_force_branchless{};

    template <typename T, typename U> inline constexpr bool _is_same_decay_v
            = std::is_same<std::decay_t<T>, std::decay_t<U>>::value;
}

namespace ppqsort::impl {
    enum side {
        left,
        right
    };
}
