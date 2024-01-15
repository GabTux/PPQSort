#pragma once

namespace ppqsort::impl {

    using bitset_type = uint64_t;
    long int bitset_size = sizeof(bitset_type) * 8;

    template<side s, class RandomIt, class Compare, typename T = std::iterator_traits<RandomIt>::value_type>
    inline void _populate_bitset(RandomIt pos, Compare comp, T & pivot, bitset_type& bitset,
                                 size_t block_size = bitset_size) {
        RandomIt it = pos;
        // this loop can be vectorized
        for (size_t i = 0; i < block_size; ++i) {
            if constexpr (s == side::left)
                bitset |= (static_cast<bitset_type>(!comp(*it++, pivot)) << i);
            else
                bitset |= (static_cast<bitset_type>(comp(*it--, pivot)) << i);
        }
    }

    // Reset Lowest Set Bit
    // (x & -x) --> -x flips all bits higher than rightmost 1 bit
    //          --> (x & -x) isolate the rightmost 1 bit (all other bits set to 0)
    // x ^ above --> flip the rightmost 1 bit and keep all others unchanged
    inline bitset_type _blsr(bitset_type x) {
        return x ^ (x & -x);
    }

    template<class RandomIt>
    inline void _swap_bitsets(RandomIt first, RandomIt last, bitset_type& left_bitset, bitset_type& right_bitset) {
        typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;
        // swap one pair in each iteration
        while (left_bitset != 0 && right_bitset != 0) {
            // skip all zeros (correctly placed elements)
            diff_t tz_left = __builtin_ctzl(left_bitset);
            diff_t tz_right = __builtin_ctzl(right_bitset);
            // swap pair
            std::iter_swap(first + tz_left, last - tz_right);
            // flip that bits to 0, because we have swapped
            left_bitset = _blsr(left_bitset);
            right_bitset = _blsr(right_bitset);
        }
    }

    template<class RandomIt, class Compare, typename T = std::iterator_traits<RandomIt>::value_type>
    inline void _bitset_partition_partial(RandomIt& first, RandomIt& last, Compare comp, T & pivot,
                                          bitset_type& left_bitset, bitset_type& right_bitset) {
        typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;
        diff_t size = last - first + 1;
        diff_t l_size, r_size;

        if (left_bitset == 0 && right_bitset == 0) {
            l_size = size / 2;
            r_size = size - l_size;
        } else if (left_bitset == 0) {
            // one side is a full block
            l_size = size - bitset_size;
            r_size = bitset_size;
        } else { // if (right_bitset == 0)
            r_size = size - bitset_size;
            l_size = bitset_size;
        }
        // record comparisons results
        if (left_bitset == 0)
            _populate_bitset<side::left>(first, comp, pivot, left_bitset, l_size);
        if (right_bitset == 0)
            _populate_bitset<side::right>(last, comp, pivot, right_bitset, r_size);

        _swap_bitsets(first, last, left_bitset, right_bitset);
        first += (left_bitset == 0) ? l_size : 0;
        last -= (right_bitset == 0) ? r_size : 0;
    }

    template<class RandomIt>
    inline void _swap_bitsets_inside_block(RandomIt& first, RandomIt& last,
                                           bitset_type& left_bitset, bitset_type& right_bitset) {
        typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;

        if (left_bitset) {
            // swap inside left side, find positions in reverse order
            while (left_bitset) {
                diff_t lz_left = bitset_size - 1 - __builtin_clzl(left_bitset);
                left_bitset &= (static_cast<bitset_type>(1) << lz_left) - 1;
                RandomIt it = first + lz_left;
                if (it != last)
                    std::iter_swap(it, last);
                --last;
            }
            first = last + 1;
        } else if (right_bitset) {
            while (right_bitset) {
                diff_t lz_right = bitset_size - 1 - __builtin_clzl(right_bitset);
                right_bitset &= (static_cast<bitset_type>(1) << lz_right) - 1;
                RandomIt it = last - lz_right;
                if (it != first)
                    std::iter_swap(it, first);
                first++;
            }
        }
    }

    template<class RandomIt, class Compare, typename T = std::iterator_traits<RandomIt>::value_type>
    inline std::pair<RandomIt, bool> _bitset_partition(RandomIt begin, RandomIt end, Compare comp) {
        T pivot = std::move(*begin);
        RandomIt first = begin;
        RandomIt last = end;

        // find first elem greater or equal to the pivot from start
        while (comp(*++first, pivot));

        // find first elem from end, which is less than pivot
        if (begin == first - 1) {
            while (first < last && !comp(*--last, pivot));
        } else {
            while (!comp(*--last, pivot));
        }

        bool already_partitioned = first >= last;
        if (!already_partitioned) {
            std::iter_swap(first, last);
            ++first;
            --last;
            // last is always pointing to the last element
            // it is inclusive: [first, last]

            // process elements by blocks
            bitset_type left_bitset = 0;
            bitset_type right_bitset = 0;
            while (last - first >= 2 * bitset_size - 1) {
                // Record comparison results
                // process next block, if we resolved current
                if (left_bitset == 0)
                    _populate_bitset<side::left>(first, comp, pivot, left_bitset);
                if (right_bitset == 0)
                    _populate_bitset<side::right>(last, comp, pivot, right_bitset);
                // swap the recorded candidates
                _swap_bitsets(first, last, left_bitset, right_bitset);
                // advance iterators, if the whole block has been resolved
                first += (left_bitset == 0) ? bitset_size : 0;
                last -= (right_bitset == 0) ? bitset_size : 0;
            }
            // less than block of elements on both sides (or at least one side)
            _bitset_partition_partial(first, last, comp, pivot, left_bitset, right_bitset);
            // at least one bitset is empty, process the eventually non-empty
            _swap_bitsets_inside_block(first, last, left_bitset, right_bitset);
        }

        // move pivot from start to according place
        RandomIt pivot_pos = --first;
        if (begin != pivot_pos)
            *begin = std::move(*pivot_pos);
        *pivot_pos = std::move(pivot);
        return std::make_pair(pivot_pos, already_partitioned);
    }
}
