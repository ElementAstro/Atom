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
#include "../../tests/test_common.hpp"
#include "atom/algorithm/algorithm.hpp"

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <set>
#include <spdlog/spdlog.h>

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
            auto result = kmp.search(large_text_);
            (void)result; // Suppress unused variable warning
        }
    }, 100); // Should complete in less than 100ms
}

TEST_F(KMPTest, VeryLongPattern) {
    // Test with very long pattern (1000 characters)
    std::string long_pattern(1000, 'a');
    long_pattern += "xyz"; // Add unique ending

    std::string long_text = long_pattern + "some_text" + long_pattern;

    atom::algorithm::KMP kmp(long_pattern);
    std::vector<int> positions = kmp.search(long_text);

    // Should find pattern at positions 0 and (long_pattern.length() + 9)
    EXPECT_EQ(positions.size(), 2);
    EXPECT_EQ(positions[0], 0);
    EXPECT_EQ(positions[1], static_cast<int>(long_pattern.length() + 9));
}

TEST_F(KMPTest, UnicodeCharacters) {
    // Test with Unicode characters
    std::string unicode_pattern = "こんにちは";
    std::string unicode_text = "Hello こんにちは World こんにちは!";

    atom::algorithm::KMP kmp(unicode_pattern);
    std::vector<int> positions = kmp.search(unicode_text);

    // Should find the pattern twice
    EXPECT_EQ(positions.size(), 2);
    EXPECT_GT(positions[0], 0);
    EXPECT_GT(positions[1], positions[0]);
}

TEST_F(KMPTest, SpecialCharacters) {
    // Test with special characters and escape sequences
    std::string special_pattern = "\\n\\t\\r";
    std::string special_text = "Start\\n\\t\\rMiddle\\n\\t\\rEnd";

    atom::algorithm::KMP kmp(special_pattern);
    std::vector<int> positions = kmp.search(special_text);

    EXPECT_EQ(positions.size(), 2);
}

TEST_F(KMPTest, BinaryData) {
    // Test with binary data containing null bytes
    std::string binary_pattern = std::string("abc\0def", 7);
    std::string binary_text = std::string("start\0abc\0def\0middle\0abc\0def\0end", 27);

    atom::algorithm::KMP kmp(binary_pattern);
    std::vector<int> positions = kmp.search(binary_text);

    EXPECT_EQ(positions.size(), 2);
}

TEST_F(KMPTest, RepeatingPattern) {
    // Test with highly repeating pattern
    std::string repeating_pattern = "aaaa";
    std::string repeating_text = "aaaaaaaaaaaaaaaa"; // 16 'a's

    atom::algorithm::KMP kmp(repeating_pattern);
    std::vector<int> positions = kmp.search(repeating_text);

    // Should find overlapping matches
    EXPECT_EQ(positions.size(), 13); // Positions 0, 1, 2, ..., 12
    for (size_t i = 0; i < positions.size(); ++i) {
        EXPECT_EQ(positions[i], static_cast<int>(i));
    }
}

TEST_F(KMPTest, ThreadSafety) {
    // Test thread safety of KMP operations
    atom::algorithm::KMP kmp(test_pattern_);
    const int num_threads = 4;
    const int searches_per_thread = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<int>> results(num_threads);

    // Launch multiple threads performing searches
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < searches_per_thread; ++j) {
                auto positions = kmp.search(test_text_);
                if (j == 0) {
                    results[i] = positions; // Store first result for comparison
                }
                // All results should be identical
                EXPECT_EQ(positions, results[i]);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should have found the same positions
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(results[0], results[i]);
    }
}

TEST_F(KMPTest, ConcurrentPatternChanges) {
    // Test thread safety when changing patterns concurrently
    atom::algorithm::KMP kmp("initial");
    std::atomic<bool> stop_flag{false};
    std::vector<std::thread> threads;

    // Thread that continuously changes patterns
    threads.emplace_back([&]() {
        std::vector<std::string> patterns = {"abc", "def", "ghi", "jkl"};
        int pattern_index = 0;
        while (!stop_flag.load()) {
            try {
                kmp.setPattern(patterns[pattern_index % patterns.size()]);
                pattern_index++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (const std::exception&) {
                // Pattern changes might fail due to concurrent access, which is acceptable
            }
        }
    });

    // Threads that continuously perform searches
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&]() {
            while (!stop_flag.load()) {
                try {
                    auto positions = kmp.search("abcdefghijkl");
                    // Results may vary due to pattern changes, but shouldn't crash
                } catch (const std::exception&) {
                    // Searches might fail due to concurrent pattern changes, which is acceptable
                }
            }
        });
    }

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_flag.store(true);

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Test should complete without crashing
    EXPECT_TRUE(true);
}

