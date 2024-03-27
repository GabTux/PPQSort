#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ppqsort.h>

#include <random>
#include <limits>


/****************************************************
 * PPQSort Tests
****************************************************/

template <typename T>
class VectorFixture : public ::testing::Test {
    public:
        virtual void AllocateVector(std::size_t size) {
            this->data.resize(size);
        };

    protected:
        std::vector<T> data;
};

template <typename T, typename Distribution = std::conditional_t<std::is_integral_v<T>,
                                              std::uniform_int_distribution<T>,
                                              std::uniform_real_distribution<T>>>
class RandomVectorFixture : public VectorFixture<T> {
    protected:
        T min;
        T max;

        void FillVector(T from, T to) {
            std::mt19937 rng(std::random_device{}());
            Distribution uniform_dist(from, to);
            for (std::size_t i = 0; i < this->data.size(); ++i)
                this->data[i] = uniform_dist(rng);
        }

    public:
        RandomVectorFixture() : min(std::numeric_limits<T>::min()),
                                max(std::numeric_limits<T>::max())
        {
            static_assert(std::is_arithmetic_v<T>, "Type must be arithmetic");
        }

        void FillVector() {
            this->FillVector(min, max);
        }

        void FillVector(std::size_t range) {
            if (range > static_cast<std::size_t>(max - min))
                return;
            T from, to;
            if constexpr (std::is_signed_v<T>) {
                from = -range / 2;
                to = range / 2;
            } else {
                from = 0;
                to = range;
            }
            this->FillVector(from, to);
        }
};

static std::size_t sizes[] = {
    10, 20, 129, // to test small inputs --> switch to sequential
    static_cast<std::size_t>(1e6)
};

static std::size_t ranges[] = {
    1, 2, 3, 5, 10, 20
};

TEST(StaticInputs, vals_20) {
    std::vector<int> in = {52, 0, 5, 1, 2, 3, 45, 8, 1, 10,
                           52, 0, 5, 1, 2, 3, 45, 8, 1, 10};
    std::vector<int> ref(in);
    ppqsort::sort(in.begin(), in.end());
    std::sort(ref.begin(), ref.end());
    ASSERT_THAT(in, ::testing::ContainerEq(ref));
}

TEST(StaticInputs, EmptyContainer) {
    std::vector<int> in = {};
    ppqsort::sort(ppqsort::execution::par, in.begin(), in.end());
    ppqsort::sort(ppqsort::execution::seq, in.begin(), in.end());
    ASSERT_THAT(in, ::testing::ContainerEq(std::vector<int>{}));
}

#ifdef _OPENMP

TEST(StaticInputs, TriggerSeqPartition) {
    std::vector<int> in = {52, 0, 5, 1, 2, 3, 45, 8, 1, 10,
                           52, 0, 5, 1, 2, 3, 45, 8, 1, 10};
    std::vector ref(in);
    auto res = ppqsort::impl::openmp::partition_right_branchless_par(in.begin(), in.end(), std::less<>(), 4);
    auto pivot = *res.first;
    ASSERT_TRUE(std::is_partitioned(in.begin(), in.end(), [&](const int & i){ return i < pivot;}));
    res = ppqsort::impl::openmp::partition_to_right_par(in.begin(), in.end(), std::less<>(), 4);
    pivot = *res.first;
    ASSERT_TRUE(std::is_partitioned(in.begin(), in.end(), [&](const int & i){ return i < pivot;}));
}

#else
TEST(StaticInputs, TriggerSeqPartition) {
    std::vector<int> in = {52, 0, 5, 1, 2, 3, 45, 8, 1, 10,
                           52, 0, 5, 1, 2, 3, 45, 8, 1, 10};
    std::vector ref(in);
    ppqsort::impl::cpp::ThreadPool<> threadpool(4);
    auto res = ppqsort::impl::cpp::partition_right_branchless_par(in.begin(), in.end(), std::less<>(), 4, threadpool);
    auto pivot = *res.first;
    ASSERT_TRUE(std::is_partitioned(in.begin(), in.end(), [&](const int & i){ return i < pivot;}));
    res = ppqsort::impl::cpp::partition_to_right_par(in.begin(), in.end(), std::less<>(), 4, threadpool);
    pivot = *res.first;
    ASSERT_TRUE(std::is_partitioned(in.begin(), in.end(), [&](const int & i){ return i < pivot;}));
}
#endif


TEST(Patterns, Ascending) {
    for (auto const & size: sizes) {
        std::vector<int> in(size);
        std::iota(in.begin(), in.end(), 0);
        std::vector<int> ref(in);
        ppqsort::sort(ppqsort::execution::par, in.begin(), in.end());
        std::sort(ref.begin(), ref.end());
        ASSERT_THAT(in, ::testing::ContainerEq(ref));
    }
}

TEST(Patterns, Desceding) {
    for (auto const & size: sizes) {
        std::vector<int> in(size);
        std::iota(in.rbegin(), in.rend(), 0);
        std::vector<int> ref(in);
        ppqsort::sort(ppqsort::execution::par, in.begin(), in.end());
        std::sort(ref.begin(), ref.end());
        ASSERT_THAT(in, ::testing::ContainerEq(ref));
    }
}

