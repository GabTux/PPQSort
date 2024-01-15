#pragma once

namespace ppqsort::impl {

    template<class RandomIt, typename Offset,
            typename T = std::iterator_traits<RandomIt>::value_type,
            typename diff_t = std::iterator_traits<RandomIt>::difference_type>
    inline void swap_offsets_core(RandomIt first, RandomIt last,
                                  Offset* offsets_l, Offset* offsets_r,
                                  diff_t & num, bool swap)
    {
        if (swap) {
            // this case is needed for descending distribution
            for (int i = 0; i < num; ++i)
                std::iter_swap(first + offsets_l[i], last - offsets_r[i]);
        } else {
            // cyclic permutation instead of swapping
            // faster, just one tmp element
            RandomIt left = first + offsets_l[0];
            RandomIt right = last - offsets_r[0];
            T elem = std::move(*left);
            *left = std::move(*right);
            for (int i = 1; i < num; ++i) {
                left = first + offsets_l[i];
                *right = std::move(*left);
                right = last - offsets_r[i];
                *left = std::move(*right);
            }
            *right = std::move(elem);
        }
    }

    template<class RandomIt, typename Offset,
            typename T = std::iterator_traits<RandomIt>::value_type,
            typename diff_t = std::iterator_traits<RandomIt>::difference_type>
    inline diff_t swap_offsets(RandomIt first, RandomIt last,
                               Offset* offsets_l, Offset* offsets_r,
                               diff_t & num_l, diff_t & num_r) {
        diff_t num = std::min(num_l, num_r);
        if (num == 0)
            return 0;
        swap_offsets_core(first, last, offsets_l, offsets_r, num, num_l == num_r);
        return num;
    }

    template<class RandomIt, typename Offset,
            typename T = std::iterator_traits<RandomIt>::value_type,
            typename diff_t = std::iterator_traits<RandomIt>::difference_type>
    inline diff_t swap_offsets(RandomIt first, RandomIt last,
                               Offset* offsets_l, Offset* offsets_r,
                               diff_t & num_l, diff_t & num_r,
                               bool & t_already_partitioned) {
        diff_t num = std::min(num_l, num_r);
        if (num == 0)
            return 0;
        t_already_partitioned = false;
        swap_offsets_core(first, last, offsets_l, offsets_r, num, num_l == num_r);
        return num;
    }


    template <side s, typename RandomIt, typename Offset, class Compare,
              typename T = std::iterator_traits<RandomIt>::value_type,
            typename diff_t = std::iterator_traits<RandomIt>::difference_type>
    inline void populate_block(RandomIt & begin, diff_t & offset, T pivot, Offset* offsets, diff_t& count,
                               Compare comp, const int & block_size) {
        for (int i = 0; i < block_size; ++i) {
            offsets[count] = i;
            if constexpr (s == side::left)
                count += !comp(begin[offset++], pivot);
            else
                count += comp(begin[offset--], pivot);
        }
    }

    template <side s, typename RandomIt, typename Offset, class Compare,
              typename T = std::iterator_traits<RandomIt>::value_type,
              typename diff_t = std::iterator_traits<RandomIt>::difference_type>
    inline void populate_block(RandomIt & it, T & pivot, Offset* offsets, diff_t& count,
                               Compare comp, const int & block_size) {
        for (int i = 0; i < block_size; ++i) {
            offsets[count] = i;
            if constexpr (s == side::left)
                count += !comp(*it++, pivot);
            else
                count += comp(*it--, pivot);
        }
    }

