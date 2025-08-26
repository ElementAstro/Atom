/*
 * test_generator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Async Generator
Tests coroutine generators, two-way generators, edge cases, exception handling, and iterator patterns.

**************************************************/

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <numeric>

#include "atom/async/utils/generator.hpp"
#include "../test_utils.hpp"
#include "../test_fixtures.hpp"

using namespace atom::async;

namespace atom::async::utils::test {

// ============================================================================
// Generator Tests
// ============================================================================

class GeneratorTest : public atom::async::test::AsyncTestBase {
protected:
    void SetUp() override {
        AsyncTestBase::SetUp();
    }

    void TearDown() override {
        AsyncTestBase::TearDown();
    }

    // Helper generator functions for testing
    Generator<int> simpleIntGenerator(int start, int end) {
        for (int i = start; i <= end; ++i) {
            co_yield i;
        }
    }

    Generator<std::string> stringGenerator() {
        co_yield "hello";
        co_yield "world";
        co_yield "test";
    }

    TwoWayGenerator<int, int> echoGenerator() {
        int received = 0;
        while (true) {
            received = co_yield received * 2;
        }
    }

    TwoWayGenerator<std::string, void> messageGenerator() {
        co_yield "first";
        co_yield "second";
        co_yield "third";
    }
};

// Test basic generator functionality
TEST_F(GeneratorTest, BasicGeneratorFunctionality) {
    auto gen = simpleIntGenerator(1, 5);
    
    std::vector<int> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }
    
    std::vector<int> expected = {1, 2, 3, 4, 5};
    EXPECT_EQ(values, expected);
}

// Test generator iterator interface
TEST_F(GeneratorTest, GeneratorIteratorInterface) {
    auto gen = simpleIntGenerator(10, 12);
    
    auto it = gen.begin();
    EXPECT_NE(it, gen.end());
    EXPECT_EQ(*it, 10);
    
    ++it;
    EXPECT_NE(it, gen.end());
    EXPECT_EQ(*it, 11);
    
    ++it;
    EXPECT_NE(it, gen.end());
    EXPECT_EQ(*it, 12);
    
    ++it;
    EXPECT_EQ(it, gen.end());
}

// Test empty generator
TEST_F(GeneratorTest, EmptyGenerator) {
    auto gen = simpleIntGenerator(1, 0); // Empty range
    
    auto it = gen.begin();
    EXPECT_EQ(it, gen.end());
    
    std::vector<int> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }
    EXPECT_TRUE(values.empty());
}

// Test string generator
TEST_F(GeneratorTest, StringGenerator) {
    auto gen = stringGenerator();
    
    std::vector<std::string> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }
    
    std::vector<std::string> expected = {"hello", "world", "test"};
    EXPECT_EQ(values, expected);
}

// Test generator with STL algorithms
TEST_F(GeneratorTest, GeneratorWithSTLAlgorithms) {
    auto gen = simpleIntGenerator(1, 10);
    
    std::vector<int> values;
    std::copy(gen.begin(), gen.end(), std::back_inserter(values));
    
    EXPECT_EQ(values.size(), 10);
    EXPECT_EQ(values.front(), 1);
    EXPECT_EQ(values.back(), 10);
    
    int sum = std::accumulate(values.begin(), values.end(), 0);
    EXPECT_EQ(sum, 55); // Sum of 1 to 10
}

// Test range utility function
TEST_F(GeneratorTest, RangeUtilityFunction) {
    auto gen = range(0, 5);
    
    std::vector<int> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }
    
    std::vector<int> expected = {0, 1, 2, 3, 4};
    EXPECT_EQ(values, expected);
}

// Test range with step
TEST_F(GeneratorTest, RangeWithStep) {
    auto gen = range(0, 10, 2);
    
    std::vector<int> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }
    
    std::vector<int> expected = {0, 2, 4, 6, 8};
    EXPECT_EQ(values, expected);
}

// Test range with negative step
TEST_F(GeneratorTest, RangeWithNegativeStep) {
    auto gen = range(10, 0, -2);
    
    std::vector<int> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }
    
    std::vector<int> expected = {10, 8, 6, 4, 2};
    EXPECT_EQ(values, expected);
}

// Test range with zero step (should throw)
TEST_F(GeneratorTest, RangeWithZeroStep) {
    EXPECT_THROW(range(0, 10, 0), std::invalid_argument);
}

