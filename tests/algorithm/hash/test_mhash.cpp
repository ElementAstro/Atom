#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <random>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include "atom/algorithm/mhash.hpp"
#include "atom/error/exception.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

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
        return static_cast<size_t>(p.x * 73856093) ^
               static_cast<size_t>(p.y * 19349663);
    }
};
}  // namespace std

class MHashTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    std::string generateRandomString(size_t length) {
        std::string result;
        result.reserve(length);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(32, 126);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dist(gen)));
        }
        return result;
    }

    std::pair<std::set<std::string>, std::set<std::string>>
    createSetPairWithSimilarity(double targetSimilarity, size_t totalElements) {
        std::random_device rd;
        std::mt19937 gen(rd());
        size_t commonElements = static_cast<size_t>(
            targetSimilarity * totalElements / (2 - targetSimilarity));
        size_t uniqueElements = totalElements - commonElements;
        std::set<std::string> set1, set2;
        for (size_t i = 0; i < commonElements; ++i) {
            std::string element = "common_" + std::to_string(i);
            set1.insert(element);
            set2.insert(element);
        }
        for (size_t i = 0; i < uniqueElements; ++i) {
            set1.insert("set1_" + std::to_string(i));
            set2.insert("set2_" + std::to_string(i));
        }
        return {set1, set2};
    }

    double calculateSimilarity(const std::set<std::string>& set1,
                               const std::set<std::string>& set2) {
        size_t intersectionSize = 0;
        for (const auto& element : set1) {
            if (set2.find(element) != set2.end()) {
                intersectionSize++;
            }
        }
        size_t unionSize = set1.size() + set2.size() - intersectionSize;
        return static_cast<double>(intersectionSize) / unionSize;
    }
};

TEST_F(MHashTest, MinHashConstruction) {
    EXPECT_NO_THROW({ MinHash minhash(10); });
    EXPECT_THROW({ MinHash minhash(0); }, atom::error::InvalidArgument);
}

TEST_F(MHashTest, MinHashEmptySets) {
    MinHash minhash(10);
    std::vector<std::string> emptySet;
    auto signature = minhash.computeSignature(emptySet);
    EXPECT_EQ(signature.size(), 10);
    for (const auto& val : signature) {
        EXPECT_EQ(val, std::numeric_limits<size_t>::max());
    }
}

TEST_F(MHashTest, MinHashSignatureSize) {
    for (size_t numHashes : {1, 5, 20, 100}) {
        MinHash minhash(numHashes);
        std::vector<std::string> testSet = {"item1", "item2", "item3"};
        auto signature = minhash.computeSignature(testSet);
        EXPECT_EQ(signature.size(), numHashes);
    }
}

TEST_F(MHashTest, MinHashConsistency) {
    MinHash minhash(20);
    std::vector<std::string> testSet = {"apple", "banana", "cherry", "date"};
    auto sig1 = minhash.computeSignature(testSet);
    auto sig2 = minhash.computeSignature(testSet);
    ASSERT_EQ(sig1.size(), sig2.size());
    for (size_t i = 0; i < sig1.size(); ++i) {
        EXPECT_EQ(sig1[i], sig2[i]);
    }
}

TEST_F(MHashTest, MinHashSimilarityIndexBasic) {
    std::vector<size_t> sig1 = {1, 2, 3, 4, 5};
    double similarity = MinHash::jaccardIndex(sig1, sig1);
    EXPECT_DOUBLE_EQ(similarity, 1.0);
    std::vector<size_t> sig2 = {6, 7, 8, 9, 10};
    similarity = MinHash::jaccardIndex(sig1, sig2);
    EXPECT_DOUBLE_EQ(similarity, 0.0);
    std::vector<size_t> sig3 = {1, 2, 8, 9, 10};
    similarity = MinHash::jaccardIndex(sig1, sig3);
    EXPECT_DOUBLE_EQ(similarity, 0.4);
}

TEST_F(MHashTest, MinHashSimilarityIndexErrorCases) {
    std::vector<size_t> sig1 = {1, 2, 3, 4, 5};
    std::vector<size_t> sig2 = {1, 2, 3};
    ASSERT_THROW(
        {
            double result = MinHash::jaccardIndex(sig1, sig2);
            (void)result;
        },
        atom::error::InvalidArgument);
    std::vector<size_t> empty;
    double result = MinHash::jaccardIndex(empty, empty);
    EXPECT_EQ(result, 0.0);
}

TEST_F(MHashTest, MinHashSimilarityAccuracy) {
    const size_t numTests = 5;
    const size_t numHashes = 200;
    const size_t totalElements = 1000;
    std::vector<double> targetSimilarities = {0.1, 0.3, 0.5, 0.7, 0.9};
    for (double targetSimilarity : targetSimilarities) {
        double totalError = 0.0;
        for (size_t test = 0; test < numTests; ++test) {
            auto [set1, set2] =
                createSetPairWithSimilarity(targetSimilarity, totalElements);
            double actualSimilarity = calculateSimilarity(set1, set2);
            MinHash minhash(numHashes);
            auto sig1 = minhash.computeSignature(set1);
            auto sig2 = minhash.computeSignature(set2);
            double estimatedSimilarity = MinHash::jaccardIndex(sig1, sig2);
            totalError += std::abs(actualSimilarity - estimatedSimilarity);
        }
        double avgError = totalError / numTests;
        spdlog::info("Target similarity: {}, Average error: {}",
                     targetSimilarity, avgError);
        EXPECT_LT(avgError, 0.1);
    }
}