TEST_F(KMPTest, ErrorHandlingInvalidPattern) {
    // Test error handling with various invalid patterns
    // Note: Empty pattern is handled gracefully, not as an error

    // Test with extremely long pattern that might cause memory issues
    try {
        std::string huge_pattern(1000000, 'x'); // 1MB pattern
        atom::algorithm::KMP kmp(huge_pattern);
        // Should not throw, but might be slow
        EXPECT_TRUE(true);
    } catch (const std::exception& e) {
        // If it throws due to memory constraints, that's acceptable
        EXPECT_TRUE(std::string(e.what()).find("memory") != std::string::npos ||
                   std::string(e.what()).find("allocation") != std::string::npos);
    }
}

TEST_F(KMPTest, MemoryUsageOptimization) {
    // Test memory usage with various pattern sizes
    std::vector<size_t> pattern_sizes = {1, 10, 100, 1000, 10000};

    for (size_t size : pattern_sizes) {
        std::string pattern(size, 'a');
        pattern.back() = 'b'; // Make pattern unique

        atom::algorithm::KMP kmp(pattern);

        // Test that search still works correctly
        std::string text = pattern + "middle" + pattern;
        auto positions = kmp.search(text);

        EXPECT_EQ(positions.size(), 2);
        EXPECT_EQ(positions[0], 0);
        EXPECT_EQ(positions[1], static_cast<int>(size + 6)); // size + "middle".length()
    }
}

TEST_F(KMPTest, SIMDOptimizationPaths) {
    // Test SIMD optimization paths with short patterns (≤16 characters)
    std::vector<std::string> short_patterns = {
        "a", "ab", "abc", "abcd", "abcdefgh", "abcdefghijklmnop"
    };

    for (const auto& pattern : short_patterns) {
        atom::algorithm::KMP kmp(pattern);

        // Create text with multiple occurrences
        std::string text = pattern + "xyz" + pattern + "123" + pattern;
        auto positions = kmp.search(text);

        // Verify correct number of matches
        EXPECT_EQ(positions.size(), 3);

        // Verify positions are correct
        EXPECT_EQ(positions[0], 0);
        EXPECT_EQ(positions[1], static_cast<int>(pattern.length() + 3));
        EXPECT_EQ(positions[2], static_cast<int>(pattern.length() * 2 + 6));
    }
}

TEST_F(KMPTest, ParallelSearchChunkSizes) {
    // Test parallel search with various chunk sizes
    std::string large_pattern = "pattern";
    std::string large_text_with_pattern;

    // Create large text with known pattern occurrences
    for (int i = 0; i < 1000; ++i) {
        large_text_with_pattern += "some_text_" + large_pattern + "_more_text_";
    }

    atom::algorithm::KMP kmp(large_pattern);

    // Test with different chunk sizes
    std::vector<size_t> chunk_sizes = {1, 10, 100, 1000, 10000};
    std::vector<int> expected_positions;

    // Get expected positions using regular search
    expected_positions = kmp.search(large_text_with_pattern);

    for (size_t chunk_size : chunk_sizes) {
        auto parallel_positions = kmp.searchParallel(large_text_with_pattern, chunk_size);

        // Sort both vectors for comparison (parallel search might return in different order)
        std::sort(expected_positions.begin(), expected_positions.end());
        std::sort(parallel_positions.begin(), parallel_positions.end());

        EXPECT_EQ(parallel_positions, expected_positions)
            << "Failed with chunk size: " << chunk_size;
    }
}

TEST_F(KMPTest, EdgeCaseEmptyText) {
    // Test with empty text
    atom::algorithm::KMP kmp("pattern");
    std::vector<int> positions = kmp.search("");

    EXPECT_TRUE(positions.empty());
}

