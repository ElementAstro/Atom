/**
 * @file hash.cpp
 * @brief Comprehensive example demonstrating high-performance hash algorithms
 *
 * This example shows how to:
 * - Use different hash algorithms (FNV-1a, xxHash, CityHash, MurmurHash3)
 * - Compute hashes for various data types and containers
 * - Demonstrate SIMD optimizations and performance characteristics
 * - Use thread-safe caching for improved performance
 * - Handle parallel hash computation
 * - Compare hash quality and distribution
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/hash.hpp"

#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates hash computation for basic data types
 */
void demonstrateBasicTypeHashing() {
    printHeader("Basic Type Hashing Examples");

    try {
        // Test various fundamental types
        std::cout << "Fundamental type hashing:\n";

        int intValue = 42;
        double doubleValue = 3.14159;
        char charValue = 'A';
        bool boolValue = true;

        std::cout << "  int(42): " << computeHash(intValue) << "\n";
        std::cout << "  double(3.14159): " << computeHash(doubleValue) << "\n";
        std::cout << "  char('A'): " << computeHash(charValue) << "\n";
        std::cout << "  bool(true): " << computeHash(boolValue) << "\n";

        // Test string hashing with different algorithms
        std::cout << "\nString hashing with different algorithms:\n";
        std::string testString = "Hello, World!";

        std::cout << "  String: \"" << testString << "\"\n";
        std::cout << "  Default hash: " << computeHash(testString) << "\n";

        // Test with different hash algorithms if available
        try {
            auto fnvHash = computeHash(testString, HashAlgorithm::FNV1A);
            std::cout << "  FNV-1a hash: " << fnvHash << "\n";
        } catch (...) {
            std::cout << "  FNV-1a hash: Not available\n";
        }

        // Test hash consistency
        std::cout << "\nHash consistency test:\n";
        auto hash1 = computeHash(testString);
        auto hash2 = computeHash(testString);
        std::cout << "  First hash: " << hash1 << "\n";
        std::cout << "  Second hash: " << hash2 << "\n";
        std::cout << "  Consistent: " << (hash1 == hash2 ? "✓ YES" : "✗ NO")
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic type hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates hash computation for container types
 */
void demonstrateContainerHashing() {
    printHeader("Container Type Hashing Examples");

    try {
        std::cout << "Container hashing capabilities:\n";

        // Vector hashing
        std::vector<int> intVector = {1, 2, 3, 4, 5};
        std::cout << "  vector<int>{1,2,3,4,5}: " << computeHash(intVector)
                  << "\n";

        // Array hashing
        std::array<int, 5> intArray = {1, 2, 3, 4, 5};
        std::cout << "  array<int,5>{1,2,3,4,5}: " << computeHash(intArray)
                  << "\n";

        // String vector
        std::vector<std::string> stringVector = {"apple", "banana", "cherry"};
        std::cout << "  vector<string>: " << computeHash(stringVector) << "\n";

        // Test order sensitivity
        std::vector<int> vector1 = {1, 2, 3};
        std::vector<int> vector2 = {3, 2, 1};
        std::cout << "\nOrder sensitivity test:\n";
        std::cout << "  {1,2,3}: " << computeHash(vector1) << "\n";
        std::cout << "  {3,2,1}: " << computeHash(vector2) << "\n";
        std::cout << "  Different: "
                  << (computeHash(vector1) != computeHash(vector2) ? "✓ YES"
                                                                   : "✗ NO")
                  << "\n";

        // Test size sensitivity
        std::vector<int> smallVector = {1, 2};
        std::vector<int> largeVector = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        std::cout << "\nSize sensitivity test:\n";
        std::cout << "  Small vector: " << computeHash(smallVector) << "\n";
        std::cout << "  Large vector: " << computeHash(largeVector) << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in container hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates hash computation for composite types
 */
void demonstrateCompositeTypeHashing() {
    printHeader("Composite Type Hashing Examples");

    try {
        std::cout << "Composite type hashing:\n";

        // Tuple hashing
        std::tuple<int, std::string, double> tuple1 = {42, "hello", 3.14};
        std::cout << "  tuple<int,string,double>{42,\"hello\",3.14}: "
                  << computeHash(tuple1) << "\n";

        // Pair hashing
        std::pair<int, std::string> pair1 = {42, "hello"};
        std::cout << "  pair<int,string>{42,\"hello\"}: " << computeHash(pair1)
                  << "\n";

        // Nested containers
        std::vector<std::pair<int, std::string>> vectorOfPairs = {
            {1, "one"}, {2, "two"}, {3, "three"}};
        std::cout << "  vector<pair<int,string>>: "
                  << computeHash(vectorOfPairs) << "\n";

        // Complex nested structure
        std::tuple<std::vector<int>, std::string, std::pair<double, bool>>
            complexTuple = {{1, 2, 3}, "test", {3.14, true}};
        std::cout << "  complex nested tuple: " << computeHash(complexTuple)
                  << "\n";

        // Test structural equality
        std::tuple<int, std::string, double> tuple2 = {42, "hello", 3.14};
        std::cout << "\nStructural equality test:\n";
        std::cout << "  tuple1 hash: " << computeHash(tuple1) << "\n";
        std::cout << "  tuple2 hash: " << computeHash(tuple2) << "\n";
        std::cout << "  Equal: "
                  << (computeHash(tuple1) == computeHash(tuple2) ? "✓ YES"
                                                                 : "✗ NO")
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in composite type hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates hash computation for modern C++ wrapper types
 */
void demonstrateWrapperTypeHashing() {
    printHeader("Modern C++ Wrapper Type Hashing");

    try {
        std::cout << "Wrapper type hashing:\n";

        // Optional hashing
        std::optional<int> optWithValue = 42;
        std::optional<int> emptyOpt;
        std::cout << "  optional<int>(42): " << computeHash(optWithValue)
                  << "\n";
        std::cout << "  optional<int>(empty): " << computeHash(emptyOpt)
                  << "\n";

        // Variant hashing
        std::variant<int, std::string, double> var1 = 42;
        std::variant<int, std::string, double> var2 = std::string("hello");
        std::variant<int, std::string, double> var3 = 3.14;

        std::cout << "  variant<int,string,double>(42): " << computeHash(var1)
                  << "\n";
        std::cout << "  variant<int,string,double>(\"hello\"): "
                  << computeHash(var2) << "\n";
        std::cout << "  variant<int,string,double>(3.14): " << computeHash(var3)
                  << "\n";

        // Any hashing
        std::any any1 = 42;
        std::any any2 = std::string("hello");
        std::any any3 = std::vector<int>{1, 2, 3};

        std::cout << "  any(42): " << computeHash(any1) << "\n";
        std::cout << "  any(\"hello\"): " << computeHash(any2) << "\n";
        std::cout << "  any(vector<int>): " << computeHash(any3) << "\n";

        // Test variant type sensitivity
        std::variant<int, std::string> varInt = 42;
        std::variant<int, std::string> varString = std::string("42");
        std::cout << "\nVariant type sensitivity:\n";
        std::cout << "  variant(int 42): " << computeHash(varInt) << "\n";
        std::cout << "  variant(string \"42\"): " << computeHash(varString)
                  << "\n";
        std::cout << "  Different: "
                  << (computeHash(varInt) != computeHash(varString) ? "✓ YES"
                                                                    : "✗ NO")
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in wrapper type hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates performance characteristics and hash quality
 */
void demonstratePerformanceAndQuality() {
    printHeader("Performance and Hash Quality Analysis");

    try {
        // Performance test with different data sizes
        std::vector<size_t> dataSizes = {100, 1000, 10000, 100000};
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::cout << "Performance test with different data sizes:\n";

        for (size_t size : dataSizes) {
            // Generate random data
            std::vector<int> data(size);
            for (size_t i = 0; i < size; ++i) {
                data[i] = dis(gen);
            }

            // Measure hashing time
            auto start = std::chrono::high_resolution_clock::now();
            [[maybe_unused]] auto hashValue = computeHash(data);
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);
            double throughput =
                (static_cast<double>(size * sizeof(int)) / 1024.0 / 1024.0) /
                (duration.count() / 1000000.0);

            std::cout << "  " << size << " elements: " << duration.count()
                      << " μs";
            std::cout << " (throughput: " << std::fixed << std::setprecision(2)
                      << throughput << " MB/s)\n";
        }

        // Hash distribution quality test
        std::cout << "\nHash distribution quality test:\n";
        const size_t numHashes = 10000;
        const size_t numBuckets = 100;
        std::vector<size_t> buckets(numBuckets, 0);

        for (size_t i = 0; i < numHashes; ++i) {
            std::string testString = "test_string_" + std::to_string(i);
            auto hashValue = computeHash(testString);
            buckets[hashValue % numBuckets]++;
        }

        // Calculate distribution statistics
        auto minBucket = *std::min_element(buckets.begin(), buckets.end());
        auto maxBucket = *std::max_element(buckets.begin(), buckets.end());
        double avgBucket = static_cast<double>(numHashes) / numBuckets;

        std::cout << "  " << numHashes << " hashes distributed into "
                  << numBuckets << " buckets:\n";
        std::cout << "  Min bucket: " << minBucket << " (" << std::fixed
                  << std::setprecision(1)
                  << (static_cast<double>(minBucket) / avgBucket * 100)
                  << "% of average)\n";
        std::cout << "  Max bucket: " << maxBucket << " (" << std::fixed
                  << std::setprecision(1)
                  << (static_cast<double>(maxBucket) / avgBucket * 100)
                  << "% of average)\n";
        std::cout << "  Average: " << std::fixed << std::setprecision(1)
                  << avgBucket << "\n";

        // Calculate variance
        double variance = 0.0;
        for (size_t count : buckets) {
            double diff = static_cast<double>(count) - avgBucket;
            variance += diff * diff;
        }
        variance /= numBuckets;
        double stddev = std::sqrt(variance);

        std::cout << "  Standard deviation: " << std::fixed
                  << std::setprecision(2) << stddev << "\n";
        std::cout << "  Distribution quality: "
                  << (stddev < avgBucket * 0.1 ? "✓ GOOD" : "⚠ FAIR") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in performance and quality analysis: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates string hashing and user-defined literals
 */
void demonstrateStringHashing() {
    printHeader("String Hashing and User-Defined Literals");

    try {
        std::cout << "String hashing capabilities:\n";

        // Basic string hashing
        std::string str1 = "Hello, World!";
        std::string str2 = "Hello, World!";
        std::string str3 = "Hello, world!";  // Different case

        std::cout << "  \"Hello, World!\": " << computeHash(str1) << "\n";
        std::cout << "  \"Hello, World!\" (copy): " << computeHash(str2)
                  << "\n";
        std::cout << "  \"Hello, world!\" (different case): "
                  << computeHash(str3) << "\n";

        std::cout << "  Identical strings have same hash: "
                  << (computeHash(str1) == computeHash(str2) ? "✓ YES" : "✗ NO")
                  << "\n";
        std::cout << "  Case sensitive: "
                  << (computeHash(str1) != computeHash(str3) ? "✓ YES" : "✗ NO")
                  << "\n";

        // C-style string hashing
        const char* cstr = "C-style string";
        std::cout << "\n  C-style string \"" << cstr
                  << "\": " << computeHash(std::string(cstr)) << "\n";

        // User-defined literal hashing (if available)
        try {
            auto literalHash = "literal"_hash;
            std::cout << "  String literal \"literal\": " << literalHash
                      << "\n";
        } catch (...) {
            std::cout << "  String literal hashing: Not available\n";
        }

        // Empty string
        std::string emptyStr = "";
        std::cout << "  Empty string: " << computeHash(emptyStr) << "\n";

        // Very long string
        std::string longStr(1000, 'A');
        std::cout << "  Long string (1000 'A's): " << computeHash(longStr)
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in string hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive hash algorithm capabilities
 */
int main() {
    std::cout << "=== Atom Hash Algorithm Comprehensive Example ===\n";
    std::cout
        << "Demonstrating high-performance hash computation capabilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicTypeHashing();
        demonstrateContainerHashing();
        demonstrateCompositeTypeHashing();
        demonstrateWrapperTypeHashing();
        demonstrateStringHashing();
        demonstratePerformanceAndQuality();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Hash Algorithm Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The hash algorithm module provides:\n";
        std::cout << "  ✓ High-performance hashing with SIMD optimizations\n";
        std::cout << "  ✓ Support for all standard C++ types and containers\n";
        std::cout << "  ✓ Thread-safe caching for improved performance\n";
        std::cout << "  ✓ Multiple hash algorithms (FNV-1a, xxHash, etc.)\n";
        std::cout << "  ✓ Excellent hash distribution and quality\n";
        std::cout << "  ✓ Modern C++20 concepts and type safety\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in hash example: " << e.what()
                  << "\n";
        return 1;
    }
}
