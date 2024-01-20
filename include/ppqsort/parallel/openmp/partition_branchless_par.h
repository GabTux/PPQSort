#pragma once

namespace ppqsort::impl::par {

    template<class RandomIt, typename Offset, class Compare,
            typename T = typename std::iterator_traits<RandomIt>::value_type,
            typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void solve_left_block(const RandomIt & g_begin,
                                 diff_t & t_left, diff_t & t_left_start, diff_t & t_left_end,
                                 Offset * t_offsets_l, diff_t & t_count_l, T & g_pivot, Compare comp) {
        // find the first element from block start, which is greater or equal to pivot
        while (t_left <= t_left_end && comp(g_begin[t_left], g_pivot)) {
            ++t_left;
        }
        const diff_t elem_rem_l = t_left_end - t_left + 1;
        t_left_start = t_left;
        populate_block<side::left>(g_begin, t_left, g_pivot, t_offsets_l, t_count_l, comp, elem_rem_l);
    }

    template<class RandomIt, typename Offset, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type,
             typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline void solve_right_block(const RandomIt & g_begin,
                                  diff_t & t_right, diff_t & t_right_start, diff_t & t_right_end,
                                 Offset * t_offsets_r, diff_t & t_count_r, T & g_pivot, Compare comp) {
        // find first elem from block g_end, which is less than pivot
        while (t_right >= t_right_start && !comp(g_begin[t_right], g_pivot)) {
            --t_right;
        }
        const diff_t elem_rem_r = t_right - t_right_start + 1;
        t_right_end = t_right;
        populate_block<side::right>(g_begin, t_right, g_pivot, t_offsets_r, t_count_r, comp, elem_rem_r);
    }


    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type,
             typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
    inline std::pair<RandomIt, bool> partition_right_branchless_par(const RandomIt g_begin,
                                                                    const RandomIt g_end, Compare comp,
                                                                    const int thread_count)
    {
        const diff_t g_size = g_end - g_begin;
        constexpr int block_size = parameters::buffer_size;

        diff_t g_distance = g_size - 1;

        // at least 2 blocks per each thread
        if (g_distance < 2 * block_size * thread_count)
            return partition_right_branchless(g_begin, g_end, comp);

        T g_pivot = std::move(*g_begin);
        diff_t g_first_offset = 1;
        diff_t g_last_offset = g_size - 1;

        // counters for dirty blocks
        int g_dirty_blocks_left = 0;
        int g_dirty_blocks_right = 0;

        std::unique_ptr<bool[]> g_reserved_left(new bool[thread_count]);
        std::unique_ptr<bool[]> g_reserved_right(new bool[thread_count]);
        bool g_already_partitioned = true;
        for (int i = 0; i < thread_count; ++i) {
            g_reserved_left[i] = g_reserved_right[i] = false;
        }

        // reserve first blocks for each thread
        g_first_offset += block_size * thread_count;
        g_last_offset -= block_size * thread_count;
        g_distance -= block_size * thread_count * 2;

        // prefix g_ is for shared variables
        // prefix t_ is for private variables

        // each thread will get two blocks (from start and from end)
        // it will record offsets for swapping and then swap
        // it is efficient due to avoiding branch mispredictions
        #pragma omp parallel num_threads(thread_count)
        {
            // each thread will get first two blocks without collisions
            const int t_my_id = omp_get_thread_num();

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
            solve_left_block(g_begin, t_left, t_left_start, t_left_end, t_offsets_l, t_count_l, g_pivot, comp);
            solve_right_block(g_begin, t_right, t_right_start, t_right_end, t_offsets_r, t_count_r, g_pivot, comp);
            diff_t swapped_count = swap_offsets(g_begin + t_left_start, g_begin + t_right_end,
                                                t_offsets_l + t_start_l, t_offsets_r + t_start_r,
                                                t_count_l, t_count_r, t_already_partitioned);
            t_count_l -= swapped_count; t_start_l += swapped_count;
            t_count_r -= swapped_count; t_start_r += swapped_count;

            while (true) {
                // get new blocks if needed or end partitioning
                if (t_count_l == 0) {
                    t_start_l = 0;
                    if (!get_new_block<side::left>(t_size, t_left, t_left_end,
                                                   g_distance, g_first_offset, block_size))
                        break;
                    else {
                        solve_left_block(g_begin, t_left, t_left_start, t_left_end,
                                         t_offsets_l, t_count_l, g_pivot, comp);
                    }
                }

                if (t_count_r == 0) {
                    t_start_r = 0;
                    if (!get_new_block<side::right>(t_size, t_right, t_right_start,
                                                    g_distance, g_last_offset, block_size))
                        break;
                    else {
                        solve_right_block(g_begin, t_right, t_right_start, t_right_end,
                                          t_offsets_r, t_count_r, g_pivot, comp);
                    }
                }

                // swap elements according to the recorded offsets
                swapped_count = swap_offsets(g_begin + t_left_start, g_begin + t_right_end,
                                                    t_offsets_l + t_start_l, t_offsets_r + t_start_r,
                                                    t_count_l, t_count_r, t_already_partitioned);
                t_count_l -= swapped_count; t_start_l += swapped_count;
                t_count_r -= swapped_count; t_start_r += swapped_count;
            }
            #pragma omp atomic update
            g_already_partitioned &= t_already_partitioned;

            t_right = t_right_start + t_count_r;
            t_left = t_left_end - t_count_l;
            // swap dirty blocks to the middle
            swap_dirty_blocks(g_begin, t_left, t_right,
                              t_left_end, t_right_start,
                              g_dirty_blocks_left, g_dirty_blocks_right,
                              g_first_offset, g_last_offset, g_reserved_left, g_reserved_right,
                              block_size);
        }  // End of parallel segment

        // sequential cleanup of dirty blocks in the middle
        // call partition branchless inner loop
        // do sequential cleanup of dirty segment
        RandomIt final_first = g_begin + g_first_offset - 1;
        RandomIt final_last = g_begin + g_last_offset + 1;
        while (final_first < final_last && comp(*++final_first, g_pivot));
        while (final_first < final_last && !comp(*--final_last, g_pivot));
        bool cleanup_already_partitioned = final_first >= final_last;

        if (!cleanup_already_partitioned)
            partition_branchless_core(final_first, final_last, g_pivot, comp);

        g_already_partitioned &= cleanup_already_partitioned;

        // Put the g_pivot in the correct place
        RandomIt pivot_pos = --final_first;
        if (g_begin != pivot_pos)
            *g_begin = std::move(*pivot_pos);
        *pivot_pos = std::move(g_pivot);
        return std::make_pair(pivot_pos, g_already_partitioned);
    }
}