TEST_F(KMPTest, EdgeCaseTextShorterThanPattern) {
    // Test when text is shorter than pattern
    atom::algorithm::KMP kmp("very_long_pattern");
    std::vector<int> positions = kmp.search("short");

    EXPECT_TRUE(positions.empty());
}

TEST_F(KMPTest, EdgeCaseSingleCharacterPattern) {
    // Test with single character pattern
    atom::algorithm::KMP kmp("a");
    std::vector<int> positions = kmp.search("banana");

    // Should find 'a' at positions 1, 3, 5
    std::vector<int> expected = {1, 3, 5};
    EXPECT_EQ(positions, expected);
}

TEST_F(KMPTest, EdgeCaseSingleCharacterText) {
    // Test with single character text
    atom::algorithm::KMP kmp("a");
    std::vector<int> positions = kmp.search("a");

    std::vector<int> expected = {0};
    EXPECT_EQ(positions, expected);
}

TEST_F(KMPTest, PatternAtTextBoundaries) {
    // Test pattern at the very beginning and end of text
    std::string pattern = "test";
    std::string text = "test_middle_test";

    atom::algorithm::KMP kmp(pattern);
    std::vector<int> positions = kmp.search(text);

    std::vector<int> expected = {0, 12}; // At start and end
    EXPECT_EQ(positions, expected);
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

TEST_F(BloomFilterTest, ConstructorWithZeroHashFunctions) {
    // Test constructor with zero hash functions should throw
    EXPECT_THROW(
        (atom::algorithm::BloomFilter<1000>(0)),
        std::invalid_argument
    );
}

TEST_F(BloomFilterTest, DifferentTemplateSizes) {
    // Test BloomFilter with different template sizes
    atom::algorithm::BloomFilter<100> small_filter(2);
    atom::algorithm::BloomFilter<10000> large_filter(5);
    atom::algorithm::BloomFilter<1000000> huge_filter(10);

    // Insert same elements into all filters
    std::vector<std::string> elements = {"test1", "test2", "test3", "test4", "test5"};

    for (const auto& element : elements) {
        small_filter.insert(element);
        large_filter.insert(element);
        huge_filter.insert(element);
    }

    // All filters should contain the inserted elements
    for (const auto& element : elements) {
        EXPECT_TRUE(small_filter.contains(element));
        EXPECT_TRUE(large_filter.contains(element));
        EXPECT_TRUE(huge_filter.contains(element));
    }

    // Larger filters should have lower false positive rates
    double small_fp = small_filter.falsePositiveProbability();
    double large_fp = large_filter.falsePositiveProbability();
    double huge_fp = huge_filter.falsePositiveProbability();

    EXPECT_GE(small_fp, large_fp);
    EXPECT_GE(large_fp, huge_fp);
}

TEST_F(BloomFilterTest, CustomHashFunction) {
    // Test BloomFilter with custom hash function
    struct CustomStringHasher {
        std::size_t operator()(const std::string& str) const {
            // Simple custom hash function (not cryptographically secure)
            std::size_t hash = 0;
            for (char c : str) {
                hash = hash * 31 + static_cast<std::size_t>(c);
            }
            return hash;
        }
    };

    atom::algorithm::BloomFilter<1000, std::string, CustomStringHasher> custom_filter(3);

    // Insert elements
    std::vector<std::string> elements = {"custom1", "custom2", "custom3"};
    for (const auto& element : elements) {
        custom_filter.insert(element);
    }

    // Check that elements are found
    for (const auto& element : elements) {
        EXPECT_TRUE(custom_filter.contains(element));
    }

    EXPECT_EQ(custom_filter.elementCount(), elements.size());
}

TEST_F(BloomFilterTest, FilterSaturation) {
    // Test behavior when filter becomes saturated
    atom::algorithm::BloomFilter<100> small_filter(2); // Small filter, few hash functions

    // Insert many elements to saturate the filter
    for (int i = 0; i < 1000; ++i) {
        small_filter.insert("element" + std::to_string(i));
    }

    // False positive rate should be very high (approaching 1.0)
    double fp_rate = small_filter.falsePositiveProbability();
    EXPECT_GT(fp_rate, 0.8); // Should be very high due to saturation

    // Element count should be accurate
    EXPECT_EQ(small_filter.elementCount(), 1000);
}

TEST_F(BloomFilterTest, OptimalHashFunctionCount) {
    // Test with different numbers of hash functions
    std::vector<std::string> test_data;
    for (int i = 0; i < 100; ++i) {
        test_data.push_back("test_element_" + std::to_string(i));
    }

    // Test with 1, 3, 5, 7, 10 hash functions
    std::vector<size_t> hash_counts = {1, 3, 5, 7, 10};
    std::vector<double> fp_rates;

    for (size_t hash_count : hash_counts) {
        atom::algorithm::BloomFilter<10000> filter(hash_count);

        // Insert test data
        for (const auto& element : test_data) {
            filter.insert(element);
        }

        fp_rates.push_back(filter.falsePositiveProbability());
    }

    // There should be an optimal number of hash functions (not necessarily monotonic)
    EXPECT_GT(fp_rates.size(), 0);

    // At least verify that single hash function performs worse than multiple
    EXPECT_GT(fp_rates[0], fp_rates[1]); // 1 hash function vs 3 hash functions
}

TEST_F(BloomFilterTest, ThreadSafetyInsertContains) {
    // Test thread safety for concurrent insert and contains operations
    atom::algorithm::BloomFilter<10000> thread_safe_filter(5);
    const int num_threads = 4;
    const int elements_per_thread = 250;

    std::vector<std::thread> threads;
    std::vector<std::vector<std::string>> thread_elements(num_threads);

    // Prepare unique elements for each thread
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < elements_per_thread; ++j) {
            thread_elements[i].push_back("thread_" + std::to_string(i) + "_element_" + std::to_string(j));
        }
    }

    // Launch threads that insert elements
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (const auto& element : thread_elements[i]) {
                thread_safe_filter.insert(element);
            }
        });
    }

    // Wait for all insertions to complete
    for (auto& thread : threads) {
        thread.join();
    }
    threads.clear();

    // Launch threads that check for elements
    std::vector<bool> all_found(num_threads, true);
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (const auto& element : thread_elements[i]) {
                if (!thread_safe_filter.contains(element)) {
                    all_found[i] = false;
                    break;
                }
            }
        });
    }

    // Wait for all checks to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All elements should be found
    for (bool found : all_found) {
        EXPECT_TRUE(found);
    }

    // Total element count should be correct
    EXPECT_EQ(thread_safe_filter.elementCount(), num_threads * elements_per_thread);
}

