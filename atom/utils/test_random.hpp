// filepath: /home/max/Atom-1/atom/utils/test_random.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <numeric>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "random.hpp"

using namespace atom::utils;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Ge;
using ::testing::Le;

class RandomTest : public ::testing::Test {
protected:
    // Constants for tests
    const int DEFAULT_TEST_LENGTH = 1000;
    const int TEST_ITERATIONS = 100;
    const int RANDOM_SEED = 12345;

    // Custom character sets for testing
    const std::string NUMERIC_CHARSET = "0123456789";
    const std::string ALPHA_CHARSET =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string SPECIAL_CHARSET = "!@#$%^&*()_+-=[]{}|;:,.<>?";
};

// Test the RandomEngine concept
TEST_F(RandomTest, RandomEngineConcept) {
    // Check that std::mt19937 satisfies the RandomEngine concept
    EXPECT_TRUE(
        (std::is_same_v<std::true_type,
                        std::bool_constant<RandomEngine<std::mt19937>>>));
    EXPECT_TRUE(
        (std::is_same_v<std::true_type,
                        std::bool_constant<RandomEngine<std::minstd_rand0>>>));
    EXPECT_TRUE(
        (std::is_same_v<std::true_type,
                        std::bool_constant<RandomEngine<std::ranlux24_base>>>));

    // Check that non-engine types don't satisfy the RandomEngine concept
    struct NotAnEngine {
        int operator()() { return 42; }
    };
    EXPECT_FALSE(
        (std::is_same_v<std::true_type,
                        std::bool_constant<RandomEngine<NotAnEngine>>>));
}

// Test the RandomDistribution concept
TEST_F(RandomTest, RandomDistributionConcept) {
    // Check that standard distributions satisfy the RandomDistribution concept
    EXPECT_TRUE((std::is_same_v<std::true_type,
                                std::bool_constant<RandomDistribution<
                                    std::uniform_int_distribution<int>>>>));
    EXPECT_TRUE((std::is_same_v<std::true_type,
                                std::bool_constant<RandomDistribution<
                                    std::uniform_real_distribution<double>>>>));
    EXPECT_TRUE((
        std::is_same_v<std::true_type, std::bool_constant<RandomDistribution<
                                           std::normal_distribution<float>>>>));

    // Check that non-distribution types don't satisfy the RandomDistribution
    // concept
    struct NotADistribution {
        // Missing result_type and param_type
        int operator()() { return 42; }
    };
    EXPECT_FALSE((std::is_same_v<
                  std::true_type,
                  std::bool_constant<RandomDistribution<NotADistribution>>>));
}

// Test Random class constructor with min-max parameters
TEST_F(RandomTest, RandomConstructorMinMax) {
    // Valid min-max
    EXPECT_NO_THROW(
        (Random<std::mt19937, std::uniform_int_distribution<int>>(1, 100)));

    // Equal min-max
    EXPECT_NO_THROW(
        (Random<std::mt19937, std::uniform_int_distribution<int>>(50, 50)));

    // Invalid min-max (min > max)
    EXPECT_THROW(
        (Random<std::mt19937, std::uniform_int_distribution<int>>(100, 1)),
        atom::error::Exception);
}

// Test Random class with seed constructor
TEST_F(RandomTest, RandomConstructorWithSeed) {
    // Create two generators with the same seed
    Random<std::mt19937, std::uniform_int_distribution<int>> gen1(RANDOM_SEED,
                                                                  1, 100);
    Random<std::mt19937, std::uniform_int_distribution<int>> gen2(RANDOM_SEED,
                                                                  1, 100);

    // Both generators should produce the same sequence
    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        EXPECT_EQ(gen1(), gen2());
    }
}

// Test Random class operator() method
TEST_F(RandomTest, RandomOperatorCall) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    // Generate random values and check they are within bounds
    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        int value = gen();
        EXPECT_GE(value, 1);
        EXPECT_LE(value, 100);
    }
}

// Test Random class operator() with param method
TEST_F(RandomTest, RandomOperatorCallWithParam) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    // Create a different param and use it
    std::uniform_int_distribution<int>::param_type param(200, 300);

    // Generate random values with custom param and check they are within bounds
    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        int value = gen(param);
        EXPECT_GE(value, 200);
        EXPECT_LE(value, 300);
    }
}

