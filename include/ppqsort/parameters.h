#pragma once

#include <bit>
#include <execution>


#define TIME_MEASURE

#ifndef NDEBUG
    #include <bitset>
    #include <iostream>
    #include <syncstream>
    #define TO_S(val) std::to_string(val)
    #define PRINT_ITERATORS(lp, rp, msg)                               \
        std::cout << msg << ": ";                                      \
        for (auto i = lp; i != (rp - 1); ++i) std::cout << *i << ", "; \
        std::cout << *(rp - 1) << std::endl;
    #define PRINT_ATOMIC_OMP(id, str) \
        _Pragma("omp critical") { std::cout << "my_id: " + std::to_string(id) + " " + str << std::endl << std::flush; }
    #define DO_SINGLE_OMP(func) \
        _Pragma("omp barrier") _Pragma("omp single") { func; }
    #define PRINT_VAL(str, val) (std::cout << str << ": " << std::to_string(val) << std::endl)
    #define PRINT_BIN(str, val) (std::cout << str << ": " << std::bitset<64>(val) << std::endl)

    #define PRINT_ATOMIC_CPP(str) \
        std::osyncstream(std::cout) << str << std::endl;
#else
    #define TO_S(val)
    #define PRINT_ITERATORS(lp, rp, msg)
    #define PRINT_ATOMIC_OMP(id, str)
    #define PRINT_VAL(str, val)
    #define PRINT_BIN(str, val)
    #define DO_SINGLE_OMP(func)
    #define PRINT_ATOMIC_CPP(str)
#endif

#ifdef TIME_MEASURE
    #include <chrono>
    #define MEASURE_START(name) \
        const std::chrono::steady_clock::time_point name = std::chrono::steady_clock::now();
    #define MEASURE_END(name, msg)                                                                             \
        const std::chrono::steady_clock::time_point end_##name = std::chrono::steady_clock::now();                  \
        std::cout << msg << " " << std::chrono::duration_cast<std::chrono::milliseconds>(end_##name - name).count() \
                  << " ms" << std::endl
#else
    #define MEASURE_START(name)
    #define MEASURE_END(name_start, name_end, msg)
#endif

namespace ppqsort::parameters {
    #ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    #else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr std::size_t hardware_constructive_interference_size = 64;
    #endif

    constexpr int insertion_threshold_primitive = 32;
    constexpr int insertion_threshold = 12;
    constexpr int partial_insertion_threshold = 8;
    constexpr int median_threshold = 128;
    constexpr int partition_ratio = 8;
    constexpr int cacheline_size = hardware_constructive_interference_size;
    // 1536 elements from input blocks --> 24,5KB for doubles
    // offsets arrays 6 KB --> 30,5KB total should fit in 32 KB L1 cache
    constexpr int buffer_size = 24 * cacheline_size;
    using buffer_type = unsigned short;
    static_assert(std::numeric_limits<buffer_type>::max() > buffer_size,
                  "buffer_size is bigger than type for buffer can hold. This will overflow");
    constexpr int par_thr_div = 10;
    constexpr int par_partition_block_size = 1 << 14;
    constexpr int seq_threshold = 1 << 18;
} // namespace ppqsort::parameters

namespace ppqsort::execution {
    class sequenced_policy {};
    class parallel_policy {};
    class sequenced_policy_force_branchless {};
    class parallel_policy_force_branchless {};

    inline constexpr sequenced_policy seq{};
    inline constexpr parallel_policy par{};
    inline constexpr sequenced_policy_force_branchless seq_force_branchless{};
    inline constexpr parallel_policy_force_branchless par_force_branchless{};

    template<typename T, typename U>
    inline constexpr bool _is_same_decay_v = std::is_same_v<std::decay_t<T>, std::decay_t<U>>;
} // namespace ppqsort::execution

namespace ppqsort::impl {
    enum side { left, right };

    inline int log2(const unsigned long long value) {
        // This is floor(log2(x))+1
        return std::bit_width(value);
    }
} // namespace ppqsort::impl