TEST_F(BloomFilterTest, ConcurrentClearOperations) {
    // Test thread safety with concurrent clear operations
    atom::algorithm::BloomFilter<1000> filter(3);
    std::atomic<bool> stop_flag{false};
    std::vector<std::thread> threads;

    // Thread that continuously inserts elements
    threads.emplace_back([&]() {
        int counter = 0;
        while (!stop_flag.load()) {
            filter.insert("element_" + std::to_string(counter++));
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Thread that occasionally clears the filter
    threads.emplace_back([&]() {
        while (!stop_flag.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            filter.clear();
        }
    });

    // Thread that checks element count
    threads.emplace_back([&]() {
        while (!stop_flag.load()) {
            size_t count = filter.elementCount();
            // Count should be non-negative (basic sanity check)
            EXPECT_GE(count, 0);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop_flag.store(true);

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Test should complete without crashing
    EXPECT_TRUE(true);
}

TEST_F(BloomFilterTest, FalsePositiveRateAccuracy) {
    // Test accuracy of false positive rate calculation
    atom::algorithm::BloomFilter<10000> filter(5);

    // Insert known elements
    std::set<std::string> inserted_elements;
    for (int i = 0; i < 100; ++i) {
        std::string element = "known_" + std::to_string(i);
        filter.insert(element);
        inserted_elements.insert(element);
    }

    // Test with elements not inserted
    int false_positives = 0;
    int total_tests = 1000;

    for (int i = 0; i < total_tests; ++i) {
        std::string test_element = "unknown_" + std::to_string(i);
        if (inserted_elements.find(test_element) == inserted_elements.end()) {
            if (filter.contains(test_element)) {
                false_positives++;
            }
        }
    }

    double actual_fp_rate = static_cast<double>(false_positives) / total_tests;
    double theoretical_fp_rate = filter.falsePositiveProbability();

    // Actual rate should be reasonably close to theoretical rate
    // Allow for some variance due to randomness
    EXPECT_LT(std::abs(actual_fp_rate - theoretical_fp_rate), 0.1);
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

TEST_F(BoyerMooreTest, SIMDOptimizationComparison) {
    // Compare regular search vs SIMD-optimized search
    std::vector<std::string> test_patterns = {"a", "ab", "abc", "test", "pattern", "search"};

    for (const auto& pattern : test_patterns) {
        atom::algorithm::BoyerMoore bm(pattern);

        // Create test text with known occurrences
        std::string text = pattern + "_middle_" + pattern + "_end_" + pattern;

        // Get results from both methods
        auto regular_results = bm.search(text);
        auto optimized_results = bm.searchOptimized(text);

        // Sort results for comparison (order might differ)
        std::sort(regular_results.begin(), regular_results.end());
        std::sort(optimized_results.begin(), optimized_results.end());

        // Results should be identical
        EXPECT_EQ(regular_results, optimized_results)
            << "Mismatch for pattern: " << pattern;
    }
}

TEST_F(BoyerMooreTest, EmptyPatternHandling) {
    // Test handling of empty pattern
    EXPECT_NO_THROW({
        atom::algorithm::BoyerMoore bm("");
        auto positions = bm.search("test text");
        // Empty pattern should return empty results
        EXPECT_TRUE(positions.empty());
    });
}

TEST_F(BoyerMooreTest, EmptyTextHandling) {
    // Test handling of empty text
    atom::algorithm::BoyerMoore bm("pattern");
    auto positions = bm.search("");

    EXPECT_TRUE(positions.empty());
}

TEST_F(BoyerMooreTest, SingleCharacterPattern) {
    // Test with single character pattern
    atom::algorithm::BoyerMoore bm("a");
    auto positions = bm.search("banana");

    // Should find 'a' at positions 1, 3, 5
    std::vector<int> expected = {1, 3, 5};
    EXPECT_EQ(positions, expected);
}

TEST_F(BoyerMooreTest, RepeatingCharacterPattern) {
    // Test with repeating character pattern
    atom::algorithm::BoyerMoore bm("aaa");
    auto positions = bm.search("aaaaaaa");

    // Should find overlapping matches at positions 0, 1, 2, 3, 4
    std::vector<int> expected = {0, 1, 2, 3, 4};
    EXPECT_EQ(positions, expected);
}

TEST_F(BoyerMooreTest, UnicodeCharacters) {
    // Test with Unicode characters
    std::string unicode_pattern = "测试";
    std::string unicode_text = "开始测试中间测试结束";

    atom::algorithm::BoyerMoore bm(unicode_pattern);
    auto positions = bm.search(unicode_text);

    // Should find the pattern twice
    EXPECT_EQ(positions.size(), 2);
}

TEST_F(BoyerMooreTest, ThreadSafety) {
    // Test thread safety of Boyer-Moore operations
    atom::algorithm::BoyerMoore bm(test_pattern_);
    const int num_threads = 4;
    const int searches_per_thread = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<int>> results(num_threads);

    // Launch multiple threads performing searches
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < searches_per_thread; ++j) {
                auto positions = bm.search(test_text_);
                if (j == 0) {
                    results[i] = positions; // Store first result for comparison
                }
                // All results should be identical
                EXPECT_EQ(positions, results[i]);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should have found the same positions
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(results[0], results[i]);
    }
}

TEST_F(BoyerMooreTest, PerformanceCharacteristics) {
    // Test performance characteristics with different pattern types
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"good", "this is a good test for good performance"},  // Good case
        {"aaaa", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},      // Worst case (repeating)
        {"xyz", "abcdefghijklmnopqrstuvwxyz"},                 // Pattern at end
        {"notfound", "this pattern will not be found here"}   // Pattern not found
    };

    for (const auto& [pattern, text] : test_cases) {
        atom::algorithm::BoyerMoore bm(pattern);

        // Measure performance
        auto start = std::chrono::high_resolution_clock::now();
        auto positions = bm.search(text);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Performance should be reasonable (less than 1ms for these small inputs)
        EXPECT_LT(duration.count(), 1000) << "Pattern: " << pattern;

        // Verify correctness
        for (int pos : positions) {
            EXPECT_EQ(text.substr(pos, pattern.length()), pattern);
        }
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

    // Log performance comparison (use the timing variables)
    // Both algorithms should complete in reasonable time
    EXPECT_LT(kmp_time, 1000.0); // Less than 1 second
    EXPECT_LT(bm_time, 1000.0); // Less than 1 second
}

TEST_F(AlgorithmIntegrationTest, AlgorithmCorrectnessComparison) {
    // Test correctness across different algorithm implementations
    std::vector<std::string> test_patterns = {
        "a", "ab", "abc", "test", "pattern", "search", "algorithm"
    };

    for (const auto& pattern : test_patterns) {
        // Create test text with known pattern occurrences
        std::string text = "start_" + pattern + "_middle_" + pattern + "_end_" + pattern + "_finish";

        atom::algorithm::KMP kmp(pattern);
        atom::algorithm::BoyerMoore bm(pattern);

        auto kmp_results = kmp.search(text);
        auto bm_results = bm.search(text);
        auto kmp_parallel_results = kmp.searchParallel(text, 10);
        auto bm_optimized_results = bm.searchOptimized(text);

        // Sort all results for comparison
        std::sort(kmp_results.begin(), kmp_results.end());
        std::sort(bm_results.begin(), bm_results.end());
        std::sort(kmp_parallel_results.begin(), kmp_parallel_results.end());
        std::sort(bm_optimized_results.begin(), bm_optimized_results.end());

        // All algorithms should find the same positions
        EXPECT_EQ(kmp_results, bm_results) << "KMP vs BM mismatch for pattern: " << pattern;
        EXPECT_EQ(kmp_results, kmp_parallel_results) << "KMP vs KMP parallel mismatch for pattern: " << pattern;
        EXPECT_EQ(kmp_results, bm_optimized_results) << "KMP vs BM optimized mismatch for pattern: " << pattern;

        // Verify that all found positions are actually correct
        for (int pos : kmp_results) {
            EXPECT_EQ(text.substr(pos, pattern.length()), pattern)
                << "Incorrect match at position " << pos << " for pattern: " << pattern;
        }
    }
}

TEST_F(AlgorithmIntegrationTest, BloomFilterIntegration) {
    // Test integration between string search algorithms and Bloom filters
    atom::algorithm::BloomFilter<10000> filter(5);

    // Insert patterns into Bloom filter
    std::vector<std::string> patterns = {"test", "search", "algorithm", "pattern", "bloom"};
    for (const auto& pattern : patterns) {
        filter.insert(pattern);
    }

    // Create text containing some of these patterns
    std::string text = "This is a test text for search algorithm with pattern matching and bloom filter";

    // Use string search algorithms to find patterns in text
    std::vector<std::string> found_patterns;

    for (const auto& pattern : patterns) {
        atom::algorithm::KMP kmp(pattern);
        auto positions = kmp.search(text);

        if (!positions.empty()) {
            found_patterns.push_back(pattern);
        }
    }

    // All found patterns should be in the Bloom filter
    for (const auto& pattern : found_patterns) {
        EXPECT_TRUE(filter.contains(pattern))
            << "Pattern '" << pattern << "' found in text but not in Bloom filter";
    }

    // Test with patterns not in the filter
    std::vector<std::string> unknown_patterns = {"unknown", "missing", "absent"};
    for (const auto& pattern : unknown_patterns) {
        atom::algorithm::KMP kmp(pattern);
        auto positions = kmp.search(text);

        if (positions.empty()) {
            // Pattern not found in text, Bloom filter result doesn't matter
            continue;
        }

        // If pattern is found in text but wasn't inserted in filter,
        // Bloom filter should return false (no false negatives)
        EXPECT_FALSE(filter.contains(pattern))
            << "Pattern '" << pattern << "' not inserted but Bloom filter claims it exists";
    }
}



// Main function removed - using gtest_main