TEST(Patterns, Constant) {
    for (auto const & size: sizes) {
        std::vector<int> in(size, 42);
        std::vector<int> ref(in);
        ppqsort::sort(ppqsort::execution::par, in.begin(), in.end());
        std::sort(ref.begin(), ref.end());
        ASSERT_THAT(in, ::testing::ContainerEq(ref));
    }
}

TEST(Patterns, HalfSorted) {
    for (auto const & size: sizes) {
        std::vector<int> in(size);
        auto mid = size / 2;
        std::iota(in.begin(), in.end(), 0);
        std::shuffle(in.begin() + mid, in.end(), std::mt19937 {std::random_device{}()});
        std::vector<int> ref(in);
        ppqsort::sort(ppqsort::execution::par, in.begin(), in.end());
        std::sort(ref.begin(), ref.end());
        ASSERT_THAT(in, ::testing::ContainerEq(ref));
    }
}

TEST(Patterns, Adversary) {
    std::size_t sizes[] = {128, static_cast<std::size_t>(1e7)};
    for (const auto & Size: sizes) {
        std::vector<int> data;
        data.resize(Size);
        int candidate = 0;
        int nsolid = 0;
        const int gas = Size - 1;

        // initially, all values are gas
        std::ranges::fill(data, gas);

        // fill with values from 0 to Size-1
        // will be used as indices to this->data
        std::vector<int> asc_vals(Size);
        std::iota(asc_vals.begin(), asc_vals.end(), 0);

        auto cmp = [&](int x, int y) {
            if (data[x] == gas && data[y] == gas)
            {
                if (x == candidate)
                    data[x] = nsolid++;
                else
                    data[y] = nsolid++;
            }
            if (data[x] == gas)
                candidate = x;
            else if (data[y] == gas)
                candidate = y;
            return data[x] < data[y];
        };
        ppqsort::sort(ppqsort::execution::par_force_branchless, asc_vals.begin(), asc_vals.end(), cmp);
        ppqsort::sort(ppqsort::execution::par, data.begin(), data.end());
    }
}


// Random to test general cases, different types and ranges
TYPED_TEST_SUITE_P(RandomVectorFixture);

TYPED_TEST_P(RandomVectorFixture, FullTypeRange) {
    for (auto const & size: sizes) {
        this->AllocateVector(size);
        this->FillVector();
        auto ref(this->data);
        ppqsort::sort(ppqsort::execution::par, this->data.begin(), this->data.end());
        std::sort(ref.begin(), ref.end());
        ASSERT_THAT(this->data, ::testing::ContainerEq(ref));
    }
}

TYPED_TEST_P(RandomVectorFixture, CustomComparator) {
    for (auto const & size: sizes) {
        auto comp = [](const auto & a, const auto & b) { return a > b; };
        this->AllocateVector(size);
        this->FillVector();
        auto ref(this->data);
        ppqsort::sort(ppqsort::execution::par, this->data.begin(), this->data.end(), comp);
        std::sort(ref.begin(), ref.end(), comp);
        ASSERT_THAT(this->data, ::testing::ContainerEq(ref));
    }
}

TYPED_TEST_P(RandomVectorFixture, Threads) {
    for (auto const & size: sizes) {
        for (int threadCount = 1; threadCount <= 4; ++threadCount) {
            auto comp = [](const auto & a, const auto & b) { return a > b; };
            this->AllocateVector(size);
            this->FillVector();
            auto ref(this->data);
            ppqsort::sort(ppqsort::execution::par, this->data.begin(), this->data.end(), comp, threadCount*2);
            std::sort(ref.begin(), ref.end(), comp);
            ASSERT_THAT(this->data, ::testing::ContainerEq(ref));
        }
        for (int threadCount = 1; threadCount <= 4; ++threadCount) {
            this->AllocateVector(size);
            this->FillVector();
            auto ref(this->data);
            ppqsort::sort(ppqsort::execution::par, this->data.begin(), this->data.end(), threadCount*2);
            std::sort(ref.begin(), ref.end());
            ASSERT_THAT(this->data, ::testing::ContainerEq(ref));
        }
    }
}

TYPED_TEST_P(RandomVectorFixture, Ranges) {
    for (auto const & size: sizes) {
        for (auto const & i: ranges) {
            if (i > static_cast<std::size_t>(this->max - this->min))
                continue;
            this->AllocateVector(size);
            this->FillVector(i);
            auto ref(this->data);
            ppqsort::sort(ppqsort::execution::par, this->data.begin(), this->data.end());
            std::sort(ref.begin(), ref.end());
            ASSERT_THAT(this->data, ::testing::ContainerEq(ref));
        }
    }
}

REGISTER_TYPED_TEST_SUITE_P(RandomVectorFixture, FullTypeRange, CustomComparator, Ranges, Threads);
using baseTypesUnsigned = ::testing::Types<unsigned char, unsigned short, unsigned int, unsigned long, unsigned long long>;
using baseTypes = ::testing::Types<char, short, int, long, long long, float, double, long double>;
INSTANTIATE_TYPED_TEST_SUITE_P(UnsignedTypes, RandomVectorFixture, baseTypesUnsigned);
INSTANTIATE_TYPED_TEST_SUITE_P(SignedTypes, RandomVectorFixture, baseTypes);