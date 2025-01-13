#ifndef ATOM_ALGORITHM_TEST_HASH_HPP
#define ATOM_ALGORITHM_TEST_HASH_HPP

#include "atom/algorithm/hash.hpp"
#include <gtest/gtest.h>
#include <array>
#include <optional>
#include <string>
#include <tuple>
#include <variant>

using namespace atom::algorithm;

class HashTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test HashCache operations
TEST_F(HashTest, HashCacheOperations) {
    HashCache<std::string> cache;

    cache.set("test", 12345);
    auto result = cache.get("test");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 12345);

    cache.clear();
    result = cache.get("test");
    EXPECT_FALSE(result.has_value());
}

// Test basic hash computation
TEST_F(HashTest, BasicHashComputation) {
    EXPECT_NE(computeHash(42), 0);
    EXPECT_NE(computeHash(3.14), 0);
    EXPECT_NE(computeHash(std::string("test")), 0);
}

// Test different hash algorithms
TEST_F(HashTest, HashAlgorithms) {
    int value = 42;
    auto stdHash = computeHash(value, HashAlgorithm::STD);
    auto fnv1aHash = computeHash(value, HashAlgorithm::FNV1A);
    EXPECT_NE(stdHash, fnv1aHash);
}

// Test vector hashing
TEST_F(HashTest, VectorHashing) {
    std::vector<int> vec1 = {1, 2, 3};
    std::vector<int> vec2 = {1, 2, 3};
    std::vector<int> vec3 = {1, 2, 4};

    EXPECT_EQ(computeHash(vec1), computeHash(vec2));
    EXPECT_NE(computeHash(vec1), computeHash(vec3));

    // Test parallel hashing
    std::vector<int> largeVec(10000, 1);
    EXPECT_NO_THROW(computeHash(largeVec, true));
}

// Test array hashing
TEST_F(HashTest, ArrayHashing) {
    std::array<int, 3> arr1 = {1, 2, 3};
    std::array<int, 3> arr2 = {1, 2, 3};
    std::array<int, 3> arr3 = {1, 2, 4};

    EXPECT_EQ(computeHash(arr1), computeHash(arr2));
    EXPECT_NE(computeHash(arr1), computeHash(arr3));
}

// Test tuple hashing
TEST_F(HashTest, TupleHashing) {
    auto tuple1 = std::make_tuple(1, "test", 3.14);
    auto tuple2 = std::make_tuple(1, "test", 3.14);
    auto tuple3 = std::make_tuple(1, "test", 3.15);

    EXPECT_EQ(computeHash(tuple1), computeHash(tuple2));
    EXPECT_NE(computeHash(tuple1), computeHash(tuple3));
}

// Test pair hashing
TEST_F(HashTest, PairHashing) {
    auto pair1 = std::make_pair(1, "test");
    auto pair2 = std::make_pair(1, "test");
    auto pair3 = std::make_pair(1, "test2");

    EXPECT_EQ(computeHash(pair1), computeHash(pair2));
    EXPECT_NE(computeHash(pair1), computeHash(pair3));
}

// Test optional hashing
TEST_F(HashTest, OptionalHashing) {
    std::optional<int> opt1 = 42;
    std::optional<int> opt2 = 42;
    std::optional<int> opt3;

    EXPECT_EQ(computeHash(opt1), computeHash(opt2));
    EXPECT_NE(computeHash(opt1), computeHash(opt3));
}

// Test variant hashing
TEST_F(HashTest, VariantHashing) {
    std::variant<int, std::string> var1 = 42;
    std::variant<int, std::string> var2 = 42;
    std::variant<int, std::string> var3 = "42";

    EXPECT_EQ(computeHash(var1), computeHash(var2));
    EXPECT_NE(computeHash(var1), computeHash(var3));
}

// Test any hashing
TEST_F(HashTest, AnyHashing) {
    std::any any1 = 42;
    std::any any2 = 42;
    std::any any3 = std::string("42");

    EXPECT_EQ(computeHash(any1), computeHash(any2));
    EXPECT_NE(computeHash(any1), computeHash(any3));
}

// Test string literal hash operator
TEST_F(HashTest, StringLiteralHashOperator) {
    auto hash1 = "test"_hash;
    auto hash2 = "test"_hash;
    auto hash3 = "test2"_hash;

    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
}

// Test hash verification
TEST_F(HashTest, HashVerification) {
    auto hash1 = computeHash(42);
    auto hash2 = computeHash(42);
    auto hash3 = computeHash(43);

    EXPECT_TRUE(verifyHash(hash1, hash2));
    EXPECT_FALSE(verifyHash(hash1, hash3));
    EXPECT_TRUE(verifyHash(hash1, hash3, 1000));  // With tolerance
}

// Test thread safety
TEST_F(HashTest, ThreadSafety) {
    const int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<size_t> results(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&results, i] { results[i] = computeHash(42); });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 1; i < numThreads; ++i) {
        EXPECT_EQ(results[0], results[i]);
    }
}

// Test edge cases
TEST_F(HashTest, EdgeCases) {
    // Empty containers
    EXPECT_EQ(computeHash(std::vector<int>{}), 0);
    EXPECT_EQ(computeHash(std::array<int, 0>{}), 0);

    // Empty optional/any
    EXPECT_EQ(computeHash(std::optional<int>{}), 0);
    EXPECT_EQ(computeHash(std::any{}), 0);

    // Empty string
    EXPECT_NE(computeHash(""), 0);
}

// Test hash combination
TEST_F(HashTest, HashCombination) {
    size_t seed = 0;
    size_t hash1 = computeHash(42);
    size_t hash2 = computeHash("test");

    size_t combined1 = hashCombine(seed, hash1);
    size_t combined2 = hashCombine(combined1, hash2);

    EXPECT_NE(combined1, combined2);
    EXPECT_NE(combined1, hash1);
    EXPECT_NE(combined2, hash2);
}

#endif  // ATOM_ALGORITHM_TEST_HASH_HPP