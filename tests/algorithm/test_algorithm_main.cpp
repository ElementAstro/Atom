/*
 * test_algorithm.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Algorithm Library
Tests KMP string matching, Bloom filters, and other core algorithms.

**************************************************/

#include <gtest/gtest.h>
#include "../tests/test_common.hpp"
#include "atom/algorithm/algorithm.hpp"

#include <string>
#include <vector>

namespace atom::algorithm::test {

// ============================================================================
// KMP Algorithm Tests
// ============================================================================

class KMPTest : public atom::test::AtomTestBase {
protected:
    void SetUp() override {
        AtomTestBase::SetUp();
        // Setup KMP test data
        test_pattern_ = "abc";
        test_text_ = "abcabcabc";
        large_text_ = "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc";
    }

    void TearDown() override {
        AtomTestBase::TearDown();
    }

    std::string test_pattern_;
    std::string test_text_;
    std::string large_text_;
};

TEST_F(KMPTest, BasicPatternSearch) {
    // Test basic KMP pattern searching
    atom::algorithm::KMP kmp(test_pattern_);
    std::vector<int> positions = kmp.search(test_text_);

    // Expected positions: 0, 3, 6
    std::vector<int> expected = {0, 3, 6};
    EXPECT_EQ(positions, expected);
}

TEST_F(KMPTest, PatternChange) {
    // Test changing pattern
    atom::algorithm::KMP kmp(test_pattern_);

    // Change pattern and search
    kmp.setPattern("bca");
    std::vector<int> positions = kmp.search(test_text_);

    // Expected positions: 1, 4
    std::vector<int> expected = {1, 4};
    EXPECT_EQ(positions, expected);
}

TEST_F(KMPTest, EmptyPattern) {
    // Test empty pattern handling
    atom::algorithm::KMP kmp("");
    std::vector<int> positions = kmp.search(test_text_);

    // Empty pattern should return empty results or handle gracefully
    EXPECT_TRUE(positions.empty() || positions.size() == test_text_.length() + 1);
}

TEST_F(KMPTest, PatternNotFound) {
    // Test pattern not found
    atom::algorithm::KMP kmp("xyz");
    std::vector<int> positions = kmp.search(test_text_);

    EXPECT_TRUE(positions.empty());
}

TEST_F(KMPTest, ParallelSearch) {
    // Test parallel search functionality
    atom::algorithm::KMP kmp(test_pattern_);
    std::vector<int> positions = kmp.searchParallel(large_text_, 4);

    // Should find multiple occurrences
    EXPECT_FALSE(positions.empty());

    // Verify positions are valid
    for (int pos : positions) {
        EXPECT_GE(pos, 0);
        EXPECT_LE(pos, static_cast<int>(large_text_.length()) - static_cast<int>(test_pattern_.length()));
    }
}

TEST_F(KMPTest, PerformanceTest) {
    // Test KMP performance
    atom::algorithm::KMP kmp(test_pattern_);

    EXPECT_PERFORMANCE_BETTER_THAN({
        for (int i = 0; i < 1000; ++i) {
            kmp.search(large_text_);
        }
    }, 100); // Should complete in less than 100ms
}

// ============================================================================
// Bloom Filter Tests
// ============================================================================

class BloomFilterTest : public atom::test::AtomTestBase {
protected:
    void SetUp() override {
        AtomTestBase::SetUp();
        // Setup Bloom filter test data
        test_elements_ = {"hello", "world", "example", "bloom", "filter"};
        non_existent_element_ = "test";
    }

    void TearDown() override {
        AtomTestBase::TearDown();
    }

    std::vector<std::string> test_elements_;
    std::string non_existent_element_;
};

TEST_F(BloomFilterTest, BasicInsertion) {
    // Test basic insertion functionality
    atom::algorithm::BloomFilter<1000> bloomFilter(3);

    for (const auto& element : test_elements_) {
        bloomFilter.insert(element);
    }

    EXPECT_EQ(bloomFilter.elementCount(), test_elements_.size());
}

TEST_F(BloomFilterTest, ContainsInsertedElements) {
    // Test that inserted elements are found
    atom::algorithm::BloomFilter<1000> bloomFilter(3);

    for (const auto& element : test_elements_) {
        bloomFilter.insert(element);
    }

    for (const auto& element : test_elements_) {
        EXPECT_TRUE(bloomFilter.contains(element));
    }
}

TEST_F(BloomFilterTest, FalsePositiveRate) {
    // Test false positive rate is reasonable
    atom::algorithm::BloomFilter<1000> bloomFilter(3);

    for (const auto& element : test_elements_) {
        bloomFilter.insert(element);
    }

    double falsePositiveProb = bloomFilter.falsePositiveProbability();
    EXPECT_GE(falsePositiveProb, 0.0);
    EXPECT_LE(falsePositiveProb, 1.0);
}

TEST_F(BloomFilterTest, ClearFunctionality) {
    // Test clear functionality
    atom::algorithm::BloomFilter<1000> bloomFilter(3);

    for (const auto& element : test_elements_) {
        bloomFilter.insert(element);
    }

    bloomFilter.clear();
    EXPECT_EQ(bloomFilter.elementCount(), 0);

    // After clear, elements should not be found
    for (const auto& element : test_elements_) {
        EXPECT_FALSE(bloomFilter.contains(element));
    }
}

TEST_F(BloomFilterTest, PerformanceTest) {
    // Test Bloom filter performance
    atom::algorithm::BloomFilter<10000> bloomFilter(5);

    EXPECT_PERFORMANCE_BETTER_THAN({
        for (int i = 0; i < 1000; ++i) {
            bloomFilter.insert("element" + std::to_string(i));
        }
    }, 50); // Should complete in less than 50ms
}

// ============================================================================
// Boyer-Moore Algorithm Tests
// ============================================================================

class BoyerMooreTest : public atom::test::AtomTestBase {
protected:
    void SetUp() override {
        AtomTestBase::SetUp();
        // Setup Boyer-Moore test data
        test_pattern_ = "abc";
        test_text_ = "abcabcabc";
        large_text_ = "abcabcabcabcabcabcbcabcabcabcbcabcabc";
    }

