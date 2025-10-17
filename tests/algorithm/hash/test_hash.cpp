#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "atom/algorithm/hash.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

inline std::string generateRandomString(size_t length, bool onlyAscii = true) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::string result;
    result.reserve(length);
    if (onlyAscii) {
        std::uniform_int_distribution<> dis(32, 126);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dis(gen)));
        }
    } else {
        std::uniform_int_distribution<> dis(0, 255);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dis(gen)));
        }
    }
    return result;
}

class HashTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }
};

TEST_F(HashTest, BasicStringHash) {
    const char* str1 = "Hello, World!";
    const char* str2 = "Hello, World!";
    const char* str3 = "Different string";
    EXPECT_EQ(atom::algorithm::hash(str1), atom::algorithm::hash(str2));
    EXPECT_NE(atom::algorithm::hash(str1), atom::algorithm::hash(str3));
}

TEST_F(HashTest, HashLiteralOperator) {
    auto hash1 = "test string"_hash;
    auto hash2 = "test string"_hash;
    auto hash3 = "different"_hash;
    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_EQ("hello"_hash, atom::algorithm::hash("hello"));
}

TEST_F(HashTest, EmptyStringHash) {
    EXPECT_EQ(atom::algorithm::hash(""), atom::algorithm::hash(""));
    EXPECT_NE(atom::algorithm::hash(""), atom::algorithm::hash("a"));
}

TEST_F(HashTest, HashCacheBasic) {
    HashCache<std::string> cache;
    EXPECT_EQ(cache.get("test"), std::nullopt);
    cache.set("test", 12345);
    auto value = cache.get("test");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 12345);
    cache.set("test", 67890);
    value = cache.get("test");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 67890);
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

TEST_F(HashTest, HashCacheThreadSafety) {
    HashCache<std::string> cache;
    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; ++i) {
        readers.emplace_back([&]() {
            while (!stop) {
                cache.get("test");
                std::this_thread::yield();
            }
        });
    }
    std::vector<std::thread> writers;
    for (int i = 0; i < 5; ++i) {
        writers.emplace_back([&, i]() {
            for (int j = 0; j < 100; ++j) {
                cache.set("test", i * 1000 + j);
                std::this_thread::yield();
            }
        });
    }
    std::this_thread::sleep_for(100ms);
    stop = true;
    for (auto& t : readers)
        t.join();
    for (auto& t : writers)
        t.join();
}

TEST_F(HashTest, ComputeHashBasicTypes) {
    EXPECT_EQ(computeHash(42), std::hash<int>{}(42));
    EXPECT_EQ(computeHash(3.14), std::hash<double>{}(3.14));
    EXPECT_EQ(computeHash(std::string("hello")),
              std::hash<std::string>{}("hello"));
    EXPECT_EQ(computeHash(true), std::hash<bool>{}(true));
}

TEST_F(HashTest, ComputeHashWithDifferentAlgorithms) {
    std::string testStr = "test string";
    auto hashStd = computeHash(testStr, HashAlgorithm::STD);
    auto hashFnv1a = computeHash(testStr, HashAlgorithm::FNV1A);
    EXPECT_NE(hashStd, hashFnv1a);
    EXPECT_EQ(computeHash(testStr, HashAlgorithm::STD),
              computeHash(testStr, HashAlgorithm::STD));
}

TEST_F(HashTest, ComputeHashCaching) {
    std::string testStr = "test string for caching";
    auto hash1 = computeHash(testStr);
    auto start = std::chrono::high_resolution_clock::now();
    auto hash2 = computeHash(testStr);
    auto end = std::chrono::high_resolution_clock::now();
    EXPECT_EQ(hash1, hash2);
    auto duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();
    spdlog::info("Cache lookup duration: {} ns", duration);
}

TEST_F(HashTest, ComputeHashVector) {
    std::vector<int> v1 = {1, 2, 3, 4, 5};
    std::vector<int> v2 = {1, 2, 3, 4, 5};
    std::vector<int> v3 = {5, 4, 3, 2, 1};
    EXPECT_EQ(computeHash(v1), computeHash(v2));
    EXPECT_NE(computeHash(v1), computeHash(v3));
    std::vector<int> empty1, empty2;
    EXPECT_EQ(computeHash(empty1), computeHash(empty2));
}

TEST_F(HashTest, ComputeHashVectorParallel) {
    std::vector<int> largeVector1(10000, 42);
    std::vector<int> largeVector2(10000, 42);
    std::vector<int> largeVector3(10000, 43);
    EXPECT_EQ(computeHash(largeVector1, false),
              computeHash(largeVector1, true));
    EXPECT_EQ(computeHash(largeVector1, true), computeHash(largeVector2, true));
    EXPECT_NE(computeHash(largeVector1, true), computeHash(largeVector3, true));
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
    spdlog::info("Sequential hash: {}ms", seqDuration);
    spdlog::info("Parallel hash: {}ms", parDuration);
}