// Test TwoWayGenerator basic functionality
TEST_F(GeneratorTest, TwoWayGeneratorBasicFunctionality) {
    auto gen = echoGenerator();
    
    // First call should return 0 (initial value * 2)
    int result1 = gen.next(5);
    EXPECT_EQ(result1, 0); // 0 * 2
    
    // Second call should return 10 (5 * 2)
    int result2 = gen.next(7);
    EXPECT_EQ(result2, 10); // 5 * 2
    
    // Third call should return 14 (7 * 2)
    int result3 = gen.next(3);
    EXPECT_EQ(result3, 14); // 7 * 2
}

// Test TwoWayGenerator with void receive type
TEST_F(GeneratorTest, TwoWayGeneratorVoidReceive) {
    auto gen = messageGenerator();
    
    EXPECT_FALSE(gen.done());
    
    std::string msg1 = gen.next();
    EXPECT_EQ(msg1, "first");
    EXPECT_FALSE(gen.done());
    
    std::string msg2 = gen.next();
    EXPECT_EQ(msg2, "second");
    EXPECT_FALSE(gen.done());
    
    std::string msg3 = gen.next();
    EXPECT_EQ(msg3, "third");
    EXPECT_TRUE(gen.done());
}

// Test TwoWayGenerator done state
TEST_F(GeneratorTest, TwoWayGeneratorDoneState) {
    auto gen = messageGenerator();
    
    // Consume all values
    while (!gen.done()) {
        gen.next();
    }
    
    EXPECT_TRUE(gen.done());
    EXPECT_THROW(gen.next(), std::logic_error);
}

// Test generator exception handling
TEST_F(GeneratorTest, GeneratorExceptionHandling) {
    auto throwingGenerator = []() -> Generator<int> {
        co_yield 1;
        co_yield 2;
        throw std::runtime_error("Generator exception");
        co_yield 3; // Should not be reached
    };
    
    auto gen = throwingGenerator();
    auto it = gen.begin();
    
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    
    // Next increment should throw
    EXPECT_THROW(++it, std::runtime_error);
}

// Test TwoWayGenerator exception handling
TEST_F(GeneratorTest, TwoWayGeneratorExceptionHandling) {
    auto throwingTwoWayGenerator = []() -> TwoWayGenerator<int, void> {
        co_yield 1;
        throw std::runtime_error("TwoWay generator exception");
        co_yield 2; // Should not be reached
    };
    
    auto gen = throwingTwoWayGenerator();
    
    EXPECT_EQ(gen.next(), 1);
    EXPECT_THROW(gen.next(), std::runtime_error);
}

// Test generator move semantics
TEST_F(GeneratorTest, GeneratorMoveSemantics) {
    auto gen1 = simpleIntGenerator(1, 3);
    auto gen2 = std::move(gen1);
    
    std::vector<int> values;
    for (const auto& value : gen2) {
        values.push_back(value);
    }
    
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(values, expected);
}

// Test generator with complex types
TEST_F(GeneratorTest, GeneratorWithComplexTypes) {
    auto complexGenerator = []() -> Generator<std::pair<int, std::string>> {
        co_yield std::make_pair(1, "one");
        co_yield std::make_pair(2, "two");
        co_yield std::make_pair(3, "three");
    };
    
    auto gen = complexGenerator();
    std::vector<std::pair<int, std::string>> values;
    
    for (const auto& value : gen) {
        values.push_back(value);
    }
    
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0].first, 1);
    EXPECT_EQ(values[0].second, "one");
    EXPECT_EQ(values[2].first, 3);
    EXPECT_EQ(values[2].second, "three");
}

// Test generator performance characteristics
TEST_F(GeneratorTest, GeneratorPerformance) {
    const int numValues = 10000;

    auto largeGenerator = []() -> Generator<int> {
        for (int i = 0; i < 10000; ++i) {
            co_yield i;
        }
    };

    auto timer = createTimer();
    auto gen = largeGenerator();

    int count = 0;
    for (const auto& value : gen) {
        count++;
        (void)value; // Suppress unused variable warning
    }

    auto elapsed = timer.elapsed();

    EXPECT_EQ(count, numValues);

    // Performance should be reasonable (this is a rough check)
    EXPECT_LT(elapsed.count(), 1000000); // Less than 1 second

    std::cout << "Generator performance: " << numValues << " values in "
              << elapsed.count() << " microseconds" << std::endl;
}