// Test Random class generate method with range
TEST_F(RandomTest, RandomGenerateRange) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    std::vector<int> values(TEST_ITERATIONS);
    gen.generate(values);

    // Check that all values are within bounds
    EXPECT_THAT(values, Each(AllOf(Ge(1), Le(100))));
}

// Test Random class generate method with iterators
TEST_F(RandomTest, RandomGenerateIterators) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    std::vector<int> values(TEST_ITERATIONS);
    gen.generate(values.begin(), values.end());

    // Check that all values are within bounds
    EXPECT_THAT(values, Each(AllOf(Ge(1), Le(100))));
}

// Test Random class vector method
TEST_F(RandomTest, RandomVector) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    auto values = gen.vector(TEST_ITERATIONS);

    // Check vector size and bounds
    EXPECT_EQ(values.size(), static_cast<size_t>(TEST_ITERATIONS));
    EXPECT_THAT(values, Each(AllOf(Ge(1), Le(100))));
}

// Test Random class vector method with excessive size
TEST_F(RandomTest, RandomVectorExcessiveSize) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    // This should be handled safely, either by throwing an exception or by
    // returning a vector of appropriate size
    EXPECT_THROW((gen.vector(std::numeric_limits<size_t>::max())),
                 atom::error::Exception);
}

// Test Random::range static method
TEST_F(RandomTest, RandomRangeStatic) {
    auto values =
        Random<std::mt19937, std::uniform_int_distribution<int>>::range(
            TEST_ITERATIONS, 1, 100);

    // Check vector size and bounds
    EXPECT_EQ(values.size(), static_cast<size_t>(TEST_ITERATIONS));
    EXPECT_THAT(values, Each(AllOf(Ge(1), Le(100))));
}

// Test Random::range with invalid arguments
TEST_F(RandomTest, RandomRangeInvalidArgs) {
    EXPECT_THROW(
        (Random<std::mt19937, std::uniform_int_distribution<int>>::range(
            TEST_ITERATIONS, 100, 1)),
        atom::error::Exception);
}

// Test Random with floating point distribution
TEST_F(RandomTest, RandomWithFloatingPointDistribution) {
    Random<std::mt19937, std::uniform_real_distribution<double>> gen(0.0, 1.0);

    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        double value = gen();
        EXPECT_GE(value, 0.0);
        EXPECT_LE(value, 1.0);
    }
}

// Test Random with normal distribution
TEST_F(RandomTest, RandomWithNormalDistribution) {
    // Normal distribution with mean 0 and stddev 1
    Random<std::mt19937, std::normal_distribution<double>> gen(0.0, 1.0);

    // Generate many values to check distribution properties
    std::vector<double> values(DEFAULT_TEST_LENGTH);
    gen.generate(values);

    // Calculate mean and standard deviation
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();

    double sq_sum =
        std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
    double stddev = std::sqrt(sq_sum / values.size() - mean * mean);

    // Check that mean and stddev are roughly as expected (allow some margin for
    // randomness)
    EXPECT_NEAR(mean, 0.0, 0.2);
    EXPECT_NEAR(stddev, 1.0, 0.2);
}

// Test generateRandomString function
TEST_F(RandomTest, GenerateRandomString) {
    // Test with default charset
    std::string str = generateRandomString(DEFAULT_TEST_LENGTH);
    EXPECT_EQ(str.length(), static_cast<size_t>(DEFAULT_TEST_LENGTH));

    // Check that all chars are from the default charset
    const std::string& defaultCharset =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (char c : str) {
        EXPECT_NE(defaultCharset.find(c), std::string::npos);
    }
}

// Test generateRandomString with custom charset
TEST_F(RandomTest, GenerateRandomStringCustomCharset) {
    // Test with custom charset
    std::string str =
        generateRandomString(DEFAULT_TEST_LENGTH, NUMERIC_CHARSET);
    EXPECT_EQ(str.length(), static_cast<size_t>(DEFAULT_TEST_LENGTH));

    // Check that all chars are from the custom charset
    for (char c : str) {
        EXPECT_NE(NUMERIC_CHARSET.find(c), std::string::npos);
    }
}

// Test generateRandomString with invalid length
TEST_F(RandomTest, GenerateRandomStringInvalidLength) {
    EXPECT_THROW(generateRandomString(0), std::invalid_argument);
    EXPECT_THROW(generateRandomString(-10), std::invalid_argument);
}