TEST_F(HashTest, ComputeHashTuple) {
    auto tuple1 = std::make_tuple(1, "hello", 3.14);
    auto tuple2 = std::make_tuple(1, "hello", 3.14);
    auto tuple3 = std::make_tuple(2, "hello", 3.14);
    EXPECT_EQ(computeHash(tuple1), computeHash(tuple2));
    EXPECT_NE(computeHash(tuple1), computeHash(tuple3));
    auto emptyTuple1 = std::tuple<>();
    auto emptyTuple2 = std::tuple<>();
    EXPECT_EQ(computeHash(emptyTuple1), computeHash(emptyTuple2));
}

TEST_F(HashTest, ComputeHashArray) {
    std::array<int, 5> arr1 = {1, 2, 3, 4, 5};
    std::array<int, 5> arr2 = {1, 2, 3, 4, 5};
    std::array<int, 5> arr3 = {5, 4, 3, 2, 1};
    EXPECT_EQ(computeHash(arr1), computeHash(arr2));
    EXPECT_NE(computeHash(arr1), computeHash(arr3));
    std::array<int, 3> smallArr = {1, 2, 3};
    EXPECT_NE(computeHash(arr1), computeHash(smallArr));
}

TEST_F(HashTest, ComputeHashPair) {
    auto pair1 = std::make_pair(1, "hello");
    auto pair2 = std::make_pair(1, "hello");
    auto pair3 = std::make_pair(2, "hello");
    auto pair4 = std::make_pair(1, "world");
    EXPECT_EQ(computeHash(pair1), computeHash(pair2));
    EXPECT_NE(computeHash(pair1), computeHash(pair3));
    EXPECT_NE(computeHash(pair1), computeHash(pair4));
}

TEST_F(HashTest, ComputeHashOptional) {
    std::optional<int> opt1 = 42;
    std::optional<int> opt2 = 42;
    std::optional<int> opt3 = 43;
    std::optional<int> empty1 = std::nullopt;
    std::optional<int> empty2 = std::nullopt;
    EXPECT_EQ(computeHash(opt1), computeHash(opt2));
    EXPECT_NE(computeHash(opt1), computeHash(opt3));
    EXPECT_EQ(computeHash(empty1), computeHash(empty2));
    EXPECT_NE(computeHash(opt1), computeHash(empty1));
}

TEST_F(HashTest, ComputeHashVariant) {
    std::variant<int, std::string, double> var1 = 42;
    std::variant<int, std::string, double> var2 = 42;
    std::variant<int, std::string, double> var3 = 43;
    std::variant<int, std::string, double> var4 = "hello";
    std::variant<int, std::string, double> var5 = 3.14;
    EXPECT_EQ(computeHash(var1), computeHash(var2));
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
    EXPECT_EQ(computeHash(any1), computeHash(any2));
    EXPECT_NE(computeHash(any1), computeHash(any3));
    EXPECT_EQ(computeHash(empty1), computeHash(empty2));
    EXPECT_NE(computeHash(any1), computeHash(empty1));
}

TEST_F(HashTest, HashCombine) {
    std::size_t seed1 = 0;
    std::size_t seed2 = 0;
    std::size_t hash1 = hashCombine(seed1, 42);
    std::size_t hash2 = hashCombine(seed2, 42);
    EXPECT_EQ(hash1, hash2);
    std::size_t hash3 = hashCombine(seed1, 43);
    EXPECT_NE(hash1, hash3);
}

TEST_F(HashTest, HashCombineConsecutive) {
    std::size_t seed = 0;
    seed = hashCombine(seed, 1);
    seed = hashCombine(seed, 2);
    seed = hashCombine(seed, 3);
    std::size_t seed2 = 0;
    seed2 = hashCombine(seed2, 1);
    seed2 = hashCombine(seed2, 2);
    seed2 = hashCombine(seed2, 3);
    EXPECT_EQ(seed, seed2);
    std::size_t seed3 = 0;
    seed3 = hashCombine(seed3, 3);
    seed3 = hashCombine(seed3, 2);
    seed3 = hashCombine(seed3, 1);
    EXPECT_NE(seed, seed3);
}

TEST_F(HashTest, VerifyHashExact) {
    std::size_t hash1 = 12345;
    std::size_t hash2 = 12345;
    std::size_t hash3 = 67890;
    EXPECT_TRUE(verifyHash(hash1, hash2));
    EXPECT_FALSE(verifyHash(hash1, hash3));
}

TEST_F(HashTest, VerifyHashWithTolerance) {
    std::size_t hash1 = 12345;
    std::size_t hash2 = 12349;
    std::size_t hash3 = 12355;
    EXPECT_TRUE(verifyHash(hash1, hash2, 5));
    EXPECT_FALSE(verifyHash(hash1, hash3, 5));
    EXPECT_TRUE(verifyHash(hash1, hash3, 10));
    EXPECT_TRUE(verifyHash(hash2, hash1, 5));
    EXPECT_TRUE(verifyHash(hash3, hash1, 10));
}

