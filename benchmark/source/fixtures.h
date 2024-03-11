#pragma once

#include <benchmark/benchmark.h>
#include <cstring>
#include <fstream>
#include <omp.h>
#include <parallel/algorithm>
#include <random>
#include <string>
#include <vector>
#include <fast_matrix_market/fast_matrix_market.hpp>

#define ELEMENTS_VECTOR_DEFAULT std::size_t(2e9)
#define ELEMENTS_STRING_DEFAULT std::size_t(2e7)
#define STRING_SIZE_DEFAULT std::size_t(1001)

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT>
class VectorFixture : public benchmark::Fixture {
public:
    VectorFixture() {
        omp_set_nested(1);
    }

    virtual void prepare() {
        deallocate();
        allocate();
        generate_data();
    }

    virtual void allocate() {
        data_.resize(Size);
    }

    virtual void deallocate() {
        if (!is_sorted_par())
            throw std::runtime_error("Not sorted");
        std::vector<T>().swap(this->data_);
    }

    virtual void generate_data() = 0;

protected:
    std::vector<T> data_;

    bool is_sorted_par(const int n_threads = omp_get_max_threads()) {
        if (data_.size() < 2)
            return true;

        std::vector<uint8_t> sorted(omp_get_num_threads());
        #pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            const std::size_t chunk_size = (data_.size() + n_threads - 1) / n_threads;
            const std::size_t my_begin = tid * chunk_size;
            const std::size_t my_end = std::min(my_begin + chunk_size + 1, data_.size());
            sorted[tid] = std::is_sorted(this->data_.begin() + my_begin, this->data_.begin() + my_end);
        }
        return std::all_of(sorted.begin(), sorted.end(), [](const uint8_t x) { return x; });
    }
};

template <typename T, std::size_t Size = ELEMENTS_VECTOR_DEFAULT,
        T Min = std::numeric_limits<T>::min(),
        T Max = std::numeric_limits<T>::max(),
        int Seed = 0, typename Distribution = std::conditional_t<std::is_integral_v<T>,
        std::uniform_int_distribution<T>,
        std::uniform_real_distribution<T>>>
class RandomVectorFixture : public VectorFixture<T, Size> {
public:
    void generate_data() override {
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
                this->data_[i] = uniform_dist(rng);
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
    void generate_data() override {
        this->RandomData();
        prepare_ascending(this->data_);
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
    void generate_data() override {
        this->RandomData();
        prepare_descending(this->data_);
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
    void generate_data() override {
        this->RandomData();
        prepare_organpipe(this->data_, Size/2);
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
    void generate_data() override {
        this->RandomData();
        prepare_rotated(this->data_);
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
    void generate_data() override {
        this->RandomData();
        prepare_heap(this->data_);
    }
};


template <typename T, std::size_t Size>
concept SizeFit = std::numeric_limits<T>::max() > (Size - 1);

template <typename T = int, std::size_t Size = ELEMENTS_VECTOR_DEFAULT>
class AdversaryVectorFixture : public VectorFixture<T, Size> {
public:
    void generate_data() requires SizeFit<T, Size> {
        // inspired from this paper: https://www.cs.dartmouth.edu/~doug/mdmspe.pdf
        int candidate = 0;
        int nsolid = 0;
        const int gas = Size - 1;

        // initially, all values are gas
        std::ranges::fill(this->data_, gas);

        // fill with values from 0 to Size-1
        // will be used as indices to this->data
        std::vector<int> asc_vals(Size);
        std::iota(asc_vals.begin(), asc_vals.end(), 0);

        auto cmp = [&](int x, int y) {
            if (this->data_[x] == gas && this->data_[y] == gas)
            {
                if (x == candidate)
                    this->data_[x] = nsolid++;
                else
                    this->data_[y] = nsolid++;
            }
            if (this->data_[x] == gas)
                candidate = x;
            else if (this->data_[y] == gas)
                candidate = y;
            return this->data_[x] < this->data_[y];
        };
        std::ranges::sort(asc_vals, cmp);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT,
          std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class RandomStringVectorFixture : public VectorFixture<std::string, Size> {
    private:
        static std::string GenerateString(const int seed) {
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
        void generate_data() override {
            RandomData();
        }

    protected:
        void RandomData() {
            std::vector<int> seeds;
            for (int i = 0; i < omp_get_max_threads(); ++i)
                seeds.emplace_back(Seed + i);
            #pragma omp parallel for schedule(static)
            for (std::size_t i = 0; i < Size; ++i)
                this->data_[i] = GenerateString(seeds[omp_get_thread_num()]);
        }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT,
          std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class AscendingStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void generate_data() override {
        this->RandomData();
        prepare_ascending(this->data_);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class DescendingStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void generate_data() override {
        this->RandomData();
        prepare_descending(this->data_);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class OrganPipeStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void generate_data() override {
        this->RandomData();
        prepare_organpipe(this->data_, Size/2);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class RotatedStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void generate_data() override {
        this->RandomData();
        prepare_rotated(this->data_);
    }
};

template <bool Prepended = true, std::size_t StringSize = STRING_SIZE_DEFAULT, std::size_t Size = ELEMENTS_STRING_DEFAULT, int Seed = 0>
class HeapStringVectorFixture : public RandomStringVectorFixture<Prepended, StringSize, Size, Seed> {
    void generate_data() override {
        this->RandomData();
        prepare_heap(this->data_);
    }
};

enum Matrices {
    af_shell10,
    cage15,
    fem_hifreq_circuit,
};

template <typename ValueType>
struct matrix_element_t {
    uint32_t row, col;
    ValueType value;

    bool operator<(const matrix_element_t& other) const {
        if (row < other.row)
            return true;
        if ((row == other.row) && (col < other.col))
            return true;
        return false;
    }
};

using matrix_element_double = matrix_element_t<double>;
using matrix_element_complex = matrix_element_t<std::complex<double>>;

template <Matrices Matrix, bool Complex>
class SparseMatrixVectorFixture : public VectorFixture<std::conditional_t<Complex, matrix_element_complex, matrix_element_double>> {
    using ValueType = std::conditional_t<Complex, std::complex<double>, double>;
    public:
        SparseMatrixVectorFixture() :
            file_(get_filename()) {
            if (!file_.is_open())
                throw std::runtime_error(std::string("Error opening matrix file: ") +get_filename());
            read_to_memory();
        }

        void prepare() override {
            this->deallocate();
            allocate();
            generate_data();
        }

        void allocate() override {
            this->data_.resize(values_.size());
        };

        void generate_data() override {
            #pragma omp parallel for schedule(static)
            for (std::size_t i = 0; i < values_.size(); ++i) {
                this->data_[i].row = rows_[i];
                this->data_[i].col = cols_[i];
                this->data_[i].value = values_[i];
            }
        }

    private:
        consteval static const char * get_filename() {
            switch (Matrix) {
                case af_shell10:
                    return "af_shell10.mtx";
                case cage15:
                    return "cage15.mtx";
                case fem_hifreq_circuit:
                    return "fem_hifreq_circuit.mtx";
                default:
                    throw std::runtime_error("Unknown matrix");
            }
        }

        void read_to_memory() {
            fast_matrix_market::read_matrix_market_triplet(file_, nrows_, ncols_, rows_, cols_, values_);
        }

        std::fstream file_;
        std::size_t nrows_{}, ncols_{};
        std::vector<uint32_t> rows_, cols_;
        std::vector<ValueType> values_{};
};