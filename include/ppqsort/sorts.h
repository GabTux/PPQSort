#pragma once

namespace ppqsort::impl {

    // 8 instructions, no jumps, only conditional moves, 1 compare
    // https://godbolt.org/z/o3sTjeGnv
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void sort_2_branchless(RandomIt a, RandomIt b, Compare comp) {
        bool res = comp(*a, *b);
        T tmp = res ? *a : *b;
        *b = res ? *b : *a;
        *a = tmp;
    }

    // sorts correctly a, b, c if b and c are already sorted, 2 compares
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void sort_3_partial_branchless(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
        bool res = comp(*c, *a);
        T tmp = res ? *c : *a;
        *c = res ? *a : *c;
        res = comp(tmp, *b);
        *a = res ? *a : *b;
        *b = res ? *b : tmp;
    }

    // 17 instructions, no jumps, 3 compares
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void sort_3_branchless(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
        sort_2_branchless(b, c, comp);
        sort_3_partial_branchless(a, b, c, comp);
    }

    // 43 instructions, no jumps, 9 compares
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void sort_5_branchless(RandomIt x1, RandomIt x2, RandomIt x3, RandomIt x4, RandomIt x5, Compare comp) {
        sort_2_branchless(x1, x2, comp);
        sort_2_branchless(x4, x5, comp);
        sort_3_partial_branchless(x3, x4, x5, comp);
        sort_2_branchless(x2, x5, comp);
        sort_3_partial_branchless(x1, x3, x4, comp);
        sort_3_partial_branchless(x2, x3, x4, comp);
    }

    // 2-3 compares, 0-2 swaps, stable
    template<class RandomIt, class Compare>
    inline void sort_3(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
        if (!comp(*b, *a)) {     // a <= b
            if (!comp(*c, *b))   // b <= c
                return;
            // b >= a, but b > c --> swap
            std::iter_swap(b, c);
            // after swap: a <= c, b < c
            if (comp(*b, *a)) {
                // a <= c, b < c, a > b
                std::iter_swap(a, b);
                // after swap: b <= c, a < c, a < b
            }
            return;
        }
        if (comp(*c, *b)) {
            // a > b, b > c
            std::iter_swap(a, c);
            return;
        }
        // a > b, b <= c
        std::iter_swap(a, b);
        // a < b, a <= c
        if (comp(*c, *b)) {
            // a < b, a <= c, b > c
            std::iter_swap(b, c);
            // after swap: a < c, a <= b, b < c
        }
    }

    // Sort [begin, end) using comp by insertion sort
    template<typename RandomIt, typename Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void insertion_sort(RandomIt begin, RandomIt end, Compare comp) {
        if (begin == end)
            return;

        // before iterator "first" is sorted segment
        // under and after iterator "second" is unsorted segment
        // | ___ sorted ___ <first> <second> ___ unsorted ___ |
        for (auto it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                // save current element
                T elem = std::move(*second);
                // iterate until according spot is found
                do {
                    *second-- = std::move(*first--);
                } while (second != begin && comp(elem, *first));
                // copy current element
                *second = std::move(elem);
            }
        }
    }

    // Sort [begin, end) using comp by insertion sort
    // Assumes that on position (first - 1) is an element
    // that is lower or equal to all elements in input range
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline void insertion_sort_unguarded(RandomIt begin, RandomIt end, Compare comp) {
        if (begin == end)
            return;

        // same approach as for insertion sort above
        // only one in while loop can be omitted
        for (RandomIt it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                T elem = std::move(*second);
                do {
                    *second-- = std::move(*first--);
                } while (comp(elem, *first));
                *second = std::move(elem);
            }
        }
    }

    // Try to sort [begin, end) using comp by insertion sort
    // if more than swap_limit swaps are made, abort sorting
    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline bool partial_insertion_sort(RandomIt begin, RandomIt end, Compare comp) {
        if (begin == end)
            return true;

        constexpr unsigned int swap_limit = parameters::partial_insertion_threshold;
        std::size_t count = 0;

        for (RandomIt it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                T elem = std::move(*second);

                do {
                    *second-- = std::move(*first--);
                } while (second != begin && comp(elem, *first));
                *second = std::move(elem);
                if (++count >= swap_limit) {
                    return ++it == end;
                }
            }
        }
        return true;
    }

    template<class RandomIt, class Compare,
             typename T = typename std::iterator_traits<RandomIt>::value_type>
    inline bool partial_insertion_sort_unguarded(RandomIt begin, RandomIt end, Compare comp) {
        if (begin == end)
            return true;

        constexpr unsigned int swap_limit = parameters::partial_insertion_threshold;
        std::size_t count = 0;

        for (RandomIt it = begin + 1; it != end; ++it) {
            auto first = --it;
            auto second = ++it;

            if (comp(*second, *first)) {
                T elem = std::move(*second);

                do {
                    *second-- = std::move(*first--);
                } while (comp(elem, *first));
                *second = std::move(elem);
                if (++count >= swap_limit) {
                    return ++it == end;
                }
            }
        }
        return true;
    }
}