    template <typename RandomIt, typename Offset, class Compare,
            typename T = std::iterator_traits<RandomIt>::value_type,
            typename diff_t = std::iterator_traits<RandomIt>::difference_type>
    inline void partition_branchless_cleanup(RandomIt & first, RandomIt & last,
                                      RandomIt & block_l, RandomIt & block_r,
                                      Offset * offsets_l, Offset * offsets_r,
                                      diff_t & start_l, diff_t & start_r,
                                      diff_t & count_l, diff_t & count_r,
                                      T & pivot, Compare comp,
                                      const int & block_size) {
        diff_t size = last - first + 1;
        diff_t l_size, r_size;
        while (first <= last) {
            if (count_l == 0) {
                start_l = 0;
                block_l = first;
                l_size = (size > block_size) ? block_size : size;
                populate_block<side::left>(first, pivot, offsets_l, count_l, comp, l_size);
                size = last - first + 1;
            }
            if (count_r == 0) {
                start_r = 0;
                block_r = last;
                r_size = (size > block_size) ? block_size : size;
                populate_block<side::right>(last, pivot, offsets_r, count_r, comp, r_size);
                size = last - first + 1;
            }
            diff_t swapped_count = swap_offsets(block_l, block_r,
                                                offsets_l + start_l, offsets_r + start_r,
                                                count_l, count_r);
            count_l -= swapped_count; count_r -= swapped_count;
            start_l += swapped_count;  start_r += swapped_count;
        }

        // One block can be dirty
        // Process remaining elements in dirty block
        if (count_l) {
            offsets_l += start_l;
            while (count_l--)
                std::iter_swap(block_l + offsets_l[count_l], last--);
            first = ++last;
        }
        if (count_r) {
            offsets_r += start_r;
            while (count_r--)
                std::iter_swap(block_r - offsets_r[count_r], first++);
        }
    }

    template <typename RandomIt, class Compare,
            typename T = std::iterator_traits<RandomIt>::value_type,
            typename diff_t = std::iterator_traits<RandomIt>::difference_type>
    inline void partition_branchless_core(RandomIt & first, RandomIt & last, T & pivot, Compare comp) {
        constexpr const int block_size = parameters::buffer_size;

        // allign offsets buffers to fill whole cachelines for speed
        // offsets holds information about elements, which needs to be swapped
        // i.e.: offsets_l[1] == 5 <-- elem on (first + 5) needs to be swapped
        alignas(parameters::cacheline_size) unsigned short offsets_l[parameters::buffer_size];
        alignas(parameters::cacheline_size) unsigned short offsets_r[parameters::buffer_size];

        static_assert(std::numeric_limits<std::remove_all_extents<decltype(offsets_l)>::type>::max() >
                      parameters::buffer_size,
                      "buffer_size is bigger than type for buffer can hold. This will overflow");

        RandomIt block_l = first;
        RandomIt block_r = last;
        diff_t count_l, count_r, start_l, start_r;
        count_l = count_r = start_l = start_r = 0;

        diff_t size = last - first + 1;
        diff_t min_size = 2 * block_size;
        while (size >= min_size) {
            // if we swapped all elements from block, get new block
            if (count_l == 0) {
                start_l = 0;
                block_l = first;
                populate_block<side::left>(first, pivot, offsets_l, count_l, comp, block_size);
                size = last - first + 1;
            }
            if (count_r == 0) {
                start_r = 0;
                block_r = last;
                populate_block<side::right>(last, pivot, offsets_r, count_r, comp, block_size);
                size = last - first + 1;
            }
            // swap elements according to the recorded offsets
            // counts hold information about how much elements needs to be swapped
            diff_t swapped_count = swap_offsets(block_l, block_r,
                                                offsets_l + start_l, offsets_r + start_r,
                                                count_l, count_r);
            count_l -= swapped_count; count_r -= swapped_count;
            start_r += swapped_count; start_l += swapped_count;
        }
        partition_branchless_cleanup(first, last, block_l, block_r,
                                     offsets_l, offsets_r,
                                     start_l, start_r, count_l, count_r,
                                     pivot, comp, block_size);
    }

    template<class RandomIt, class Compare, typename T = std::iterator_traits<RandomIt>::value_type>
    inline std::pair<RandomIt, bool> partition_right_branchless(RandomIt begin, RandomIt end, Compare comp) {
        T pivot = std::move(*begin);
        RandomIt first = begin;
        RandomIt last = end;

        // Find the first element greater than or equal to the pivot
        while (comp(*++first, pivot));

        // Find the first element less than the pivot
        if (first - 1 == begin)
            while (first < last && !comp(*--last, pivot));
        else
            while (!comp(*--last, pivot));

        bool already_partitioned = first >= last;

        if (!already_partitioned) {
            std::iter_swap(first++, last--);
            // from now, last will point to the last element not after it
            partition_branchless_core(first, last, pivot, comp);
        }

        // Put the pivot in the correct place
        RandomIt pivot_pos = --first;
        if (begin != pivot_pos)
            *begin = std::move(*pivot_pos);
        *pivot_pos = std::move(pivot);
        return std::make_pair(pivot_pos, already_partitioned);
    }
}