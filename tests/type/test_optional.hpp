// filepath: /home/max/Atom-1/atom/type/test_optional.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "optional.hpp"

using namespace atom::type;
using ::testing::Eq;

class ComplexTestType {
public:
    explicit ComplexTestType(int val = 0) : value(val) { instances++; }
    ComplexTestType(const ComplexTestType& other) : value(other.value) {
        instances++;
    }
    ComplexTestType(ComplexTestType&& other) noexcept : value(other.value) {
        other.value = 0;
        instances++;
    }

    ~ComplexTestType() { instances--; }

    ComplexTestType& operator=(const ComplexTestType& other) {
        if (this != &other) {
            value = other.value;
        }
        return *this;
    }

    ComplexTestType& operator=(ComplexTestType&& other) noexcept {
        if (this != &other) {
            value = other.value;
            other.value = 0;
        }
        return *this;
    }

    int getValue() const { return value; }
    void setValue(int val) { value = val; }

    bool operator==(const ComplexTestType& other) const {
        return value == other.value;
    }

    auto operator<=>(const ComplexTestType& other) const {
        return value <=> other.value;
    }

    static int instances;

private:
    int value;
};

int ComplexTestType::instances = 0;

class ThrowingType {
public:
    explicit ThrowingType(bool shouldThrow) {
        if (shouldThrow) {
            throw std::runtime_error("Constructor exception");
        }
    }

    ThrowingType& operator=(bool shouldThrow) {
        if (shouldThrow) {
            throw std::runtime_error("Assignment exception");
        }
        return *this;
    }
};

class OptionalTest : public ::testing::Test {
protected:
    void SetUp() override { ComplexTestType::instances = 0; }

    void TearDown() override {
        // Ensure all instances are cleaned up
        EXPECT_EQ(ComplexTestType::instances, 0);
    }
};

// Basic construction and value access tests
TEST_F(OptionalTest, DefaultConstruction) {
    Optional<ComplexTestType> opt;
    EXPECT_FALSE(opt.has_value());
    EXPECT_FALSE(static_cast<bool>(opt));
}

TEST_F(OptionalTest, NulloptConstruction) {
    Optional<ComplexTestType> opt(std::nullopt);
    EXPECT_FALSE(opt.has_value());
}

TEST_F(OptionalTest, ValueConstruction) {
    Optional<ComplexTestType> opt(ComplexTestType(42));
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt->getValue(), 42);
    EXPECT_EQ((*opt).getValue(), 42);
}

TEST_F(OptionalTest, CopyConstruction) {
    Optional<ComplexTestType> opt1(ComplexTestType(42));
    Optional<ComplexTestType> opt2(opt1);

    EXPECT_TRUE(opt1.has_value());
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt1->getValue(), 42);
    EXPECT_EQ(opt2->getValue(), 42);
}

TEST_F(OptionalTest, MoveConstruction) {
    Optional<ComplexTestType> opt1(ComplexTestType(42));
    Optional<ComplexTestType> opt2(std::move(opt1));

    EXPECT_FALSE(opt1.has_value());  // opt1 should be empty after move
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2->getValue(), 42);
}

// Assignment operators tests
TEST_F(OptionalTest, NulloptAssignment) {
    Optional<ComplexTestType> opt(ComplexTestType(42));
    opt = std::nullopt;
    EXPECT_FALSE(opt.has_value());
}

TEST_F(OptionalTest, CopyAssignment) {
    Optional<ComplexTestType> opt1(ComplexTestType(42));
    Optional<ComplexTestType> opt2;

    opt2 = opt1;
    EXPECT_TRUE(opt1.has_value());
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt1->getValue(), 42);
    EXPECT_EQ(opt2->getValue(), 42);

    // Self-assignment
    opt1 = opt1;
    EXPECT_TRUE(opt1.has_value());
    EXPECT_EQ(opt1->getValue(), 42);
}

