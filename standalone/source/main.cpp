#include <iostream>
#include "ppqsort.h"
#include <random>

/*
 * Example of usage
 */

int main() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution uniform_dist(0, std::numeric_limits<int>::max());

    constexpr int max_size = 2 << 20; // approx. 2 millions
    std::vector<int> input(max_size);
    for (int i = 0; i < max_size; ++i) {
        input.emplace_back(uniform_dist(rng));
    }
    std::vector input_std(input);

    TIME_MEASURE_START(ppqsort_time_par);
    ppqsort::sort(ppqsort::execution::par, input.begin(), input.end());
    TIME_MEASURE_END(ppqsort_time_par, "Parallel PPQSort time");

    TIME_MEASURE_START(ppqsort_time);
    std::sort(std::execution::par, input_std.begin(), input_std.end());
    TIME_MEASURE_END(ppqsort_time, "Parallel std_sort time");

    std::cout << "sorted: ";
    for (int i = 0; i < max_size; ++i) {
        if (input[i] != input_std[i]) {
            std::cout << "FALSE, error on index: "
            << i << std::endl;
            return 1;
        }
    }
    std::cout << "TRUE" << std::endl;


    // generate random strings
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::vector<std::string> input_str(max_size);
    std::uniform_int_distribution uniform_dist_str(0, static_cast<int>(chars.size()) - 1);
    for (int i = 0; i < max_size; ++i) {
      constexpr int string_size = 100;
      std::string res(string_size, 0);
      res[string_size - 1] = chars[uniform_dist_str(rng)];
      input_str.emplace_back(res);
    }

    // enforce branchless variant for strings
    TIME_MEASURE_START(ppqsort_time_par_str);
    ppqsort::sort(ppqsort::execution::par_force_branchless, input_str.begin(), input_str.end());
    TIME_MEASURE_END(ppqsort_time_par_str, "Parallel PPQSort time string");
}