struct Point {
    int x, y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

namespace std {
template <>
struct hash<Point> {
    std::size_t operator()(const Point& p) const {
        return std::hash<int>{}(p.x) ^ (std::hash<int>{}(p.y) << 1);
    }
};
}  // namespace std

TEST_F(HashTest, CustomTypeHash) {
    Point p1{1, 2};
    Point p2{1, 2};
    Point p3{2, 1};
    EXPECT_EQ(computeHash(p1), computeHash(p2));
    EXPECT_NE(computeHash(p1), computeHash(p3));
    auto hashFunc = std::hash<Point>{};
    EXPECT_EQ(computeHash(p1), hashFunc(p1));
    std::vector<Point> v1 = {{1, 2}, {3, 4}};
    std::vector<Point> v2 = {{1, 2}, {3, 4}};
    std::vector<Point> v3 = {{3, 4}, {1, 2}};
    EXPECT_EQ(computeHash(v1), computeHash(v2));
    EXPECT_NE(computeHash(v1), computeHash(v3));
}

TEST_F(HashTest, LargeDataHashing) {
    std::string large1 = generateRandomString(1000000);
    std::string large2 = large1;
    std::string large3 = generateRandomString(1000000);
    EXPECT_EQ(atom::algorithm::hash(large1.c_str()),
              atom::algorithm::hash(large2.c_str()));
    EXPECT_NE(atom::algorithm::hash(large1.c_str()),
              atom::algorithm::hash(large3.c_str()));
    auto start = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] auto hashVal = atom::algorithm::hash(large1.c_str());
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Hashing 1MB string took: {}ms", duration);
}

TEST_F(HashTest, HashDistribution) {
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
    double mean = static_cast<double>(numStrings) / numBuckets;
    double variance = 0.0;
    for (int count : bucketCounts) {
        variance += (count - mean) * (count - mean);
    }
    variance /= numBuckets;
    double stddev = std::sqrt(variance);
    double cv = stddev / mean;
    spdlog::info("Hash distribution statistics: mean={}, stddev={}, cv={}",
                 mean, stddev, cv);
    EXPECT_LT(cv, 1.0);
}

TEST_F(HashTest, HashCollisions) {
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
    int collisions = 0;
    for (const auto& [h, strings] : hashToStrings) {
        if (strings.size() > 1) {
            collisions += strings.size() - 1;
        }
    }
    double collisionRate = static_cast<double>(collisions) / numStrings;
    spdlog::info("Hash collisions: {} out of {} ({}%)", collisions, numStrings,
                 (collisionRate * 100));
    EXPECT_LT(collisionRate, 0.01);
}

TEST_F(HashTest, AvalancheEffect) {
    std::string baseStr = "test string for avalanche";
    std::size_t baseHash = atom::algorithm::hash(baseStr.c_str());
    int totalBitChanges = 0;
    const int numTests = baseStr.length();
    for (size_t i = 0; i < baseStr.length(); ++i) {
        std::string modifiedStr = baseStr;
        modifiedStr[i] ^= 1;
        std::size_t modifiedHash = atom::algorithm::hash(modifiedStr.c_str());
        std::size_t xorResult = baseHash ^ modifiedHash;
        int bitChanges = 0;
        while (xorResult != 0) {
            bitChanges += xorResult & 1;
            xorResult >>= 1;
        }
        totalBitChanges += bitChanges;
    }
    double avgBitChanges = static_cast<double>(totalBitChanges) / numTests;
    double maxPossibleBitChanges = sizeof(std::size_t) * 8;
    double changeRatio = avgBitChanges / maxPossibleBitChanges;
    spdlog::info("Average bit changes: {} out of {} ({}%)", avgBitChanges,
                 maxPossibleBitChanges, (changeRatio * 100));
    EXPECT_GT(changeRatio, 0.3);
    EXPECT_LT(changeRatio, 0.7);
}

TEST_F(HashTest, HashPerformanceBenchmark) {
    std::vector<std::pair<std::string, std::string>> testData = {
        {"Short string (10 chars)", generateRandomString(10)},
        {"Medium string (100 chars)", generateRandomString(100)},
        {"Long string (1000 chars)", generateRandomString(1000)},
        {"Very long string (10000 chars)", generateRandomString(10000)},
        {"Extremely long string (100000 chars)", generateRandomString(100000)}};
    for (const auto& [desc, str] : testData) {
        auto start = std::chrono::high_resolution_clock::now();
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
        double throughput = (str.length() * iterations) /
                            (static_cast<double>(duration) / 1000000.0);
        spdlog::info(
            "Hash performance for {}: avg time {} μs per hash, throughput {} "
            "MB/s",
            desc, avgTimePerHash, (throughput / 1000000.0));
        volatile std::size_t unused = result;
        (void)unused;
    }
}

TEST_F(HashTest, HashCombinePerformance) {
    const int iterations = 1000000;
    std::vector<std::size_t> values;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dis;
    for (int i = 0; i < 100; ++i) {
        values.push_back(dis(gen));
    }
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
    spdlog::info("HashCombine vs XOR performance: HashCombine={}ms, XOR={}ms",
                 hashCombineDuration, xorDuration);
    volatile std::size_t unused1 = seed;
    volatile std::size_t unused2 = xorSeed;
    (void)unused1;
    (void)unused2;
}