TEST_F(OptionalTest, MoveAssignment) {
    Optional<ComplexTestType> opt1(ComplexTestType(42));
    Optional<ComplexTestType> opt2;

    opt2 = std::move(opt1);
    EXPECT_FALSE(opt1.has_value());  // opt1 should be empty after move
    EXPECT_TRUE(opt2.has_value());
    EXPECT_EQ(opt2->getValue(), 42);

    // Self-move assignment
    opt2 = std::move(opt2);
    EXPECT_TRUE(opt2.has_value());  // Self move shouldn't change state
    EXPECT_EQ(opt2->getValue(), 42);
}

TEST_F(OptionalTest, ValueAssignment) {
    Optional<ComplexTestType> opt;
    opt = ComplexTestType(42);

    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt->getValue(), 42);
}

// Emplace and reset tests
TEST_F(OptionalTest, Emplace) {
    Optional<ComplexTestType> opt;

    auto& ref = opt.emplace(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt->getValue(), 42);
    EXPECT_EQ(ref.getValue(), 42);

    // Emplace again, should destroy previous value
    opt.emplace(100);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt->getValue(), 100);
}

TEST_F(OptionalTest, Reset) {
    Optional<ComplexTestType> opt(ComplexTestType(42));

    EXPECT_TRUE(opt.has_value());
    opt.reset();
    EXPECT_FALSE(opt.has_value());
}

// Value access tests
TEST_F(OptionalTest, ValueAccess) {
    Optional<ComplexTestType> opt(ComplexTestType(42));

    EXPECT_EQ(opt.value().getValue(), 42);

    const Optional<ComplexTestType> const_opt(ComplexTestType(42));
    EXPECT_EQ(const_opt.value().getValue(), 42);

    Optional<ComplexTestType> empty_opt;
    EXPECT_THROW(empty_opt.value(), OptionalAccessError);
}

TEST_F(OptionalTest, ValueOr) {
    Optional<int> opt_with_value(42);
    EXPECT_EQ(opt_with_value.value_or(100), 42);

    Optional<int> empty_opt;
    EXPECT_EQ(empty_opt.value_or(100), 100);

    // Testing with move semantics
    Optional<std::unique_ptr<int>> opt_ptr(std::make_unique<int>(42));
    auto result = std::move(opt_ptr).value_or(std::make_unique<int>(100));
    EXPECT_EQ(*result, 42);
    EXPECT_FALSE(opt_ptr.has_value());  // Should be moved-from

    Optional<std::unique_ptr<int>> empty_opt_ptr;
    auto default_result =
        std::move(empty_opt_ptr).value_or(std::make_unique<int>(100));
    EXPECT_EQ(*default_result, 100);
}

// Comparison tests
TEST_F(OptionalTest, ComparisonWithOptional) {
    Optional<int> opt1(42);
    Optional<int> opt2(42);
    Optional<int> opt3(100);
    Optional<int> empty1;
    Optional<int> empty2;

    // Equality
    EXPECT_TRUE(opt1 == opt2);
    EXPECT_FALSE(opt1 == opt3);
    EXPECT_FALSE(opt1 == empty1);
    EXPECT_TRUE(empty1 == empty2);

    // Three-way comparison
    EXPECT_TRUE(opt1 == opt2);
    EXPECT_TRUE(opt1 < opt3);
    EXPECT_TRUE(opt3 > opt1);
    EXPECT_TRUE(opt1 > empty1);
    EXPECT_TRUE(empty1 < opt1);
    EXPECT_TRUE(empty1 == empty2);
}

TEST_F(OptionalTest, ComparisonWithNullopt) {
    Optional<int> opt(42);
    Optional<int> empty;

    EXPECT_FALSE(opt == std::nullopt);
    EXPECT_TRUE(empty == std::nullopt);

    EXPECT_TRUE(opt > std::nullopt);
    EXPECT_FALSE(empty > std::nullopt);
}

