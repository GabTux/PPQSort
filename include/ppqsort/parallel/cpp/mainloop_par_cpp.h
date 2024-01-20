#pragma once

#include <iostream>

#include "thread_pool.h"
#include "../../mainloop.h"

namespace ppqsort::impl {

        namespace cpp {
        template <typename RandomIt, typename Compare,
                  bool branchless,
                  typename T = typename std::iterator_traits<RandomIt>::value_type,
                  typename diff_t = typename std::iterator_traits<RandomIt>::difference_type>
        inline void par_loop(RandomIt begin, RandomIt end, Compare comp,
                             diff_t bad_allowed, diff_t seq_thr, int threads,
                             ThreadPool<>& thread_pool,
                             bool leftmost = true) {
            constexpr int insertion_threshold = branchless ?
                                              parameters::insertion_threshold_primitive
                                              : parameters::insertion_threshold;
            while (true) {
                diff_t size = end - begin;

                if (size < insertion_threshold) {
                    if (leftmost)
                        _insertion_sort(begin, end, comp);
                    else
                        _insertion_sort_unguarded(begin, end, comp);
                    return;
                }

                choose_pivot<branchless>(begin, end, size, comp);

                // pivot is the same as previous pivot
                // put same elements to the left, and we do not have to recurse
                if (!leftmost && !comp(*(begin-1), *begin)) {
                    begin = _partition_to_left(begin, end, comp) + 1;
                    continue;
                }

                std::pair<RandomIt, bool> part_result;
                part_result = branchless ? partition_right_branchless(begin, end, comp)
                                           : _partition_to_right(begin, end, comp);
                RandomIt pivot_pos = part_result.first;
                bool already_partitioned = part_result.second;

                diff_t l_size = pivot_pos - begin;
                diff_t r_size = end - (pivot_pos + 1);

                if (already_partitioned) {
                    bool left = false;
                    bool right = false;
                    if (l_size > insertion_threshold)
                        left = _partial_insertion_sort(begin, pivot_pos, comp);
                    if (r_size > insertion_threshold)
                        right = _partial_insertion_sort_unguarded(pivot_pos + 1, end, comp);
                    if (left && right) {
                        return;
                    } else if (left) {
                        begin = ++pivot_pos;
                        continue;
                    } else if (right) {
                        end = pivot_pos;
                        continue;
                    }
                }

                bool highly_unbalanced = l_size < size / parameters::partition_ratio ||
                                         r_size < size / parameters::partition_ratio;

                if (highly_unbalanced) {
                    // switch to heapsort
                    if (--bad_allowed == 0) {
                        std::make_heap(begin, end, comp);
                        std::sort_heap(begin, end, comp);
                        return;
                    }
                    // partition unbalanced, shuffle elements
                    _deterministic_shuffle(begin, end, l_size, r_size, pivot_pos, insertion_threshold);
                }

                if (size > seq_thr) {
                    thread_pool.push_task([begin, pivot_pos, comp, bad_allowed, seq_thr, threads, &thread_pool, leftmost] {
                        par_loop<RandomIt, Compare, branchless>(begin, pivot_pos, comp, bad_allowed, seq_thr, threads, thread_pool, leftmost);
                    });
                    //par_loop<RandomIt, Compare, branchless>(begin, pivot_pos, comp, bad_allowed, seq_thr, threads, thread_pool, leftmost);
                } else {
                    seq_loop<RandomIt, Compare, branchless>(begin, pivot_pos, comp, bad_allowed, leftmost);
                }
                leftmost = false;
                begin = ++pivot_pos;
            }
        }
    }


    template <bool Force_branchless = false,
             typename RandomIt,
             typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>,
             bool Branchless = use_branchless<typename std::iterator_traits<RandomIt>::value_type, Compare>::value>
    void par_ppqsort(RandomIt begin, RandomIt end, Compare comp = Compare()) {
        if (begin == end)
            return;
        constexpr bool branchless = Force_branchless || Branchless;
        int threads = static_cast<int>(std::thread::hardware_concurrency());
        int seq_thr = (end - begin + 1) / threads / parameters::par_thr_div;


        cpp::ThreadPool<> threadpool;
        cpp::par_loop<RandomIt, Compare, branchless>(begin, end, comp,
                                                     log2(end - begin),
                                                     seq_thr, threads, threadpool);
    }
}