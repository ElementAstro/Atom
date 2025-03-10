#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/algorithm/hash.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Helper function to generate random strings
inline std::string generateRandomString(size_t length, bool onlyAscii = true) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::string result;
    result.reserve(length);

    if (onlyAscii) {
        // Generate ASCII characters from 32 to 126
        std::uniform_int_distribution<> dis(32, 126);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dis(gen)));
        }
    } else {
        // Generate any valid char value
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dis(gen)));
        }
    }

    return result;
}

// Test fixture for hash tests
class HashTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing if needed
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }
};

// Basic hash function tests
TEST_F(HashTest, BasicStringHash) {
    // Test basic hashing of strings
    const char* str1 = "Hello, World!";
    const char* str2 = "Hello, World!";
    const char* str3 = "Different string";

    // Same strings should hash to same value
    EXPECT_EQ(atom::algorithm::hash(str1), atom::algorithm::hash(str2));

    // Different strings should hash to different values
    EXPECT_NE(atom::algorithm::hash(str1), atom::algorithm::hash(str3));
}

TEST_F(HashTest, HashLiteralOperator) {
    // Test the string literal hash operator
    auto hash1 = "test string"_hash;
    auto hash2 = "test string"_hash;
    auto hash3 = "different"_hash;

    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);

    // Verify the operator gives same results as function
    EXPECT_EQ("hello"_hash, atom::algorithm::hash("hello"));
}

TEST_F(HashTest, EmptyStringHash) {
    // Empty strings should have a consistent hash
    EXPECT_EQ(atom::algorithm::hash(""), atom::algorithm::hash(""));

    // Empty should be different from non-empty
    EXPECT_NE(atom::algorithm::hash(""), atom::algorithm::hash("a"));
}

// HashCache tests
TEST_F(HashTest, HashCacheBasic) {
    HashCache<std::string> cache;

    // Initial get should return nullopt
    EXPECT_EQ(cache.get("test"), std::nullopt);

    // Set and get
    cache.set("test", 12345);
    auto value = cache.get("test");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 12345);

    // Update and get
    cache.set("test", 67890);
    value = cache.get("test");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 67890);

    // Clear should remove all values
    cache.clear();
    EXPECT_EQ(cache.get("test"), std::nullopt);
}

TEST_F(HashTest, HashCacheMultipleTypes) {
    HashCache<int> intCache;
    HashCache<std::string> strCache;

    intCache.set(42, 12345);
    strCache.set("hello", 67890);

    auto intValue = intCache.get(42);
    auto strValue = strCache.get("hello");

    ASSERT_TRUE(intValue.has_value());
    ASSERT_TRUE(strValue.has_value());
    EXPECT_EQ(*intValue, 12345);
    EXPECT_EQ(*strValue, 67890);
}

// ThreadSafety test for HashCache
TEST_F(HashTest, HashCacheThreadSafety) {
    HashCache<std::string> cache;
    std::atomic<bool> stop{false};

    // Start multiple reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; ++i) {
        readers.emplace_back([&]() {
            while (!stop) {
                cache.get("test");
                std::this_thread::yield();
            }
        });
    }

    // Start multiple writer threads
    std::vector<std::thread> writers;
    for (int i = 0; i < 5; ++i) {
        writers.emplace_back([&, i]() {
            for (int j = 0; j < 100; ++j) {
                cache.set("test", i * 1000 + j);
                std::this_thread::yield();
            }
        });
    }

    // Let the threads run for a short time
    std::this_thread::sleep_for(100ms);
    stop = true;

    // Join all threads
    for (auto& t : readers) {
        t.join();
    }
    for (auto& t : writers) {
        t.join();
    }

    // Test passes if no exceptions were thrown
}

// ComputeHash tests for basic types
TEST_F(HashTest, ComputeHashBasicTypes) {
    // Test hashing of basic types
    EXPECT_EQ(computeHash(42), std::hash<int>{}(42));
    EXPECT_EQ(computeHash(3.14), std::hash<double>{}(3.14));
    EXPECT_EQ(computeHash(std::string("hello")),
              std::hash<std::string>{}("hello"));
    EXPECT_EQ(computeHash(true), std::hash<bool>{}(true));
}

TEST_F(HashTest, ComputeHashWithDifferentAlgorithms) {
    std::string testStr = "test string";

    // Test different algorithms return different values
    auto hashStd = computeHash(testStr, HashAlgorithm::STD);
    auto hashFnv1a = computeHash(testStr, HashAlgorithm::FNV1A);

    // These should generally be different
    EXPECT_NE(hashStd, hashFnv1a);

    // Using the same algorithm twice should yield same result
    EXPECT_EQ(computeHash(testStr, HashAlgorithm::STD),
              computeHash(testStr, HashAlgorithm::STD));
}