// Transformation function tests
TEST_F(OptionalTest, Map) {
    Optional<int> opt(42);
    auto mapped = opt.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, 84);

    Optional<int> empty;
    auto empty_mapped = empty.map([](int x) { return x * 2; });
    EXPECT_FALSE(empty_mapped.has_value());

    // Map with exception
    EXPECT_THROW(opt.map([](int) -> int { throw std::runtime_error("Test"); }),
                 OptionalOperationError);
}

TEST_F(OptionalTest, SimdMap) {
    Optional<int> opt(42);
    auto mapped = opt.simd_map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, 84);

    Optional<int> empty;
    auto empty_mapped = empty.simd_map([](int x) { return x * 2; });
    EXPECT_FALSE(empty_mapped.has_value());
}

TEST_F(OptionalTest, AndThen) {
    Optional<int> opt(42);
    auto result = opt.and_then([](int x) { return x * 2; });

    EXPECT_EQ(result, 84);

    Optional<int> empty;
    auto empty_result = empty.and_then([](int x) { return x * 2; });
    EXPECT_EQ(empty_result, 0);  // Default constructed int is 0

    // And_then with exception
    EXPECT_THROW(
        opt.and_then([](int) -> int { throw std::runtime_error("Test"); }),
        OptionalOperationError);
}

TEST_F(OptionalTest, Transform) {
    Optional<int> opt(42);
    auto transformed = opt.transform([](int x) { return x * 2; });

    EXPECT_TRUE(transformed.has_value());
    EXPECT_EQ(*transformed, 84);

    Optional<int> empty;
    auto empty_transformed = empty.transform([](int x) { return x * 2; });
    EXPECT_FALSE(empty_transformed.has_value());
}

TEST_F(OptionalTest, OrElse) {
    Optional<int> opt(42);
    auto result = opt.or_else([]() { return 100; });

    EXPECT_EQ(result, 42);

    Optional<int> empty;
    auto empty_result = empty.or_else([]() { return 100; });
    EXPECT_EQ(empty_result, 100);

    // Or_else with exception
    EXPECT_THROW(
        empty.or_else([]() -> int { throw std::runtime_error("Test"); }),
        OptionalOperationError);
}

TEST_F(OptionalTest, TransformOr) {
    Optional<int> opt(42);
    auto transformed = opt.transform_or([](int x) { return x * 2; }, 100);

    EXPECT_TRUE(transformed.has_value());
    EXPECT_EQ(*transformed, 84);

    Optional<int> empty;
    auto empty_transformed =
        empty.transform_or([](int x) { return x * 2; }, 100);
    EXPECT_TRUE(empty_transformed.has_value());
    EXPECT_EQ(*empty_transformed, 100);

    // Transform_or with exception
    EXPECT_THROW(opt.transform_or(
                     [](int) -> int { throw std::runtime_error("Test"); }, 100),
                 OptionalOperationError);
}

TEST_F(OptionalTest, FlatMap) {
    Optional<int> opt(42);
    auto result = opt.flat_map([](int x) { return x * 2; });

    EXPECT_EQ(result, 84);

    Optional<int> empty;
    auto empty_result = empty.flat_map([](int x) { return x * 2; });
    EXPECT_EQ(empty_result, 0);  // Default constructed int is 0
}

TEST_F(OptionalTest, IfHasValue) {
    Optional<int> opt(42);
    int value = 0;

    opt.if_has_value([&value](int x) { value = x * 2; });
    EXPECT_EQ(value, 84);

    value = 0;
    Optional<int> empty;
    empty.if_has_value([&value](int x) { value = x * 2; });
    EXPECT_EQ(value, 0);  // Should not change

    // If_has_value with exception
    EXPECT_THROW(
        opt.if_has_value([](int) { throw std::runtime_error("Test"); }),
        OptionalOperationError);
}

