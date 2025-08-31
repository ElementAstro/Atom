#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <future>
#include <random>
#include <string>
#include <vector>
#include "atom/algorithm/algorithm.hpp"

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

// Test fixtures
class KMPTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing if needed
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
};

class BloomFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru if needed
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
};

class BoyerMooreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru if needed
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
};

// KMP Tests
TEST_F(KMPTest, BasicPatternMatching) {
    // Basic pattern matching
    KMP kmp("ABABC");
    auto result = kmp.search("ABABCABABABC");

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 7);
}

TEST_F(KMPTest, EmptyPattern) {
    KMP kmp("");
    auto result = kmp.search("Hello world");

    // Empty pattern matches at every position
    EXPECT_TRUE(result.empty());
}

TEST_F(KMPTest, EmptyText) {
    KMP kmp("pattern");
    auto result = kmp.search("");

    EXPECT_TRUE(result.empty());
}

TEST_F(KMPTest, NoMatches) {
    KMP kmp("xyz");
    auto result = kmp.search("abcdefghijklmn");

    EXPECT_TRUE(result.empty());
}

TEST_F(KMPTest, PatternLongerThanText) {
    KMP kmp("abcdefg");
    auto result = kmp.search("abc");

    EXPECT_TRUE(result.empty());
}

TEST_F(KMPTest, SetNewPattern) {
    KMP kmp("original");
    kmp.setPattern("new");

    auto result = kmp.search("This is a new test");

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 10);
}

TEST_F(KMPTest, OverlappingMatches) {
    KMP kmp("aaa");
    auto result = kmp.search("aaaaa");

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
}

TEST_F(KMPTest, SearchParallel) {
    // Create a large enough text to test parallel search
    std::string text = "abc" + std::string(10000, 'x') + "abc" +
                       std::string(10000, 'y') + "abc";

    KMP kmp("abc");
    auto result = kmp.searchParallel(text, 5000);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 10003);
    EXPECT_EQ(result[2], 20006);
}

TEST_F(KMPTest, SearchParallelSmallChunks) {
    std::string text = "abcxxxabcyyyabc";

    KMP kmp("abc");
    // Small chunk size to force multiple chunks
    auto result = kmp.searchParallel(text, 5);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 6);
    EXPECT_EQ(result[2], 12);
}

TEST_F(KMPTest, CornerCases) {
    // Test with single character pattern
    KMP kmp1("a");
    auto result1 = kmp1.search("banana");
    ASSERT_EQ(result1.size(), 3);
    EXPECT_EQ(result1[0], 1);
    EXPECT_EQ(result1[1], 3);
    EXPECT_EQ(result1[2], 5);

    // Test with repeated characters
    KMP kmp2("aaa");
    auto result2 = kmp2.search("aaaaaaa");
    ASSERT_EQ(result2.size(), 5);

    // Test with pattern that is the entire text
    KMP kmp3("fullmatch");
    auto result3 = kmp3.search("fullmatch");
    ASSERT_EQ(result3.size(), 1);
    EXPECT_EQ(result3[0], 0);
}

TEST_F(KMPTest, ThreadSafety) {
    KMP kmp("pattern");

    // Run multiple searches in parallel
    std::vector<std::future<std::vector<int>>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [&kmp]() {
            return kmp.search("This is a pattern test with pattern inside");
        }));
    }

    // All searches should return the same result
    for (auto& future : futures) {
        auto result = future.get();
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], 10);
        EXPECT_EQ(result[1], 29);
    }
}

TEST_F(KMPTest, Performance) {
    // Create a large text and pattern
    std::string largeText = generateRandomString(1000000);
    std::string pattern =
        largeText.substr(500000, 20);  // Pattern exists in the middle

    KMP kmp(pattern);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = kmp.search(largeText);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Verify correctness
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result[0], 500000);

    // This is more of a benchmark than an assertion
    spdlog::info("KMP search on 1MB text took: {}ms", duration);
}

// BloomFilter Tests
TEST_F(BloomFilterTest, BasicOperations) {
    // Create a bloom filter with 1024 bits and 3 hash functions
    BloomFilter<1024> filter(3);

    // Insert some elements
    filter.insert("apple");
    filter.insert("banana");
    filter.insert("orange");

    // Check presence
    EXPECT_TRUE(filter.contains("apple"));
    EXPECT_TRUE(filter.contains("banana"));
    EXPECT_TRUE(filter.contains("orange"));

    // Check absence (these should ideally be false, but false positives are
    // possible)
    EXPECT_FALSE(filter.contains("grape"));
    EXPECT_FALSE(filter.contains("melon"));
}

TEST_F(BloomFilterTest, ClearOperation) {
    BloomFilter<1024> filter(3);

    filter.insert("element1");
    filter.insert("element2");

    EXPECT_TRUE(filter.contains("element1"));

    filter.clear();

    EXPECT_FALSE(filter.contains("element1"));
    EXPECT_FALSE(filter.contains("element2"));
    EXPECT_EQ(filter.elementCount(), 0);
}