TEST_F(HashTest, ComputeHashCaching) {
    std::string testStr = "test string for caching";

    // First call should compute the hash
    auto hash1 = computeHash(testStr);

    // Second call should use the cached value
    auto start = std::chrono::high_resolution_clock::now();
    auto hash2 = computeHash(testStr);
    auto end = std::chrono::high_resolution_clock::now();

    // Hashes should be the same
    EXPECT_EQ(hash1, hash2);

    // Second call should be much faster due to caching
    // This is hard to test reliably, so we just check it's reasonably fast
    auto duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();
    std::cout << "Cache lookup duration: " << duration << " ns" << std::endl;

    // Most cache lookups should be under 1000ns
    // However, this might not be reliable on all systems
    // EXPECT_LT(duration, 1000);
}

// Tests for container types
TEST_F(HashTest, ComputeHashVector) {
    std::vector<int> v1 = {1, 2, 3, 4, 5};
    std::vector<int> v2 = {1, 2, 3, 4, 5};
    std::vector<int> v3 = {5, 4, 3, 2, 1};

    // Same vectors should hash to same value
    EXPECT_EQ(computeHash(v1), computeHash(v2));

    // Different vectors should hash differently
    EXPECT_NE(computeHash(v1), computeHash(v3));

    // Empty vector should have consistent hash
    std::vector<int> empty1, empty2;
    EXPECT_EQ(computeHash(empty1), computeHash(empty2));
}

TEST_F(HashTest, ComputeHashVectorParallel) {
    // Create large vectors to test parallel hashing
    std::vector<int> largeVector1(10000, 42);
    std::vector<int> largeVector2(10000, 42);
    std::vector<int> largeVector3(10000, 43);

    // Sequential vs parallel should yield same result for same vectors
    EXPECT_EQ(computeHash(largeVector1, false),  // Sequential
              computeHash(largeVector1, true));  // Parallel

    // Same large vectors should hash to same value
    EXPECT_EQ(computeHash(largeVector1, true), computeHash(largeVector2, true));

    // Different large vectors should hash differently
    EXPECT_NE(computeHash(largeVector1, true), computeHash(largeVector3, true));

    // Performance comparison
    auto start = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] auto seqHash = computeHash(largeVector1, false);
    auto seqEnd = std::chrono::high_resolution_clock::now();

    auto parStart = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] auto parHash = computeHash(largeVector1, true);
    auto parEnd = std::chrono::high_resolution_clock::now();

    auto seqDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(seqEnd - start)
            .count();
    auto parDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(parEnd - parStart)
            .count();

    std::cout << "Sequential hash: " << seqDuration << "ms" << std::endl;
    std::cout << "Parallel hash: " << parDuration << "ms" << std::endl;
}

TEST_F(HashTest, ComputeHashTuple) {
    auto tuple1 = std::make_tuple(1, "hello", 3.14);
    auto tuple2 = std::make_tuple(1, "hello", 3.14);
    auto tuple3 = std::make_tuple(2, "hello", 3.14);

    // Same tuples should hash to same value
    EXPECT_EQ(computeHash(tuple1), computeHash(tuple2));

    // Different tuples should hash differently
    EXPECT_NE(computeHash(tuple1), computeHash(tuple3));

    // Empty tuple
    auto emptyTuple1 = std::tuple<>();
    auto emptyTuple2 = std::tuple<>();
    EXPECT_EQ(computeHash(emptyTuple1), computeHash(emptyTuple2));
}

TEST_F(HashTest, ComputeHashArray) {
    std::array<int, 5> arr1 = {1, 2, 3, 4, 5};
    std::array<int, 5> arr2 = {1, 2, 3, 4, 5};
    std::array<int, 5> arr3 = {5, 4, 3, 2, 1};

    // Same arrays should hash to same value
    EXPECT_EQ(computeHash(arr1), computeHash(arr2));

    // Different arrays should hash differently
    EXPECT_NE(computeHash(arr1), computeHash(arr3));

    // Arrays of different sizes compile and work
    std::array<int, 3> smallArr = {1, 2, 3};
    EXPECT_NE(computeHash(arr1), computeHash(smallArr));
}