// Thread safety tests
TEST(OptionalThreadSafetyTest, ConcurrentAccess) {
    Optional<int> opt(42);
    std::atomic<int> success_count = 0;
    std::atomic<int> exception_count = 0;

    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int iterations = 1000;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&opt, &success_count, &exception_count, i]() {
            for (int j = 0; j < iterations; ++j) {
                if (i % 2 == 0) {
                    // Even threads read and write
                    try {
                        int value = *opt;
                        opt = value + 1;
                        success_count++;
                    } catch (const OptionalAccessError&) {
                        exception_count++;
                    }
                } else {
                    // Odd threads reset and emplace
                    try {
                        if (j % 2 == 0) {
                            opt.reset();
                        } else {
                            opt.emplace(i * 100 + j);
                        }
                        success_count++;
                    } catch (...) {
                        exception_count++;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // We don't assert specific values because the final state depends on thread
    // interleaving Just ensure all operations completed
    EXPECT_EQ(success_count + exception_count, num_threads * iterations);
}

// Exception safety tests
TEST_F(OptionalTest, ExceptionSafetyConstruction) {
    // Constructor that throws
    EXPECT_THROW(Optional<ThrowingType>(ThrowingType(true)),
                 std::runtime_error);

    // Constructor that doesn't throw
    EXPECT_NO_THROW(Optional<ThrowingType>(ThrowingType(false)));
}

TEST_F(OptionalTest, ExceptionSafetyAssignment) {
    Optional<ThrowingType> opt(ThrowingType(false));

    // Assignment that throws
    EXPECT_THROW(opt = ThrowingType(true), std::runtime_error);
}

TEST_F(OptionalTest, ExceptionSafetyEmplace) {
    Optional<ThrowingType> opt;

    // Emplace that throws
    EXPECT_THROW(opt.emplace(true), std::runtime_error);

    // Emplace that doesn't throw
    EXPECT_NO_THROW(opt.emplace(false));
}

// Utility function tests
TEST_F(OptionalTest, MakeOptionalValue) {
    auto opt = make_optional(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, 42);
}

TEST_F(OptionalTest, MakeOptionalConstruct) {
    auto opt = make_optional<ComplexTestType>(42);
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt->getValue(), 42);
}

// Performance test (basic benchmark)
TEST(OptionalPerformanceTest, BasicBenchmark) {
    const int iterations = 1000000;

    auto start = std::chrono::high_resolution_clock::now();

    Optional<int> opt;
    for (int i = 0; i < iterations; ++i) {
        opt = i;
        int value = *opt;
        opt.reset();
        static_cast<void>(value);  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Just output the time, don't assert on it as it depends on the system
    std::cout << "Optional performance test: " << duration << "ms for "
              << iterations << " iterations" << std::endl;
}

// Compare with std::optional performance
TEST(OptionalPerformanceTest, CompareWithStdOptional) {
    const int iterations = 1000000;

    // Benchmark atom::type::Optional
    auto start1 = std::chrono::high_resolution_clock::now();

    Optional<int> custom_opt;
    for (int i = 0; i < iterations; ++i) {
        custom_opt = i;
        int value = *custom_opt;
        custom_opt.reset();
        static_cast<void>(value);  // Prevent optimization
    }

    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 =
        std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1)
            .count();

    // Benchmark std::optional
    auto start2 = std::chrono::high_resolution_clock::now();

    std::optional<int> std_opt;
    for (int i = 0; i < iterations; ++i) {
        std_opt = i;
        int value = *std_opt;
        std_opt.reset();
        static_cast<void>(value);  // Prevent optimization
    }

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 =
        std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2)
            .count();

    std::cout << "Custom Optional: " << duration1
              << "ms, std::optional: " << duration2 << "ms for " << iterations
              << " iterations" << std::endl;
    std::cout << "Overhead factor: "
              << static_cast<double>(duration1) / duration2 << "x" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}