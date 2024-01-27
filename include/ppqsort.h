/*
* TBD: license
*/

#pragma once

#include "ppqsort/mainloop.h"
#include "ppqsort/parameters.h"


#ifdef _OPENMP
#include "ppqsort/parallel/openmp/mainloop_par.h"
#else
#include "ppqsort/parallel/cpp/mainloop_par.h"
#endif


namespace ppqsort::impl {
   template <bool Force_branchless = false,
            typename RandomIt,
            typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>,
            bool Branchless = use_branchless<typename std::iterator_traits<RandomIt>::value_type, Compare>::value>
   inline void seq_ppqsort(RandomIt begin, RandomIt end, Compare comp = Compare()) {
       if (begin == end)
           return;

       constexpr bool branchless = Force_branchless || Branchless;
       seq_loop<RandomIt, Compare, branchless>(begin, end, comp, log2(end - begin));
   }

   template <typename ExecutionPolicy, typename... T>
   constexpr void call_sort(ExecutionPolicy&&, T... args) {
       static_assert(execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq)> ||
                     execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par)> ||
                     execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq_force_branchless)> ||
                     execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par_force_branchless)>,
           "Provided ExecutionPolicy is not valid. Use predefined policies from namespace ppqsort::execution.");
       if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq)>) {
           seq_ppqsort(std::forward<T>(args)...);
       } else if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par)>) {
           par_ppqsort(std::forward<T>(args)...);
       } else if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::seq_force_branchless)>) {
           seq_ppqsort<true>(std::forward<T>(args)...);
       } else if constexpr (execution::_is_same_decay_v<ExecutionPolicy, decltype(execution::par_force_branchless)>) {
           par_ppqsort<true>(std::forward<T>(args)...);
       } else {
           throw std::invalid_argument("Unknown execution policy."); // this should never happen
       }
   }
}

namespace ppqsort {
   template <typename RandomIt>
   void sort(RandomIt begin, RandomIt end) {
       impl::seq_ppqsort(begin, end);
   }

   template <typename ExecutionPolicy, typename RandomIt>
   void sort(ExecutionPolicy&& policy, RandomIt begin, RandomIt end) {
       impl::call_sort(std::forward<ExecutionPolicy>(policy), begin, end);
   }

   template <typename RandomIt, typename Compare>
   void sort(RandomIt begin, RandomIt end, Compare comp) {
       impl::seq_ppqsort(begin, end, comp);
   }

   template <typename ExecutionPolicy, typename RandomIt, typename Compare>
   void sort(ExecutionPolicy&& policy, RandomIt begin, RandomIt end, Compare comp) {
       impl::call_sort(std::forward<ExecutionPolicy>(policy), begin, end, comp);
   }
}