    void TearDown() override {
        AtomTestBase::TearDown();
    }

    std::string test_pattern_;
    std::string test_text_;
    std::string large_text_;
};

TEST_F(BoyerMooreTest, BasicPatternSearch) {
    // Test basic Boyer-Moore pattern searching
    atom::algorithm::BoyerMoore boyerMoore(test_pattern_);
    std::vector<int> positions = boyerMoore.search(test_text_);

    // Expected positions: 0, 3, 6
    std::vector<int> expected = {0, 3, 6};
    EXPECT_EQ(positions, expected);
}

TEST_F(BoyerMooreTest, PatternChange) {
    // Test changing pattern
    atom::algorithm::BoyerMoore boyerMoore(test_pattern_);

    // Change pattern and search
    boyerMoore.setPattern("bca");
    std::vector<int> positions = boyerMoore.search(test_text_);

    // Expected positions: 1, 4
    std::vector<int> expected = {1, 4};
    EXPECT_EQ(positions, expected);
}

TEST_F(BoyerMooreTest, OptimizedSearch) {
    // Test optimized search functionality
    atom::algorithm::BoyerMoore boyerMoore(test_pattern_);
    std::vector<int> positions = boyerMoore.searchOptimized(large_text_);

    // Should find multiple occurrences
    EXPECT_FALSE(positions.empty());

    // Verify positions are valid
    for (int pos : positions) {
        EXPECT_GE(pos, 0);
        EXPECT_LE(pos, static_cast<int>(large_text_.length()) - static_cast<int>(test_pattern_.length()));
    }
}

// ============================================================================
// Custom Type Bloom Filter Tests
// ============================================================================

class CustomBloomFilterTest : public atom::test::AtomTestBase {
protected:
    void SetUp() override {
        AtomTestBase::SetUp();
    }

    void TearDown() override {
        AtomTestBase::TearDown();
    }

    struct CustomHasher {
        std::size_t operator()(int value) const {
            return std::hash<int>{}(value);
        }
    };
};

TEST_F(CustomBloomFilterTest, IntegerBloomFilter) {
    // Test Bloom filter with custom integer type
    atom::algorithm::BloomFilter<500, int, CustomHasher> intFilter(2);

    // Insert integers
    for (int i = 0; i < 10; ++i) {
        intFilter.insert(i * 10);
    }

    // Test contains
    EXPECT_TRUE(intFilter.contains(30));
    EXPECT_FALSE(intFilter.contains(31)); // Might be false positive, but unlikely
    EXPECT_EQ(intFilter.elementCount(), 10);
}

// ============================================================================
// Integration Tests
// ============================================================================

class AlgorithmIntegrationTest : public atom::test::AtomTestBase {
protected:
    void SetUp() override {
        AtomTestBase::SetUp();
    }

    void TearDown() override {
        AtomTestBase::TearDown();
    }
};

TEST_F(AlgorithmIntegrationTest, AlgorithmComparison) {
    // Compare KMP and Boyer-Moore performance
    std::string pattern = "pattern";
    std::string text = atom::test::TestDataGenerator::generateRandomString(10000) + pattern + "more_text";

    atom::algorithm::KMP kmp(pattern);
    atom::algorithm::BoyerMoore bm(pattern);

    timer_->start();
    auto kmp_results = kmp.search(text);
    timer_->stop();
    double kmp_time = timer_->getElapsedMilliseconds();

    timer_->start();
    auto bm_results = bm.search(text);
    timer_->stop();
    double bm_time = timer_->getElapsedMilliseconds();

    // Both should find the pattern
    EXPECT_FALSE(kmp_results.empty());
    EXPECT_FALSE(bm_results.empty());

    // Results should be the same
    EXPECT_EQ(kmp_results, bm_results);
}

} // namespace atom::algorithm::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
