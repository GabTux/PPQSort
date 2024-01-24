#pragma once

#include "partition_par.h"
#include "../../partition.h"
#include "../../partition_branchless.h"

namespace ppqsort::impl::cpp {

    template <typename RandomIt, typename Compare,
        typename T = typename std::iterator_traits<RandomIt>::value_type,
        typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void process_blocks_branchless(const RandomIt & g_begin, const Compare & comp,
                   const diff_t& g_size, std::atomic<diff_t>& g_distance,
                   std::atomic<diff_t>& g_first_offset, std::atomic<diff_t>& g_last_offset,
                   const int & block_size, const T& pivot,
                   std::atomic<unsigned char>& g_already_partitioned,
                   std::atomic<int>& g_dirty_blocks_left, std::atomic<int>& g_dirty_blocks_right,
                   std::unique_ptr<std::atomic<bool>[]>& g_reserved_left,
                   std::unique_ptr<std::atomic<bool>[]>& g_reserved_right,
                   std::barrier<>& barrier, const int t_my_id) {
            // iterators for given block
            diff_t t_left = (block_size * t_my_id) + 1;
            diff_t t_right = (g_size-1) - block_size * t_my_id;
            diff_t t_left_start = t_left;

            // block borders
            diff_t t_left_end = t_left + block_size - 1;
            diff_t t_right_start = t_right - block_size + 1;
            diff_t t_right_end = t_right;

            diff_t t_size = 0;
            bool t_already_partitioned = true;

            alignas(parameters::cacheline_size) unsigned short t_offsets_l[parameters::buffer_size] = {0};
            alignas(parameters::cacheline_size) unsigned short t_offsets_r[parameters::buffer_size] = {0};

            static_assert(std::numeric_limits<std::remove_all_extents<decltype(t_offsets_l)>::type>::max() >
                          parameters::buffer_size,
                          "buffer_size is bigger than type for buffer can hold. This will overflow");


            diff_t t_count_l, t_count_r, t_start_l, t_start_r;
            t_count_l = t_count_r = t_start_l = t_start_r = 0;

            // each thread will do the first two blocks
            solve_left_block(g_begin, t_left, t_left_start, t_left_end, t_offsets_l, t_count_l, pivot, comp);
            solve_right_block(g_begin, t_right, t_right_start, t_right_end, t_offsets_r, t_count_r, pivot, comp);
            diff_t swapped_count = swap_offsets(g_begin + t_left_start, g_begin + t_right_end,
                                                t_offsets_l + t_start_l, t_offsets_r + t_start_r,
                                                t_count_l, t_count_r, t_already_partitioned);
            t_count_l -= swapped_count; t_start_l += swapped_count;
            t_count_r -= swapped_count; t_start_r += swapped_count;

            while (true) {
                // get new blocks if needed or end partitioning
                if (t_count_l == 0) {
                    t_start_l = 0;
                    if (!get_new_block<left>(t_size, t_left, t_left_end,
                                             g_distance, g_first_offset, block_size)) {
                        break;
                    }
                    solve_left_block(g_begin, t_left, t_left_start, t_left_end,
                                     t_offsets_l, t_count_l, pivot, comp);
                }

                if (t_count_r == 0) {
                    t_start_r = 0;
                    if (!get_new_block<right>(t_size, t_right, t_right_start,
                                              g_distance, g_last_offset, block_size)) {
                        break;
                    }
                    solve_right_block(g_begin, t_right, t_right_start, t_right_end,
                                      t_offsets_r, t_count_r, pivot, comp);
                }

                // swap elements according to the recorded offsets
                swapped_count = swap_offsets(g_begin + t_left_start, g_begin + t_right_end,
                                          t_offsets_l + t_start_l, t_offsets_r + t_start_r,
                                         t_count_l, t_count_r, t_already_partitioned);
                t_count_l -= swapped_count; t_start_l += swapped_count;
                t_count_r -= swapped_count; t_start_r += swapped_count;
            }

            g_already_partitioned &= t_already_partitioned;

            t_right = t_right_start + t_count_r;
            t_left = t_left_end - t_count_l;
            // swap dirty blocks to the middle
            swap_dirty_blocks(g_begin, t_left, t_right,
                              t_left_end, t_right_start,
                              g_dirty_blocks_left, g_dirty_blocks_right,
                              g_first_offset, g_last_offset, g_reserved_left, g_reserved_right,
                              block_size, barrier, t_my_id);
    }

    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type,
             typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline std::pair<RandomIt, bool> partition_right_branchless_par(const RandomIt g_begin,
                                                                    const RandomIt g_end, Compare comp,
                                                                    const int thread_count) {
        const diff_t g_size = g_end - g_begin;
        constexpr int block_size = parameters::buffer_size;

        // at least 2 blocks per each thread
        if (g_size - 1 < 2 * block_size * thread_count)
            return partition_right_branchless(g_begin, g_end, comp);

        const T pivot = std::move(*g_begin);
        // reserve first blocks for each thread
        // first blocks will be assigned statically
        std::atomic<diff_t> g_first_offset = 1 + block_size * thread_count;
        std::atomic<diff_t> g_last_offset = g_size - 1 - block_size * thread_count;
        std::atomic<diff_t> g_distance = g_size - 1 - block_size * thread_count * 2;

        // counters for dirty blocks
        std::atomic<int> g_dirty_blocks_left = 0;
        std::atomic<int> g_dirty_blocks_right = 0;

        std::unique_ptr<std::atomic<bool>[]> g_reserved_left(new std::atomic<bool>[thread_count]{false});
        std::unique_ptr<std::atomic<bool>[]> g_reserved_right(new std::atomic<bool>[thread_count]{false});
        std::atomic<unsigned char> g_already_partitioned = 1;

        std::barrier<> barrier(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back(&process_blocks_branchless<RandomIt, Compare>, std::ref(g_begin), std::ref(comp),
                                 std::ref(g_size), std::ref(g_distance), std::ref(g_first_offset),
                                 std::ref(g_last_offset), std::ref(block_size), std::ref(pivot),
                                 std::ref(g_already_partitioned), std::ref(g_dirty_blocks_left),
                                 std::ref(g_dirty_blocks_right), std::ref(g_reserved_left),
                                 std::ref(g_reserved_right), std::ref(barrier), i);
        }
        for(auto & th: threads)
            th.join();

        int first_offset = g_first_offset.load(std::memory_order_seq_cst);
        int last_offset = g_last_offset.load(std::memory_order_seq_cst);
        bool already_partitioned = static_cast<bool>(g_already_partitioned.load(std::memory_order_seq_cst));
        return seq_cleanup<false>(g_begin, pivot, comp, first_offset, last_offset, already_partitioned);
    }
};