// Test generateRandomString with empty charset
TEST_F(RandomTest, GenerateRandomStringEmptyCharset) {
    EXPECT_THROW(generateRandomString(10, ""), std::invalid_argument);
}

// Test generateSecureRandomString function
TEST_F(RandomTest, GenerateSecureRandomString) {
    // Test with default parameters
    std::string str = generateSecureRandomString(DEFAULT_TEST_LENGTH);
    EXPECT_EQ(str.length(), static_cast<size_t>(DEFAULT_TEST_LENGTH));

    // Check that all chars are from the default charset
    const std::string& defaultCharset =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (char c : str) {
        EXPECT_NE(defaultCharset.find(c), std::string::npos);
    }
}

// Test generateSecureRandomString with invalid length
TEST_F(RandomTest, GenerateSecureRandomStringInvalidLength) {
    EXPECT_THROW(generateSecureRandomString(0), std::invalid_argument);
    EXPECT_THROW(generateSecureRandomString(-10), std::invalid_argument);
}

// Test secureShuffleRange function
TEST_F(RandomTest, SecureShuffleRange) {
    // Create a vector with sequential integers
    std::vector<int> vec(DEFAULT_TEST_LENGTH);
    std::iota(vec.begin(), vec.end(), 0);
    std::vector<int> original = vec;

    // Shuffle the vector
    secureShuffleRange(vec);

    // Check that all elements are still there, just in different order
    std::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, original);

    // There's a very small probability that shuffle doesn't change anything
    // so we shuffle again to make sure
    std::vector<int> vec2 = original;
    secureShuffleRange(vec2);
    EXPECT_NE(vec2, original);  // This might fail in extremely rare cases
}

// Test Random class with different engines
TEST_F(RandomTest, RandomWithDifferentEngines) {
    // Test with different engines
    Random<std::minstd_rand, std::uniform_int_distribution<int>> gen1(1, 100);
    Random<std::mt19937_64, std::uniform_int_distribution<int>> gen2(1, 100);
    Random<std::ranlux24, std::uniform_int_distribution<int>> gen3(1, 100);

    // All should generate values within bounds
    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        EXPECT_GE(gen1(), 1);
        EXPECT_LE(gen1(), 100);

        EXPECT_GE(gen2(), 1);
        EXPECT_LE(gen2(), 100);

        EXPECT_GE(gen3(), 1);
        EXPECT_LE(gen3(), 100);
    }
}

// Test Random class param method
TEST_F(RandomTest, RandomParamMethod) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    // Create a different param and set it
    std::uniform_int_distribution<int>::param_type param(200, 300);
    gen.param(param);

    // Generate random values with the new param and check bounds
    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        int value = gen();
        EXPECT_GE(value, 200);
        EXPECT_LE(value, 300);
    }
}

// Test Random class seed method
TEST_F(RandomTest, RandomSeedMethod) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen1(1, 100);
    Random<std::mt19937, std::uniform_int_distribution<int>> gen2(1, 100);

    // Generate a few values to advance the first generator
    for (int i = 0; i < 10; ++i) {
        gen1();
    }

    // Now re-seed both generators with the same seed
    int seed_value = 42;
    gen1.seed(seed_value);
    gen2.seed(seed_value);

    // Both should produce identical sequences now
    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        EXPECT_EQ(gen1(), gen2());
    }
}

// Test Random class engine and distribution accessors
TEST_F(RandomTest, RandomAccessors) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    // Test engine accessor
    auto& engine = gen.engine();
    EXPECT_EQ(typeid(engine), typeid(std::mt19937));

    // Test distribution accessor
    auto& dist = gen.distribution();
    EXPECT_EQ(typeid(dist), typeid(std::uniform_int_distribution<int>));
}

// Test RandomEngine with different result types
TEST_F(RandomTest, RandomEngineWithDifferentResultTypes) {
    // uint32_t result type
    Random<std::mt19937, std::uniform_int_distribution<uint32_t>> gen_uint32(
        1, 100);
    EXPECT_NO_THROW(gen_uint32());

    // int64_t result type
    Random<std::mt19937_64, std::uniform_int_distribution<int64_t>> gen_int64(
        1, 100);
    EXPECT_NO_THROW(gen_int64());
}

