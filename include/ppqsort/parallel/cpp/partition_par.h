#pragma once

#include <barrier>
#include <thread>

#include "thread_pool.h"
#include "../../partition.h"


namespace ppqsort::impl::cpp {

    template <side s, typename diff_t>
    inline bool get_new_block(diff_t& t_size, diff_t& t_iter,
                              diff_t& t_block_bound, std::atomic<diff_t>& g_distance,
                              std::atomic<diff_t>& g_offset, const int& block_size) {
        t_size = std::atomic_fetch_sub_explicit(&g_distance, block_size, std::memory_order_acq_rel);
        if (t_size < block_size) {
            // last block not aligned, no new blocks available
            g_distance.fetch_add(block_size, std::memory_order_acq_rel);
            return false;
        }
        // else get new block
        if constexpr (s == left) {
            t_iter = std::atomic_fetch_add_explicit(&g_offset, block_size, std::memory_order_acq_rel);
            t_block_bound = t_iter + (block_size - 1);
        } else {
            t_iter = std::atomic_fetch_sub_explicit(&g_offset, block_size, std::memory_order_acq_rel);
            t_block_bound = t_iter - (block_size - 1);
        }
        return true;
    }


    template <side s, typename RandomIt, typename diff_t>
    inline void swap_blocks(const std::atomic<int>& g_dirty_blocks_side, const RandomIt& g_begin,
                            const diff_t& t_old, const diff_t& t_block_bound,
                            std::unique_ptr<std::atomic<bool>[]>& reserved, const int& block_size)
    {
        const int t_dirty_blocks = g_dirty_blocks_side.load(std::memory_order_acquire);
        diff_t swap_start = -1;
        for (int i = 0; i < t_dirty_blocks; ++i) {
            // find clean block in dirty segment
            bool t_reserve_success = false;
            if (reserved[i].load(std::memory_order_acquire) == false) {
                bool expected = false;
                t_reserve_success = reserved[i].compare_exchange_strong(expected, true,
                                                                        std::memory_order_release,
                                                                        std::memory_order_relaxed);
            }
            if (t_reserve_success) {
                if constexpr (s == left)
                    swap_start = t_old - (i + 1) * block_size;
                else
                    swap_start = t_old + i * block_size + 1;
                break;
            }
        }
        RandomIt block_start, block_end;
        if constexpr (s == left) {
            block_start = g_begin + t_block_bound - (block_size - 1);
            block_end = g_begin + t_block_bound + 1;
        } else {
            block_start = g_begin + t_block_bound;
            block_end = g_begin + t_block_bound + block_size;
        }
        // swap our dirty block with found clean block
        std::swap_ranges(block_start, block_end, g_begin + swap_start);
    }