TEST_F(BloomFilterTest, ElementCount) {
    BloomFilter<1024> filter(3);

    EXPECT_EQ(filter.elementCount(), 0);

    filter.insert("one");
    EXPECT_EQ(filter.elementCount(), 1);

    filter.insert("two");
    EXPECT_EQ(filter.elementCount(), 2);

    filter.insert("three");
    EXPECT_EQ(filter.elementCount(), 3);

    // Adding duplicate elements still increases the counter
    // since Bloom filters don't detect duplicates
    filter.insert("one");
    EXPECT_EQ(filter.elementCount(), 4);
}

TEST_F(BloomFilterTest, FalsePositiveProbability) {
    // Create a small filter to increase chance of false positives
    BloomFilter<64> filter(2);

    EXPECT_DOUBLE_EQ(filter.falsePositiveProbability(), 0.0);

    // Insert several elements
    for (int i = 0; i < 10; ++i) {
        filter.insert(std::to_string(i));
    }

    // Check false positive rate (should be greater than 0)
    EXPECT_GT(filter.falsePositiveProbability(), 0.0);

    // More elements should increase false positive rate
    double initial_rate = filter.falsePositiveProbability();
    for (int i = 10; i < 20; ++i) {
        filter.insert(std::to_string(i));
    }

    EXPECT_GT(filter.falsePositiveProbability(), initial_rate);
}

TEST_F(BloomFilterTest, DifferentTypes) {
    // Test with integers
    BloomFilter<512, int> intFilter(3);

    intFilter.insert(123);
    intFilter.insert(456);

    EXPECT_TRUE(intFilter.contains(123));
    EXPECT_TRUE(intFilter.contains(456));
    EXPECT_FALSE(intFilter.contains(789));

    // Test with custom type (requires custom hash function)
    struct Point {
        int x, y;

        bool operator==(const Point& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct PointHash {
        std::size_t operator()(const Point& p) const {
            return std::hash<int>{}(p.x) ^ (std::hash<int>{}(p.y) << 1);
        }
    };

    BloomFilter<512, Point, PointHash> pointFilter(3);

    pointFilter.insert({1, 2});
    pointFilter.insert({3, 4});

    EXPECT_TRUE(pointFilter.contains({1, 2}));
    EXPECT_TRUE(pointFilter.contains({3, 4}));
    EXPECT_FALSE(pointFilter.contains({5, 6}));
}

TEST_F(BloomFilterTest, ExceptionHandling) {
    // Test constructor with zero hash functions (should throw)
    EXPECT_THROW({ BloomFilter<1024> filter(0); }, std::invalid_argument);
}

TEST_F(BloomFilterTest, LargeNumberOfElements) {
    // Use a larger bit array for more realistic testing
    BloomFilter<10000> filter(5);

    // Insert many elements
    const int elementCount = 1000;
    for (int i = 0; i < elementCount; ++i) {
        filter.insert(std::to_string(i));
    }

    // Verify all inserted elements are found
    for (int i = 0; i < elementCount; ++i) {
        EXPECT_TRUE(filter.contains(std::to_string(i)));
    }

    // Calculate false positive rate
    int falsePositives = 0;
    const int testCount = 1000;
    for (int i = elementCount; i < elementCount + testCount; ++i) {
        if (filter.contains(std::to_string(i))) {
            falsePositives++;
        }
    }

    double measuredFPR = static_cast<double>(falsePositives) / testCount;
    double theoreticalFPR = filter.falsePositiveProbability();

    // Measured FPR should be reasonably close to theoretical FPR
    // Allow for some statistical variation
    EXPECT_NEAR(measuredFPR, theoreticalFPR, 0.1);

    spdlog::info("Theoretical FPR: {}, Measured FPR: {} ({} / {})",
                 theoreticalFPR, measuredFPR, falsePositives, testCount);
}

// BoyerMoore Tests
TEST_F(BoyerMooreTest, BasicPatternMatching) {
    BoyerMoore bm("ABABC");
    auto result = bm.search("ABABCABABABC");

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 7);
}

TEST_F(BoyerMooreTest, EmptyPattern) {
    BoyerMoore bm("");
    auto result = bm.search("Hello world");

    EXPECT_TRUE(result.empty());
}

TEST_F(BoyerMooreTest, EmptyText) {
    BoyerMoore bm("pattern");
    auto result = bm.search("");

    EXPECT_TRUE(result.empty());
}

TEST_F(BoyerMooreTest, NoMatches) {
    BoyerMoore bm("xyz");
    auto result = bm.search("abcdefghijklmn");

    EXPECT_TRUE(result.empty());
}

TEST_F(BoyerMooreTest, PatternLongerThanText) {
    BoyerMoore bm("abcdefg");
    auto result = bm.search("abc");

    EXPECT_TRUE(result.empty());
}

TEST_F(BoyerMooreTest, SetNewPattern) {
    BoyerMoore bm("original");
    bm.setPattern("new");

    auto result = bm.search("This is a new test");

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 10);
}