TEST_F(HashTest, ComputeHashPair) {
    auto pair1 = std::make_pair(1, "hello");
    auto pair2 = std::make_pair(1, "hello");
    auto pair3 = std::make_pair(2, "hello");
    auto pair4 = std::make_pair(1, "world");

    // Same pairs should hash to same value
    EXPECT_EQ(computeHash(pair1), computeHash(pair2));

    // Different pairs should hash differently
    EXPECT_NE(computeHash(pair1), computeHash(pair3));
    EXPECT_NE(computeHash(pair1), computeHash(pair4));
}

TEST_F(HashTest, ComputeHashOptional) {
    std::optional<int> opt1 = 42;
    std::optional<int> opt2 = 42;
    std::optional<int> opt3 = 43;
    std::optional<int> empty1 = std::nullopt;
    std::optional<int> empty2 = std::nullopt;

    // Same optional values should hash to same value
    EXPECT_EQ(computeHash(opt1), computeHash(opt2));

    // Different optional values should hash differently
    EXPECT_NE(computeHash(opt1), computeHash(opt3));

    // Empty optionals should hash to same value
    EXPECT_EQ(computeHash(empty1), computeHash(empty2));

    // Empty should be different from non-empty
    EXPECT_NE(computeHash(opt1), computeHash(empty1));
}

TEST_F(HashTest, ComputeHashVariant) {
    std::variant<int, std::string, double> var1 = 42;
    std::variant<int, std::string, double> var2 = 42;
    std::variant<int, std::string, double> var3 = 43;
    std::variant<int, std::string, double> var4 = "hello";
    std::variant<int, std::string, double> var5 = 3.14;

    // Same variant values should hash to same value
    EXPECT_EQ(computeHash(var1), computeHash(var2));

    // Different variant values should hash differently
    EXPECT_NE(computeHash(var1), computeHash(var3));
    EXPECT_NE(computeHash(var1), computeHash(var4));
    EXPECT_NE(computeHash(var1), computeHash(var5));
    EXPECT_NE(computeHash(var4), computeHash(var5));
}

TEST_F(HashTest, ComputeHashAny) {
    std::any any1 = 42;
    std::any any2 = 42;
    std::any any3 = std::string("hello");
    std::any empty1;
    std::any empty2;

    // Same any values with same type should hash to same value
    EXPECT_EQ(computeHash(any1), computeHash(any2));

    // Different any values with different types should hash differently
    EXPECT_NE(computeHash(any1), computeHash(any3));

    // Empty any values should hash to same value
    EXPECT_EQ(computeHash(empty1), computeHash(empty2));

    // Empty should be different from non-empty
    EXPECT_NE(computeHash(any1), computeHash(empty1));
}

// Hash combining tests
TEST_F(HashTest, HashCombine) {
    // Test hash combining is consistent
    std::size_t seed1 = 0;
    std::size_t seed2 = 0;

    std::size_t hash1 = hashCombine(seed1, 42);
    std::size_t hash2 = hashCombine(seed2, 42);

    EXPECT_EQ(hash1, hash2);

    // Test hash combining produces different results for different inputs
    std::size_t hash3 = hashCombine(seed1, 43);
    EXPECT_NE(hash1, hash3);
}

TEST_F(HashTest, HashCombineConsecutive) {
    // Test consecutive hash combining
    std::size_t seed = 0;

    seed = hashCombine(seed, 1);
    seed = hashCombine(seed, 2);
    seed = hashCombine(seed, 3);

    std::size_t seed2 = 0;
    seed2 = hashCombine(seed2, 1);
    seed2 = hashCombine(seed2, 2);
    seed2 = hashCombine(seed2, 3);

    // Same sequence should produce same hash
    EXPECT_EQ(seed, seed2);

    std::size_t seed3 = 0;
    seed3 = hashCombine(seed3, 3);
    seed3 = hashCombine(seed3, 2);
    seed3 = hashCombine(seed3, 1);

    // Different sequence should produce different hash
    EXPECT_NE(seed, seed3);
}

// VerifyHash tests
TEST_F(HashTest, VerifyHashExact) {
    std::size_t hash1 = 12345;
    std::size_t hash2 = 12345;
    std::size_t hash3 = 67890;

    // Exact matches
    EXPECT_TRUE(verifyHash(hash1, hash2));
    EXPECT_FALSE(verifyHash(hash1, hash3));
}

TEST_F(HashTest, VerifyHashWithTolerance) {
    std::size_t hash1 = 12345;
    std::size_t hash2 = 12349;  // Difference of 4
    std::size_t hash3 = 12355;  // Difference of 10

    // With tolerance of 5
    EXPECT_TRUE(verifyHash(hash1, hash2, 5));
    EXPECT_FALSE(verifyHash(hash1, hash3, 5));

    // With tolerance of 10
    EXPECT_TRUE(verifyHash(hash1, hash3, 10));

    // Test symmetry
    EXPECT_TRUE(verifyHash(hash2, hash1, 5));
    EXPECT_TRUE(verifyHash(hash3, hash1, 10));
}