    template <typename RandomIt, typename diff_t>
    inline void swap_dirty_blocks(const RandomIt& g_begin, const diff_t & t_left, const diff_t & t_right,
                                  const diff_t & t_left_end, const diff_t & t_right_start,
                                  std::atomic<int> & g_dirty_blocks_left, std::atomic<int> & g_dirty_blocks_right,
                                  std::atomic<diff_t> & g_first_offset, std::atomic<diff_t> & g_last_offset,
                                  std::unique_ptr<std::atomic<bool>[]>& g_reserved_left,
                                  std::unique_ptr<std::atomic<bool>[]>& g_reserved_right,
                                  const int& block_size, std::barrier<>& barrier,
                                  const int& t_my_id)
    {
        const bool t_dirty_left = t_left <= t_left_end;
        const bool t_dirty_right = t_right >= t_right_start;

        // count and clean dirty blocks
        // maximum of dirty blocks is thread count
        if (t_dirty_left) {
            g_dirty_blocks_left.fetch_add(1, std::memory_order_relaxed);
        }
        if (t_dirty_right) {
            g_dirty_blocks_right.fetch_add(1, std::memory_order_relaxed);
        }

        // wait for all threads
        barrier.arrive_and_wait();

        // create new pointers which will separate clean and dirty blocks
        const diff_t t_first_old = g_first_offset;
        const diff_t t_first_clean = g_first_offset - g_dirty_blocks_left.load(std::memory_order_acquire) * block_size;
        const diff_t t_last_old = g_last_offset;
        const diff_t t_last_clean = g_last_offset + g_dirty_blocks_right.load(std::memory_order_acquire) * block_size;

        // reserve spots for dirty blocks
        // g_reserved_<side> == 1 is for dirty blocks that will be in the middle
        if (t_dirty_left && t_left_end >= t_first_clean) {
            // Block is dirty and in dirty segment --> reserve spot
            g_reserved_left[(t_first_old - (t_left_end + 1)) / block_size].store(true, std::memory_order_release);
        }
        if (t_dirty_right && t_right_start <= t_last_clean) {
            g_reserved_right[(t_right_start - 1 - t_last_old) / block_size].store(true, std::memory_order_release);
        }
        // otherwise do not reserve if is block clean and in dirty segment
        // by default, blocks are not reserved (g_reserved_<side>[b] = 0)

        // barrier resets itself and can be reused
        barrier.arrive_and_wait();

        // swap reserved
        if (t_dirty_left && t_left_end < t_first_clean) {
            // dirty block in clean segment --> swap
            swap_blocks<left>(g_dirty_blocks_left, g_begin,
                              t_first_old, t_left_end, g_reserved_left, block_size);
        }
        if (t_dirty_right && t_right_start > t_last_clean) {
            swap_blocks<right>(g_dirty_blocks_right, g_begin, t_last_old,
                    t_right_start, g_reserved_right, block_size);
        }

        // update global offsets so it separates clean and dirty segment
        if (t_my_id == 0) {
            g_first_offset.store(t_first_clean, std::memory_order_release);
            g_last_offset.store(t_last_clean, std::memory_order_release);
        }
    }



    template <typename RandomIt, typename Compare,
        typename T = typename std::iterator_traits<RandomIt>::value_type,
        typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void process_blocks(const RandomIt & g_begin, const Compare & comp,
                               const diff_t& g_size, std::atomic<diff_t>& g_distance,
                               std::atomic<diff_t>& g_first_offset, std::atomic<diff_t>& g_last_offset,
                               const int & block_size, const T& pivot,
                               std::atomic<unsigned char>& g_already_partitioned,
                               std::atomic<int>& g_dirty_blocks_left, std::atomic<int>& g_dirty_blocks_right,
                               std::unique_ptr<std::atomic<bool>[]>& g_reserved_left,
                               std::unique_ptr<std::atomic<bool>[]>& g_reserved_right,
                               std::barrier<>& barrier, const int t_my_id, std::latch& part_done) {
        // each thread will get first two blocks without collisions
        // iterators for given block
        diff_t t_left = (block_size * t_my_id) + 1;
        diff_t t_right = (g_size-1) - block_size * t_my_id;

        // block borders
        diff_t t_left_end = t_left + block_size - 1;
        diff_t t_right_start = t_right - block_size + 1;

        diff_t t_size = 0;
        int t_already_partitioned = 1;
        while (true) {
            PRINT_ATOMIC_CPP("ID "+std::to_string(t_my_id)+": <"+std::to_string(t_left)+", "
                +std::to_string(t_left_end)+">, <"+std::to_string(t_right_start)+", "
                +std::to_string(t_right)+">");
            // get new blocks if needed or end partitioning
            if (t_left > t_left_end) {
                if (!get_new_block<left>(t_size, t_left, t_left_end,
                                         g_distance, g_first_offset, block_size))
                    break;
            }
            if (t_right < t_right_start) {
                if (!get_new_block<right>(t_size, t_right, t_right_start,
                                          g_distance, g_last_offset, block_size))
                    break;
            }

            // find first element from block start, which is greater or equal to pivot
            // we must guard it, because in block nothing is guaranteed
            while (t_left <= t_left_end && comp(g_begin[t_left], pivot)) {
                ++t_left;
            }
            // find first elem from block g_end, which is less than pivot
            while (t_right >= t_right_start && !comp(g_begin[t_right], pivot)) {
                --t_right;
            }

            // do swaps
            while (t_left < t_right) {
                if (t_left > t_left_end || t_right < t_right_start)
                    break;
                std::iter_swap(g_begin + t_left, g_begin + t_right);
                t_already_partitioned = 0;
                while (++t_left <= t_left_end && comp(g_begin[t_left], pivot));
                while (--t_right >= t_right_start && !comp(g_begin[t_right], pivot));
            }
        }
        g_already_partitioned.fetch_and(t_already_partitioned, std::memory_order_release);

        // swaps dirty blocks from clean segment to dirty segment
        swap_dirty_blocks(g_begin, t_left, t_right, t_left_end, t_right_start,
                          g_dirty_blocks_left, g_dirty_blocks_right,
                          g_first_offset, g_last_offset, g_reserved_left, g_reserved_right,
                          block_size, barrier, t_my_id);
        part_done.count_down();
    }

