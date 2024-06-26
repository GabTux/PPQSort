#include <iostream>
#include "ppqsort.h"
#include <random>

/*
 * Example of usage
 */

int main() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution uniform_dist(0, std::numeric_limits<int>::max());

    constexpr int max_size = 2e7;
    std::vector<int> input;
    input.reserve(max_size);
    for (int i = 0; i < max_size; ++i) {
        input.emplace_back(uniform_dist(rng));
    }
    std::vector input_std(input);

    auto start = std::chrono::steady_clock::now();
    ppqsort::sort(ppqsort::execution::par, input.begin(), input.end());
    auto end = std::chrono::steady_clock::now();
    std::cout << "Parallel PPQSort time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    start = std::chrono::steady_clock::now();
    std::sort(input_std.begin(), input_std.end());
    end = std::chrono::steady_clock::now();
    std::cout << "std::sort time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

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
    constexpr int max_size_string = 2e6;
    std::vector<std::string> input_str;
    input_str.reserve(max_size_string);
    std::uniform_int_distribution uniform_dist_str(0, static_cast<int>(chars.size()) - 1);
    for (int i = 0; i < max_size_string; ++i) {
      constexpr int string_size = 100;
      std::string res(string_size, 0);
      res[string_size - 1] = chars[uniform_dist_str(rng)];
      input_str.emplace_back(res);
    }

    // enforce branchless variant for strings
    start = std::chrono::steady_clock::now();
    ppqsort::sort(ppqsort::execution::par_force_branchless, input_str.begin(), input_str.end());
    end = std::chrono::steady_clock::now();
    std::cout << "Parallel PPQSort time, strings: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
    return !std::ranges::is_sorted(input_str);
}
