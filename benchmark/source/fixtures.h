#pragma once

#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <string>
#include <omp.h>
#include <parallel/algorithm>
#include <cstring>

#define ELEMENTS_VECTOR_DEFAULT std::size_t(2e9)
#define ELEMENTS_STRING_DEFAULT std::size_t(2e7)
#define STRING_SIZE_DEFAULT std::size_t(1001)

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT>
class VectorFixture : public benchmark::Fixture {
public:
    VectorFixture() {
        omp_set_nested(1);
    }

    void Prepare() {
        Deallocate();
        Allocate();
        GenerateData();
    }

    void Allocate() {
        data.resize(Size);
    }

    void Deallocate() {
        if (!std::is_sorted(this->data.begin(), this->data.end()))
            throw std::runtime_error("Not sorted");
        std::vector<T>().swap(this->data);
    }

    virtual void GenerateData() = 0;

protected:
    std::vector<T> data;
};

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT,
        T Min = std::numeric_limits<T>::min(),
        T Max = std::numeric_limits<T>::max(),
        int Seed = 0, typename Distribution = std::conditional_t<std::is_integral_v<T>,
        std::uniform_int_distribution<T>,
        std::uniform_real_distribution<T>>>
class RandomVectorFixture : public VectorFixture<T, Size> {
public:
    void GenerateData() override {
        RandomData();
    }

protected:
    void RandomData() {
        // static seed to generate same data across runs
        std::vector<int> seeds;
        for (int i = 0; i < omp_get_max_threads(); ++i)
            seeds.emplace_back(Seed + i);
        #pragma omp parallel
        {
            Distribution uniform_dist(Min, Max);
            std::mt19937 rng(seeds[omp_get_thread_num()]);
            #pragma omp for schedule(static)
            for (std::size_t i = 0; i < Size; ++i)
                this->data[i] = uniform_dist(rng);
        }
    }
};


template <typename T>
void prepare_ascending(T & data) {
    __gnu_parallel::sort(data.begin(), data.end(), __gnu_parallel::balanced_quicksort_tag());
}

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT,
        T Min = std::numeric_limits<T>::min(),
        T Max = std::numeric_limits<T>::max(),
        int Seed = 0>
class AscendingVectorFixture : public RandomVectorFixture<T, Size, Min, Max, Seed> {
public:
    void GenerateData() override {
        this->RandomData();
        prepare_ascending(this->data);
    }
};

template <typename T>
void prepare_descending(T & data) {
    __gnu_parallel::sort(data.begin(), data.end(), __gnu_parallel::balanced_quicksort_tag());
    std::reverse(data.begin(), data.end());
}

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT,
        T Min = std::numeric_limits<T>::min(),
        T Max = std::numeric_limits<T>::max(),
        int Seed = 0>
class DescendingVectorFixture : public RandomVectorFixture<T, Size, Min, Max, Seed> {
public:
    void GenerateData() override {
        this->RandomData();
        prepare_descending(this->data);
    }
};

template <typename T>
void prepare_organpipe(T & data, std::size_t mid) {
    __gnu_parallel::sort(data.begin(), data.end(), __gnu_parallel::balanced_quicksort_tag());
    std::reverse(data.begin() + mid, data.end());
}

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT,
        T Min = std::numeric_limits<T>::min(),
        T Max = std::numeric_limits<T>::max(),
        int Seed = 0>
class OrganPipeVectorFixture : public RandomVectorFixture<T, Size, Min, Max, Seed> {
public:
    void GenerateData() override {
        this->RandomData();
        prepare_organpipe(this->data, Size/2);
    }
};

template <typename T>
void prepare_rotated(T & data) {
    __gnu_parallel::sort(data.begin(), data.end(), __gnu_parallel::balanced_quicksort_tag());
    std::rotate(data.begin(), data.begin() + 1, data.end());
}

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT,
        T Min = std::numeric_limits<T>::min(),
        T Max = std::numeric_limits<T>::max(),
        int Seed = 0>
class RotatedVectorFixture : public RandomVectorFixture<T, Size, Min, Max, Seed> {
public:
    void GenerateData() override {
        this->RandomData();
        prepare_rotated(this->data);
    }
};


template <typename T>
void prepare_heap(T & data) {
    std::make_heap(data.begin(), data.end());
}

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT,
        T Min = std::numeric_limits<T>::min(),
        T Max = std::numeric_limits<T>::max(),
        int Seed = 0>
class HeapVectorFixture : public RandomVectorFixture<T, Size, Min, Max, Seed> {
public:
    void GenerateData() override {
        this->RandomData();
        prepare_heap(this->data);
    }
};


template <typename T = int, std::size_t Size = ELEMENTS_VECTOR_DEFAULT>
class AdversaryVectorFixture : public VectorFixture<T, Size> {
public:
    void GenerateData() override {
        static_assert(std::numeric_limits<T>::max() > (Size - 1), "Size is too big, this will overflow");

        // inspired from this paper: https://www.cs.dartmouth.edu/~doug/mdmspe.pdf
        int candidate = 0;
        int nsolid = 0;
        const int gas = Size - 1;

        // initially, all values are gas
        std::fill(this->data.begin(), this->data.end(), gas);

        // fill with values from 0 to Size-1
        // will be used as indices to this->data
        std::vector<int> asc_vals(Size);
        std::iota(asc_vals.begin(), asc_vals.end(), 0);

        auto cmp = [&](int x, int y) {
            if (this->data[x] == gas && this->data[y] == gas)
            {
                if (x == candidate)
                    this->data[x] = nsolid++;
                else
                    this->data[y] = nsolid++;
            }
            if (this->data[x] == gas)
                candidate = x;
            else if (this->data[y] == gas)
                candidate = y;
            return this->data[x] < this->data[y];
        };
        std::sort(asc_vals.begin(), asc_vals.end(), cmp);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT,
          std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class RandomStringVectorFixture : public VectorFixture<std::string, Size> {
    private:
        std::string GenerateString(const int seed) {
            std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            std::uniform_int_distribution<int> uniform_dist(0, static_cast<int>(chars.size()) - 1);
            std::mt19937 rng(seed);
            std::string res(StringSize, 0);
            if constexpr (Prepended)
                res[StringSize - 1] = chars[uniform_dist(rng)];
            else
                for (int i = 0; i < StringSize; ++i)
                    res[i] = chars[uniform_dist(rng)];
            return res;
        }

    public:
        void GenerateData() override {
            RandomData();
        }

    protected:
        void RandomData() {
            std::vector<int> seeds;
            for (int i = 0; i < omp_get_max_threads(); ++i)
                seeds.emplace_back(Seed + i);
            #pragma omp parallel for schedule(static)
            for (std::size_t i = 0; i < Size; ++i)
                this->data[i] = GenerateString(seeds[omp_get_thread_num()]);
        }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT,
          std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class AscendingStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void GenerateData() override {
        this->RandomData();
        prepare_ascending(this->data);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class DescendingStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void GenerateData() override {
        this->RandomData();
        prepare_descending(this->data);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class OrganPipeStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void GenerateData() override {
        this->RandomData();
        prepare_organpipe(this->data, Size/2);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class RotatedStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void GenerateData() override {
        this->RandomData();
        prepare_rotated(this->data);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class HeapStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void GenerateData() override {
        this->RandomData();
        prepare_heap(this->data);
    }
};