// Define Point structure in global scope
struct Point {
    int x, y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// Define hash in global namespace
namespace std {
template <>
struct hash<Point> {
    std::size_t operator()(const Point& p) const {
        return std::hash<int>{}(p.x) ^ (std::hash<int>{}(p.y) << 1);
    }
};
}  // namespace std

// Custom type tests
TEST_F(HashTest, CustomTypeHash) {
    Point p1{1, 2};
    Point p2{1, 2};
    Point p3{2, 1};

    // Same points should hash to same value
    EXPECT_EQ(computeHash(p1), computeHash(p2));

    // Different points should hash differently
    EXPECT_NE(computeHash(p1), computeHash(p3));

    // Should match std::hash result
    auto hashFunc = std::hash<Point>{};
    EXPECT_EQ(computeHash(p1), hashFunc(p1));

    // Test with containers
    std::vector<Point> v1 = {{1, 2}, {3, 4}};
    std::vector<Point> v2 = {{1, 2}, {3, 4}};
    std::vector<Point> v3 = {{3, 4}, {1, 2}};

    EXPECT_EQ(computeHash(v1), computeHash(v2));
    EXPECT_NE(computeHash(v1), computeHash(v3));
}

// Large data tests
TEST_F(HashTest, LargeDataHashing) {
    // Create large strings
    std::string large1 = generateRandomString(1000000);
    std::string large2 = large1;
    std::string large3 = generateRandomString(1000000);

    // Same large strings should hash to same value
    EXPECT_EQ(atom::algorithm::hash(large1.c_str()),
              atom::algorithm::hash(large2.c_str()));

    // Different large strings should hash differently (with extremely high
    // probability)
    EXPECT_NE(atom::algorithm::hash(large1.c_str()),
              atom::algorithm::hash(large3.c_str()));

    // Time the hash function
    auto start = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] auto hashVal = atom::algorithm::hash(large1.c_str());
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::cout << "Hashing 1MB string took: " << duration << "ms" << std::endl;
}

// Quality of hash tests
TEST_F(HashTest, HashDistribution) {
    // Test the distribution quality of the hash function
    const int numStrings = 100000;
    const int numBuckets = 10000;

    std::vector<std::string> testStrings;
    testStrings.reserve(numStrings);

    for (int i = 0; i < numStrings; ++i) {
        testStrings.push_back(generateRandomString(20));
    }

    std::vector<int> bucketCounts(numBuckets, 0);

    for (const auto& str : testStrings) {
        std::size_t h = atom::algorithm::hash(str.c_str()) % numBuckets;
        bucketCounts[h]++;
    }

    // Calculate statistics
    double mean = static_cast<double>(numStrings) / numBuckets;
    double variance = 0.0;

    for (int count : bucketCounts) {
        variance += (count - mean) * (count - mean);
    }

    variance /= numBuckets;
    double stddev = std::sqrt(variance);

    // For a good hash function, the coefficient of variation (stddev/mean)
    // should be low
    double cv = stddev / mean;

    std::cout << "Hash distribution statistics:" << std::endl;
    std::cout << "Mean bucket size: " << mean << std::endl;
    std::cout << "Standard deviation: " << stddev << std::endl;
    std::cout << "Coefficient of variation: " << cv << std::endl;

    // This is not a strict test, but a good hash function should have cv < 1.0
    // A perfect hash would have cv = 0 (all buckets equally filled)
    EXPECT_LT(cv, 1.0);
}

// Collision tests
TEST_F(HashTest, HashCollisions) {
    // Generate many short strings and check for collisions
    const int numStrings = 100000;
    const int stringLength = 8;

    std::vector<std::string> testStrings;
    testStrings.reserve(numStrings);

    for (int i = 0; i < numStrings; ++i) {
        testStrings.push_back(generateRandomString(stringLength));
    }

    std::unordered_map<std::size_t, std::vector<std::string>> hashToStrings;

    for (const auto& str : testStrings) {
        std::size_t h = atom::algorithm::hash(str.c_str());
        hashToStrings[h].push_back(str);
    }

    // Count collisions
    int collisions = 0;
    for (const auto& [h, strings] : hashToStrings) {
        if (strings.size() > 1) {
            collisions += strings.size() - 1;
        }
    }

    double collisionRate = static_cast<double>(collisions) / numStrings;

    std::cout << "Hash collisions: " << collisions << " out of " << numStrings
              << " (" << (collisionRate * 100) << "%)" << std::endl;

    // For a good hash function with 8-char strings and 64-bit output,
    // collision rate should be very low
    EXPECT_LT(collisionRate, 0.01);  // Expecting less than 1% collision rate
}