// Test distribution quality (Chi-squared test)
TEST_F(RandomTest, DistributionQuality) {
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 10);

    // Generate a large number of values to test distribution
    constexpr int num_samples = 10000;
    constexpr int num_bins = 10;
    std::array<int, num_bins> bins{};

    for (int i = 0; i < num_samples; ++i) {
        int value = gen();
        bins[value - 1]++;  // -1 because values start at 1
    }

    // Expected count per bin for a uniform distribution
    double expected = static_cast<double>(num_samples) / num_bins;

    // Calculate chi-squared statistic
    double chi_squared = 0;
    for (int count : bins) {
        double diff = count - expected;
        chi_squared += (diff * diff) / expected;
    }

    // For 9 degrees of freedom (10 bins - 1) and 95% confidence,
    // chi-squared should be less than 16.92
    EXPECT_LE(chi_squared, 16.92);
}

// Test thread safety of Random class
TEST_F(RandomTest, RandomThreadSafety) {
    // Create Random instance shared among threads
    Random<std::mt19937, std::uniform_int_distribution<int>> gen(1, 100);

    // Run multiple threads that each generate random numbers
    constexpr int num_threads = 10;
    constexpr int num_samples_per_thread = 1000;

    std::vector<std::thread> threads;
    std::vector<std::vector<int>> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&gen, i, &results]() {
            results[i].resize(num_samples_per_thread);
            for (int j = 0; j < num_samples_per_thread; ++j) {
                results[i][j] = gen();
            }
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Check that all values are within bounds
    for (const auto& thread_results : results) {
        EXPECT_EQ(thread_results.size(),
                  static_cast<size_t>(num_samples_per_thread));
        for (int value : thread_results) {
            EXPECT_GE(value, 1);
            EXPECT_LE(value, 100);
        }
    }
}

// Test thread safety of generateRandomString
TEST_F(RandomTest, GenerateRandomStringThreadSafety) {
    constexpr int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<std::string> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(
            [i, &results]() { results[i] = generateRandomString(100); });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Check that all strings have correct length and valid characters
    const std::string& defaultCharset =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (const auto& str : results) {
        EXPECT_EQ(str.length(), 100);
        for (char c : str) {
            EXPECT_NE(defaultCharset.find(c), std::string::npos);
        }
    }

    // Strings should be different from each other
    std::set<std::string> unique_strings(results.begin(), results.end());
    EXPECT_EQ(unique_strings.size(), results.size());
}

// Test thread safety of generateSecureRandomString
TEST_F(RandomTest, GenerateSecureRandomStringThreadSafety) {
    constexpr int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<std::string> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(
            [i, &results]() { results[i] = generateSecureRandomString(100); });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Check that all strings have correct length and valid characters
    const std::string& defaultCharset =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (const auto& str : results) {
        EXPECT_EQ(str.length(), 100);
        for (char c : str) {
            EXPECT_NE(defaultCharset.find(c), std::string::npos);
        }
    }

    // Strings should be different from each other
    std::set<std::string> unique_strings(results.begin(), results.end());
    EXPECT_EQ(unique_strings.size(), results.size());
}

// Test secureShuffleRange thread safety
TEST_F(RandomTest, SecureShuffleRangeThreadSafety) {
    constexpr int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<std::vector<int>> results(num_threads);

    // Initialize each vector with sequential integers
    for (int i = 0; i < num_threads; ++i) {
        results[i].resize(100);
        std::iota(results[i].begin(), results[i].end(), 0);
    }

    // Shuffle each vector in its own thread
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(
            [i, &results]() { secureShuffleRange(results[i]); });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Create a vector with the original content to compare
    std::vector<int> original(100);
    std::iota(original.begin(), original.end(), 0);

    // Check that each shuffled vector has the same elements as original, but in
    // different order
    for (const auto& vec : results) {
        std::vector<int> sorted_vec = vec;
        std::sort(sorted_vec.begin(), sorted_vec.end());
        EXPECT_EQ(sorted_vec, original);
    }
}

// Performance test for generateRandomString
TEST_F(RandomTest, GenerateRandomStringPerformance) {
    // Measure time to generate a large random string
    auto start = std::chrono::high_resolution_clock::now();

    constexpr int large_length = 1000000;
    std::string large_string = generateRandomString(large_length);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Just log the time, don't assert on it as it will vary by system
    std::cout << "Generated " << large_length << " char string in "
              << duration_ms << "ms" << std::endl;

    // Check that the string has the correct length
    EXPECT_EQ(large_string.length(), static_cast<size_t>(large_length));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}