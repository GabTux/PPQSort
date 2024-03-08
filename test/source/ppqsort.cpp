#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ppqsort.h>

#include <random>
#include <limits>
#include <tuple>
#include <ostream>

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
    private:
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

        void FillVector(long long range) {
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

TEST(StaticInputs, vals_20) {
    std::vector<int> in = {52, 0, 5, 1, 2, 3, 45, 8, 1, 10,
                           52, 0, 5, 1, 2, 3, 45, 8, 1, 10};
    std::vector<int> ref(in);
    ppqsort::sort(in.begin(), in.end());
    std::sort(ref.begin(), ref.end());
    ASSERT_THAT(in, ::testing::ContainerEq(ref));
}

template <typename T>
std::string print_vec(T vec) {
    std::ostringstream res;
    for (auto it = vec.begin(); (it+1) != vec.end(); ++it)
        res << *it << ", ";
    res << *(vec.end() - 1);
    return res.str();
}

static long long sizes[] = {
    1,
    2,
    5,
    10,
    20,
    50,
    100,
    1000,
    10000,
    100000
};

TYPED_TEST_SUITE_P(RandomVectorFixture);

TYPED_TEST_P(RandomVectorFixture, FullTypeRange) {
    for (auto const & size: sizes) {
        this->AllocateVector(size);
        this->FillVector();
        auto ref(this->data);
        auto orig(this->data);
        ppqsort::sort(ppqsort::execution::par, this->data.begin(), this->data.end());
        std::sort(ref.begin(), ref.end());
        ASSERT_THAT(this->data, ::testing::ContainerEq(ref))
            << "input: " << print_vec(orig);
    }
}

TYPED_TEST_P(RandomVectorFixture, TypeRange10) {
    for (auto const & size: sizes) {
        this->AllocateVector(size);
        this->FillVector(10);
        auto ref(this->data);
        auto orig(this->data);
        ppqsort::sort(ppqsort::execution::par, this->data.begin(), this->data.end());
        std::sort(ref.begin(), ref.end());
        ASSERT_THAT(this->data, ::testing::ContainerEq(ref))
            << "input: " << print_vec(orig);
    }
}


REGISTER_TYPED_TEST_SUITE_P(RandomVectorFixture, FullTypeRange, TypeRange10);
using baseTypesUnsigned = ::testing::Types<unsigned char, unsigned short, unsigned int, unsigned long, unsigned long long>;
using baseTypes = ::testing::Types<char, short, int, long, long long, float, double, long double>;
INSTANTIATE_TYPED_TEST_SUITE_P(UnsignedTypes, RandomVectorFixture, baseTypesUnsigned);
INSTANTIATE_TYPED_TEST_SUITE_P(SignedTypes, RandomVectorFixture, baseTypes);