// Test generator with early termination
TEST_F(GeneratorTest, GeneratorEarlyTermination) {
    auto infiniteGenerator = []() -> Generator<int> {
        int i = 0;
        while (true) {
            co_yield i++;
        }
    };

    auto gen = infiniteGenerator();
    std::vector<int> values;

    // Take only first 5 values
    auto it = gen.begin();
    for (int i = 0; i < 5 && it != gen.end(); ++i, ++it) {
        values.push_back(*it);
    }

    std::vector<int> expected = {0, 1, 2, 3, 4};
    EXPECT_EQ(values, expected);
}

// Test generator resource cleanup
TEST_F(GeneratorTest, GeneratorResourceCleanup) {
    auto& tracker = getResourceTracker();

    {
        auto resourceGenerator = [&tracker]() -> Generator<int> {
            atom::async::test::ScopedResourceTracker resource(tracker);
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };

        auto gen = resourceGenerator();

        // Consume only part of the generator
        auto it = gen.begin();
        EXPECT_EQ(*it, 1);
        ++it;
        EXPECT_EQ(*it, 2);
        // Don't consume the third value, let generator go out of scope
    }

    // Give some time for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    tracker.expectNoLeaks();
}

// Test TwoWayGenerator with complex communication
TEST_F(GeneratorTest, TwoWayGeneratorComplexCommunication) {
    auto calculatorGenerator = []() -> TwoWayGenerator<int, int> {
        int accumulator = 0;
        while (true) {
            int input = co_yield accumulator;
            accumulator += input;
        }
    };

    auto gen = calculatorGenerator();

    // Initial value should be 0
    int result1 = gen.next(5);
    EXPECT_EQ(result1, 0);

    // Should return 5 (0 + 5)
    int result2 = gen.next(10);
    EXPECT_EQ(result2, 5);

    // Should return 15 (5 + 10)
    int result3 = gen.next(3);
    EXPECT_EQ(result3, 15);

    // Should return 18 (15 + 3)
    int result4 = gen.next(0);
    EXPECT_EQ(result4, 18);
}

// Test generator with conditional yields
TEST_F(GeneratorTest, GeneratorConditionalYields) {
    auto conditionalGenerator = [](bool includeEvens) -> Generator<int> {
        for (int i = 1; i <= 10; ++i) {
            if (includeEvens || i % 2 != 0) {
                co_yield i;
            }
        }
    };

    // Test with evens included
    auto genWithEvens = conditionalGenerator(true);
    std::vector<int> allValues;
    for (const auto& value : genWithEvens) {
        allValues.push_back(value);
    }
    EXPECT_EQ(allValues.size(), 10);

    // Test with only odds
    auto genOddsOnly = conditionalGenerator(false);
    std::vector<int> oddValues;
    for (const auto& value : genOddsOnly) {
        oddValues.push_back(value);
    }
    EXPECT_EQ(oddValues.size(), 5);
    std::vector<int> expectedOdds = {1, 3, 5, 7, 9};
    EXPECT_EQ(oddValues, expectedOdds);
}

// Test generator composition
TEST_F(GeneratorTest, GeneratorComposition) {
    auto firstGenerator = []() -> Generator<int> {
        co_yield 1;
        co_yield 2;
        co_yield 3;
    };

    auto secondGenerator = []() -> Generator<int> {
        co_yield 4;
        co_yield 5;
        co_yield 6;
    };

    auto composedGenerator = [&]() -> Generator<int> {
        auto gen1 = firstGenerator();
        for (const auto& value : gen1) {
            co_yield value;
        }

        auto gen2 = secondGenerator();
        for (const auto& value : gen2) {
            co_yield value;
        }
    };

    auto gen = composedGenerator();
    std::vector<int> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }

    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(values, expected);
}

// Test generator with state preservation
TEST_F(GeneratorTest, GeneratorStatePreservation) {
    auto statefulGenerator = []() -> Generator<int> {
        int state = 0;
        while (state < 5) {
            co_yield state * state;
            state++;
        }
    };

    auto gen = statefulGenerator();
    std::vector<int> values;
    for (const auto& value : gen) {
        values.push_back(value);
    }

    std::vector<int> expected = {0, 1, 4, 9, 16}; // Squares of 0, 1, 2, 3, 4
    EXPECT_EQ(values, expected);
}

}  // namespace atom::async::utils::test