TEST_F(MHashTest, MinHashDifferentTypes) {
    MinHash minhash(10);
    std::vector<std::string> stringSet = {"apple", "banana", "cherry"};
    auto stringSig = minhash.computeSignature(stringSet);
    EXPECT_EQ(stringSig.size(), 10);
    std::vector<int> intSet = {1, 2, 3, 4, 5};
    auto intSig = minhash.computeSignature(intSet);
    EXPECT_EQ(intSig.size(), 10);
    std::vector<Point> pointSet = {{1, 2}, {3, 4}, {5, 6}};
    auto pointSig = minhash.computeSignature(pointSet);
    EXPECT_EQ(pointSig.size(), 10);
}

TEST_F(MHashTest, MinHashSetTypes) {
    MinHash minhash(10);
    std::vector<std::string> vecSet = {"apple", "banana", "cherry"};
    auto vecSig = minhash.computeSignature(vecSet);
    std::set<std::string> stdSet(vecSet.begin(), vecSet.end());
    auto setSig = minhash.computeSignature(stdSet);
    std::unordered_set<std::string> hashSet(vecSet.begin(), vecSet.end());
    auto hashSig = minhash.computeSignature(hashSet);
    ASSERT_EQ(vecSig.size(), setSig.size());
    ASSERT_EQ(vecSig.size(), hashSig.size());
    for (size_t i = 0; i < vecSig.size(); ++i) {
        EXPECT_EQ(vecSig[i], setSig[i]);
        EXPECT_EQ(vecSig[i], hashSig[i]);
    }
}

TEST_F(MHashTest, MinHashPerformance) {
    const size_t setSize = 10000;
    const size_t numHashes = 100;
    std::vector<std::string> largeSet;
    largeSet.reserve(setSize);
    for (size_t i = 0; i < setSize; ++i) {
        largeSet.push_back("item_" + std::to_string(i));
    }
    MinHash minhash(numHashes);
    auto start = std::chrono::high_resolution_clock::now();
    auto signature = minhash.computeSignature(largeSet);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    spdlog::info(
        "MinHash computation for {} elements with {} hash functions took {} ms",
        setSize, numHashes, duration.count());
    EXPECT_EQ(signature.size(), numHashes);
}

TEST_F(MHashTest, HexStringConversion) {
    std::string testData = "ABC";
    std::string hexResult;
    ASSERT_NO_THROW({ hexResult = hexstringFromData(testData); });
    std::string dataResult;
    ASSERT_NO_THROW({
        dataResult = dataFromHexstring(hexResult);
        EXPECT_EQ(dataResult, testData);
    });
    ASSERT_THROW(
        {
            std::string result = dataFromHexstring("123");
            (void)result;
        },
        atom::error::InvalidArgument);
    ASSERT_THROW(
        {
            std::string result = dataFromHexstring("12ZZ");
            (void)result;
        },
        atom::error::InvalidArgument);
}