TEST_F(BoyerMooreTest, BadCharacterRule) {
    // This test specifically checks the bad character rule in Boyer-Moore
    // Pattern: "EXAMPLE"
    // Text:    "HERE IS AN EXAMPLE"
    // When trying to match at position 0, 'E' from pattern doesn't match 'H' in
    // text Using the bad character rule, we should skip ahead

    BoyerMoore bm("EXAMPLE");
    auto result = bm.search("HERE IS AN EXAMPLE");

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 11);
}

TEST_F(BoyerMooreTest, GoodSuffixRule) {
    // This test checks the good suffix rule in Boyer-Moore
    // Pattern: "ABCABC"
    // Text:    "ABCABCABC"
    // When we find a match at position 0, the good suffix rule helps skip
    // redundant checks

    BoyerMoore bm("ABCABC");
    auto result = bm.search("ABCABCABC");

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 3);
}

TEST_F(BoyerMooreTest, SearchOptimized) {
    // Create a large enough text to test optimized search
    std::string text = "abc" + std::string(10000, 'x') + "abc" +
                       std::string(10000, 'y') + "abc";

    BoyerMoore bm("abc");
    auto result = bm.searchOptimized(text);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 10003);
    EXPECT_EQ(result[2], 20006);
}

TEST_F(BoyerMooreTest, CompareWithRegularSearch) {
    // Compare results from regular search and optimized search
    std::string pattern = "pattern";
    std::string text = "This is a pattern test with pattern inside";

    BoyerMoore bm(pattern);
    auto regular_result = bm.search(text);
    auto optimized_result = bm.searchOptimized(text);

    // Results should be identical
    ASSERT_EQ(regular_result.size(), optimized_result.size());
    for (size_t i = 0; i < regular_result.size(); ++i) {
        EXPECT_EQ(regular_result[i], optimized_result[i]);
    }
}

TEST_F(BoyerMooreTest, ThreadSafety) {
    BoyerMoore bm("pattern");

    // Run multiple searches in parallel
    std::vector<std::future<std::vector<int>>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [&bm]() {
            return bm.search("This is a pattern test with pattern inside");
        }));
    }

    // All searches should return the same result
    for (auto& future : futures) {
        auto result = future.get();
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0], 10);
        EXPECT_EQ(result[1], 29);
    }
}

TEST_F(BoyerMooreTest, Performance) {
    // Create a large text and pattern
    std::string largeText = generateRandomString(1000000);
    std::string pattern =
        largeText.substr(500000, 20);  // Pattern exists in the middle

    BoyerMoore bm(pattern);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = bm.search(largeText);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration1 =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    start = std::chrono::high_resolution_clock::now();
    auto result2 = bm.searchOptimized(largeText);
    end = std::chrono::high_resolution_clock::now();
    auto duration2 =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Verify correctness
    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result[0], 500000);

    ASSERT_FALSE(result2.empty());
    EXPECT_EQ(result2[0], 500000);

    // This is more of a benchmark than an assertion
    spdlog::info("BM normal search on 1MB text took: {}ms", duration1);
    spdlog::info("BM optimized search on 1MB text took: {}ms", duration2);
}

// Compare KMP vs BoyerMoore
TEST(AlgorithmComparison, KMPVsBoyerMoore) {
    // Create a large text and pattern
    std::string largeText = generateRandomString(1000000);
    std::string pattern =
        largeText.substr(500000, 20);  // Pattern exists in the middle

    KMP kmp(pattern);
    BoyerMoore bm(pattern);

    // Measure KMP performance
    auto start = std::chrono::high_resolution_clock::now();
    auto kmpResult = kmp.search(largeText);
    auto end = std::chrono::high_resolution_clock::now();
    auto kmpDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Measure BoyerMoore performance
    start = std::chrono::high_resolution_clock::now();
    auto bmResult = bm.search(largeText);
    end = std::chrono::high_resolution_clock::now();
    auto bmDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Measure BoyerMoore optimized performance
    start = std::chrono::high_resolution_clock::now();
    auto bmOptResult = bm.searchOptimized(largeText);
    end = std::chrono::high_resolution_clock::now();
    auto bmOptDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Measure KMP parallel performance
    start = std::chrono::high_resolution_clock::now();
    auto kmpParResult = kmp.searchParallel(largeText);
    end = std::chrono::high_resolution_clock::now();
    auto kmpParDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Verify all methods found the pattern at the same place
    ASSERT_EQ(kmpResult.size(), bmResult.size());
    ASSERT_EQ(kmpResult.size(), bmOptResult.size());
    ASSERT_EQ(kmpResult.size(), kmpParResult.size());

    EXPECT_EQ(kmpResult[0], 500000);
    EXPECT_EQ(bmResult[0], 500000);
    EXPECT_EQ(bmOptResult[0], 500000);
    EXPECT_EQ(kmpParResult[0], 500000);

    // Output performance comparison
    spdlog::info("Performance comparison on 1MB text with 20-char pattern:");
    spdlog::info("KMP:              {}ms", kmpDuration);
    spdlog::info("KMP Parallel:     {}ms", kmpParDuration);
    spdlog::info("Boyer-Moore:      {}ms", bmDuration);
    spdlog::info("Boyer-Moore Opt:  {}ms", bmOptDuration);
}