// Avalanche effect tests
TEST_F(HashTest, AvalancheEffect) {
    // Test that changing a single bit in the input causes significant changes
    // in the output This is a property of good hash functions called the
    // "avalanche effect"

    std::string baseStr = "test string for avalanche";
    std::size_t baseHash = atom::algorithm::hash(baseStr.c_str());

    int totalBitChanges = 0;
    const int numTests = baseStr.length();

    for (size_t i = 0; i < baseStr.length(); ++i) {
        std::string modifiedStr = baseStr;
        // Flip one bit in the character
        modifiedStr[i] ^= 1;

        std::size_t modifiedHash = atom::algorithm::hash(modifiedStr.c_str());

        // Count how many bits differ between hashes
        std::size_t xorResult = baseHash ^ modifiedHash;
        int bitChanges = 0;
        while (xorResult != 0) {
            bitChanges += xorResult & 1;
            xorResult >>= 1;
        }

        totalBitChanges += bitChanges;
    }

    double avgBitChanges = static_cast<double>(totalBitChanges) / numTests;
    double maxPossibleBitChanges =
        sizeof(std::size_t) * 8;  // Number of bits in size_t
    double changeRatio = avgBitChanges / maxPossibleBitChanges;

    std::cout << "Average bit changes: " << avgBitChanges << " out of "
              << maxPossibleBitChanges << " (" << (changeRatio * 100) << "%)"
              << std::endl;

    // A good hash function should change approximately half the bits on average
    // Allow for some flexibility: 30% to 70% change is reasonable
    EXPECT_GT(changeRatio, 0.3);
    EXPECT_LT(changeRatio, 0.7);
}

// Performance benchmark
TEST_F(HashTest, HashPerformanceBenchmark) {
    // Generate strings of various lengths
    std::vector<std::pair<std::string, std::string>> testData = {
        {"Short string (10 chars)", generateRandomString(10)},
        {"Medium string (100 chars)", generateRandomString(100)},
        {"Long string (1000 chars)", generateRandomString(1000)},
        {"Very long string (10000 chars)", generateRandomString(10000)},
        {"Extremely long string (100000 chars)", generateRandomString(100000)}};

    for (const auto& [desc, str] : testData) {
        auto start = std::chrono::high_resolution_clock::now();

        // Hash the string 1000 times (fewer for very long strings)
        int iterations = (str.length() < 10000) ? 1000 : 100;
        std::size_t result = 0;

        for (int i = 0; i < iterations; ++i) {
            result ^= atom::algorithm::hash(str.c_str());
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();

        double avgTimePerHash = static_cast<double>(duration) / iterations;
        double throughput =
            (str.length() * iterations) /
            (static_cast<double>(duration) / 1000000.0);  // Bytes per second

        std::cout << "Hash performance for " << desc << ":" << std::endl;
        std::cout << "  Average time: " << avgTimePerHash << " μs per hash"
                  << std::endl;
        std::cout << "  Throughput: " << (throughput / 1000000.0) << " MB/s"
                  << std::endl;

        // Prevent optimization
        volatile std::size_t unused = result;
        (void)unused;
    }
}

TEST_F(HashTest, HashCombinePerformance) {
    // Compare performance of hashCombine with regular operations

    const int iterations = 1000000;
    std::vector<std::size_t> values;

    // Generate random hash values
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dis;

    for (int i = 0; i < 100; ++i) {
        values.push_back(dis(gen));
    }

    // Measure hashCombine performance
    auto start = std::chrono::high_resolution_clock::now();

    std::size_t seed = 0;
    for (int i = 0; i < iterations; ++i) {
        for (const auto& val : values) {
            seed = hashCombine(seed, val);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto hashCombineDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Measure simple XOR performance as baseline
    start = std::chrono::high_resolution_clock::now();

    std::size_t xorSeed = 0;
    for (int i = 0; i < iterations; ++i) {
        for (const auto& val : values) {
            xorSeed ^= val;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto xorDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "HashCombine vs XOR performance:" << std::endl;
    std::cout << "  HashCombine: " << hashCombineDuration << "ms" << std::endl;
    std::cout << "  Simple XOR: " << xorDuration << "ms" << std::endl;

    // Prevent optimization
    volatile std::size_t unused1 = seed;
    volatile std::size_t unused2 = xorSeed;
    (void)unused1;
    (void)unused2;
}