TEST_F(MHashTest, ThreadSafety) {
    const size_t numThreads = 10;
    std::vector<std::string> testSet = {"item1", "item2", "item3"};
    MinHash minhash(10);
    auto expectedSignature = minhash.computeSignature(testSet);
    std::vector<std::thread> threads;
    std::vector<std::vector<size_t>> results;
    results.resize(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([&testSet, &results, i]() {
            MinHash threadMinhash(10);
            auto sig = threadMinhash.computeSignature(testSet);
            results[i].assign(sig.begin(), sig.end());
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    for (const auto& signature : results) {
        ASSERT_EQ(signature.size(), expectedSignature.size());
        for (size_t i = 0; i < signature.size(); ++i) {
            EXPECT_EQ(signature[i], expectedSignature[i]);
        }
    }
}

// =============================================================================
// Keccak256 Tests
// =============================================================================

TEST_F(MHashTest, Keccak256BasicHash) {
    // Test basic hashing functionality
    std::string input = "hello";
    auto hash = keccak256(input);
    EXPECT_EQ(hash.size(), K_HASH_SIZE);

    // Same input should produce same hash
    auto hash2 = keccak256(input);
    EXPECT_EQ(hash, hash2);
}

TEST_F(MHashTest, Keccak256EmptyInput) {
    // Test empty input
    std::string emptyInput = "";
    auto hash = keccak256(emptyInput);
    EXPECT_EQ(hash.size(), K_HASH_SIZE);

    // Empty span version
    std::vector<uint8_t> emptyVec;
    auto hash2 = keccak256(std::span<const uint8_t>(emptyVec));
    EXPECT_EQ(hash2.size(), K_HASH_SIZE);
}

TEST_F(MHashTest, Keccak256DifferentInputs) {
    // Different inputs should produce different hashes
    auto hash1 = keccak256("input1");
    auto hash2 = keccak256("input2");
    EXPECT_NE(hash1, hash2);
}

TEST_F(MHashTest, Keccak256BinaryData) {
    // Test with binary data
    std::vector<uint8_t> binaryData = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
    auto hash = keccak256(std::span<const uint8_t>(binaryData));
    EXPECT_EQ(hash.size(), K_HASH_SIZE);
}

TEST_F(MHashTest, Keccak256LargeInput) {
    // Test with large input
    std::string largeInput(10000, 'A');
    auto hash = keccak256(largeInput);
    EXPECT_EQ(hash.size(), K_HASH_SIZE);
}

TEST_F(MHashTest, Keccak256Deterministic) {
    // Test determinism across multiple calls
    std::string input = "test determinism";
    std::array<uint8_t, K_HASH_SIZE> firstHash = keccak256(input);

    for (int i = 0; i < 10; ++i) {
        auto hash = keccak256(input);
        EXPECT_EQ(hash, firstHash);
    }
}

TEST_F(MHashTest, Keccak256SpanOverload) {
    // Test span overload
    std::string strInput = "test";
    std::vector<uint8_t> vecInput(strInput.begin(), strInput.end());

    auto hashFromString = keccak256(strInput);
    auto hashFromSpan = keccak256(std::span<const uint8_t>(vecInput));

    EXPECT_EQ(hashFromString, hashFromSpan);
}

// =============================================================================
// HashContext Tests
// =============================================================================

TEST_F(MHashTest, HashContextConstruction) {
    EXPECT_NO_THROW({ HashContext ctx; });
}

TEST_F(MHashTest, HashContextUpdateAndFinalize) {
    HashContext ctx;

    // Update with string_view
    EXPECT_TRUE(ctx.update("hello"));

    // Finalize and get result
    auto result = ctx.finalize();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), K_HASH_SIZE);
}

TEST_F(MHashTest, HashContextIncrementalUpdate) {
    HashContext ctx1;
    ctx1.update("hello");
    ctx1.update("world");
    auto result1 = ctx1.finalize();

    HashContext ctx2;
    ctx2.update("helloworld");
    auto result2 = ctx2.finalize();

    // Incremental updates should produce same result as single update
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(*result1, *result2);
}

TEST_F(MHashTest, HashContextUpdateWithPointer) {
    HashContext ctx;
    const char* data = "test data";
    EXPECT_TRUE(ctx.update(data, strlen(data)));

    auto result = ctx.finalize();
    EXPECT_TRUE(result.has_value());
}

TEST_F(MHashTest, HashContextUpdateWithSpan) {
    HashContext ctx;
    std::vector<std::byte> data = {std::byte{0x01}, std::byte{0x02},
                                   std::byte{0x03}};
    EXPECT_TRUE(ctx.update(std::span<const std::byte>(data)));

    auto result = ctx.finalize();
    EXPECT_TRUE(result.has_value());
}

TEST_F(MHashTest, HashContextMoveSemantics) {
    HashContext ctx1;
    ctx1.update("test");

    // Move constructor
    HashContext ctx2(std::move(ctx1));
    auto result = ctx2.finalize();
    EXPECT_TRUE(result.has_value());
}

TEST_F(MHashTest, HashContextMoveAssignment) {
    HashContext ctx1;
    ctx1.update("test");

    HashContext ctx2;
    ctx2 = std::move(ctx1);
    auto result = ctx2.finalize();
    EXPECT_TRUE(result.has_value());
}

TEST_F(MHashTest, HashContextEmptyInput) {
    HashContext ctx;
    // Don't update, just finalize
    auto result = ctx.finalize();
    EXPECT_TRUE(result.has_value());
}

TEST_F(MHashTest, HashContextLargeData) {
    HashContext ctx;
    std::string largeData(100000, 'X');
    EXPECT_TRUE(ctx.update(largeData));

    auto result = ctx.finalize();
    EXPECT_TRUE(result.has_value());
}

// =============================================================================
// supportsHexStringConversion Tests
// =============================================================================

TEST_F(MHashTest, SupportsHexStringConversion) {
    // Valid hex strings
    EXPECT_TRUE(supportsHexStringConversion("0123456789ABCDEF"));
    EXPECT_TRUE(supportsHexStringConversion("0123456789abcdef"));
    EXPECT_TRUE(supportsHexStringConversion("DeAdBeEf"));

    // Invalid hex strings
    EXPECT_FALSE(supportsHexStringConversion(""));
    EXPECT_FALSE(supportsHexStringConversion("GHIJ"));
    EXPECT_FALSE(supportsHexStringConversion("12 34"));
    EXPECT_FALSE(supportsHexStringConversion("12-34"));
}

// Main function removed - using gtest_main
