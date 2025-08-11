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
    EXPECT_THROW({ MinHash minhash(0); }, std::invalid_argument);
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
        std::invalid_argument);
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
        std::invalid_argument);
    ASSERT_THROW(
        {
            std::string result = dataFromHexstring("12ZZ");
            (void)result;
        },
        std::invalid_argument);
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