    template <typename RandomIt, typename Compare,
        typename T = typename std::iterator_traits<RandomIt>::value_type,
        typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline std::pair<RandomIt, bool> partition_to_right_par(const RandomIt g_begin,
                                                             const RandomIt g_end,
                                                             Compare comp,
                                                             const int thread_count,
                                                             ThreadPool<>& thread_pool) {
        constexpr int block_size = parameters::par_partition_block_size;
        const diff_t g_size = g_end - g_begin;

        // at least 2 blocks for each thread
        if (g_size - 1 < 2 * block_size * thread_count)
            return partition_to_right(g_begin, g_end, comp);

        const T pivot = std::move(*g_begin);
        // reserve first blocks for each thread
        // first blocks will be assigned statically
        std::atomic<diff_t> g_first_offset(1 + block_size * thread_count);
        std::atomic<diff_t> g_last_offset(g_size - 1 - block_size * thread_count);
        std::atomic<diff_t> g_distance(g_size - 1 - block_size * thread_count * 2);

        // counters for dirty blocks
        std::atomic<int> g_dirty_blocks_left{};
        std::atomic<int> g_dirty_blocks_right{};

        // helper arrays for cleaning blocks
        std::unique_ptr<std::atomic<bool>[]> g_reserved_left(new std::atomic<bool>[thread_count]{});
        std::unique_ptr<std::atomic<bool>[]> g_reserved_right(new std::atomic<bool>[thread_count]{});
        std::atomic<unsigned char> g_already_partitioned(1); // because atomic bool does not support &= operator

        std::barrier<> barrier(thread_count);
        std::latch part_done(thread_count);
        for (int i = 0; i < thread_count; ++i) {
            thread_pool.push_task([g_begin, comp, g_size, &g_distance, &g_first_offset, &g_last_offset,
                                   block_size, pivot, &g_already_partitioned, &g_dirty_blocks_left,
                                   &g_dirty_blocks_right, &g_reserved_left, &g_reserved_right, &barrier,
                                   i, &part_done] {
                process_blocks<RandomIt, Compare>(g_begin, comp, g_size, g_distance, g_first_offset,
                    g_last_offset, block_size, pivot, g_already_partitioned, g_dirty_blocks_left,
                    g_dirty_blocks_right, g_reserved_left, g_reserved_right, barrier, i, part_done); });
        }
        part_done.wait();

        int first_offset = g_first_offset.load(std::memory_order_acquire);
        int last_offset = g_last_offset.load(std::memory_order_acquire);
        bool already_partitioned = static_cast<bool>(g_already_partitioned.load(std::memory_order_acquire));
        return seq_cleanup<false>(g_begin, pivot, comp, first_offset, last_offset, already_partitioned);
    }
}