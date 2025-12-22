/**
 * @file algorithm.cpp
 * @brief Comprehensive example demonstrating core algorithm concepts and
 * utilities
 *
 * This example shows how to:
 * - Use modern C++20 concepts and constraints in algorithm design
 * - Implement and use algorithm base classes and interfaces
 * - Demonstrate common algorithm patterns and utilities
 * - Show proper error handling and exception safety
 * - Utilize RAII and modern C++ best practices
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/core/algorithm.hpp"

#include <algorithm>
#include <chrono>
#include <concepts>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <ranges>
#include <string>
#include <vector>

using namespace atom::algorithm;

// Template function for policy-based algorithm designtemplate <typename
// Container, typename Predicate>
auto count_if_policy(const Container& container, Predicate pred) {
    return std::count_if(container.begin(), container.end(), pred);
}

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates C++20 concepts usage in algorithms
 */
void demonstrateConceptsUsage() {
    printHeader("C++20 Concepts in Algorithm Design");

    // Example of using concepts to constrain template parameters
    auto process_numeric = []<typename T>(T value)
        requires std::integral<T> || std::floating_point<T>
    {
        std::cout << "Processing numeric value: " << value
                  << " (type: " << typeid(T).name() << ")\n";
        return value * 2;
    };

    // Test with different numeric types
    std::cout << "Testing concept-constrained function:\n";
    auto int_result = process_numeric(42);
    auto float_result = process_numeric(3.14);
    auto double_result = process_numeric(2.718);

    std::cout << "Results: " << int_result << ", " << float_result << ", "
              << double_result << "\n";

    // Demonstrate container concepts
    auto print_container = []<typename Container>(const Container& container)
        requires std::ranges::range<Container>
    {
        std::cout << "Container contents: ";
        for (const auto& item : container) {
            std::cout << item << " ";
        }
        std::cout << "\n";
    };

    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::string str = "Hello";

    std::cout << "\nTesting range concepts:\n";
    print_container(vec);
    print_container(str);
}

/**
 * @brief Demonstrates algorithm performance measurement utilities
 */
void demonstratePerformanceMeasurement() {
    printHeader("Algorithm Performance Measurement");

    // Create test data
    std::vector<int> data(100000);
    std::iota(data.begin(), data.end(), 1);

    // Measure different sorting algorithms
    std::cout << "Comparing sorting algorithm performance:\n";

    // Test std::sort
    auto data_copy1 = data;
    auto start = std::chrono::high_resolution_clock::now();
    std::sort(data_copy1.begin(), data_copy1.end());
    auto end = std::chrono::high_resolution_clock::now();
    auto sort_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "std::sort time: " << sort_time.count() << " μs\n";

    // Test std::stable_sort
    auto data_copy2 = data;
    start = std::chrono::high_resolution_clock::now();
    std::stable_sort(data_copy2.begin(), data_copy2.end());
    end = std::chrono::high_resolution_clock::now();
    auto stable_sort_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "std::stable_sort time: " << stable_sort_time.count()
              << " μs\n";

    // Demonstrate algorithm complexity analysis
    std::cout << "\nAlgorithm complexity demonstration:\n";
    std::vector<size_t> sizes = {1000, 10000, 100000};

    for (size_t size : sizes) {
        std::vector<int> test_data(size);
        std::iota(test_data.begin(), test_data.end(), 1);
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(test_data.begin(), test_data.end(), g);

        start = std::chrono::high_resolution_clock::now();
        std::sort(test_data.begin(), test_data.end());
        end = std::chrono::high_resolution_clock::now();
        auto time =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Size: " << size << ", Time: " << time.count() << " μs, ";
        std::cout << "Time per element: "
                  << static_cast<double>(time.count()) / size << " μs\n";
    }
}

/**
 * @brief Demonstrates modern C++ algorithm patterns
 */
void demonstrateModernAlgorithmPatterns() {
    printHeader("Modern C++ Algorithm Patterns");

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Using ranges and views (C++20)
    std::cout << "Using C++20 ranges and views:\n";

    // Filter even numbers and transform them
    auto even_squares = numbers |
                        std::views::filter([](int n) { return n % 2 == 0; }) |
                        std::views::transform([](int n) { return n * n; });

    std::cout << "Even squares: ";
    for (int value : even_squares) {
        std::cout << value << " ";
    }
    std::cout << "\n";

    // Demonstrate algorithm composition
    std::cout << "\nAlgorithm composition patterns:\n";

    // Chain multiple transformations
    auto result =
        numbers | std::views::take(5)  // Take first 5 elements
        | std::views::reverse          // Reverse them
        | std::views::transform([](int n) { return n * 3; });  // Multiply by 3

    std::cout << "Composed transformation result: ";
    for (int value : result) {
        std::cout << value << " ";
    }
    std::cout << "\n";

    // Demonstrate parallel algorithms (if available)
    std::vector<int> large_data(10000);
    std::iota(large_data.begin(), large_data.end(), 1);

    auto start = std::chrono::high_resolution_clock::now();
    auto sum = std::reduce(large_data.begin(), large_data.end(), 0);
    auto end = std::chrono::high_resolution_clock::now();
    auto sequential_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "\nParallel vs Sequential comparison:\n";
    std::cout << "Sequential sum: " << sum
              << " (time: " << sequential_time.count() << " μs)\n";

    // Note: std::execution::par would require execution policy support
    // This is a placeholder for demonstration
    std::cout << "Parallel algorithms can provide significant speedup for "
                 "large datasets\n";
}

/**
 * @brief Demonstrates error handling and exception safety in algorithms
 */
void demonstrateErrorHandling() {
    printHeader("Error Handling and Exception Safety");

    // Demonstrate RAII pattern
    std::cout << "RAII pattern demonstration:\n";

    class ResourceManager {
    private:
        std::unique_ptr<int[]> data_;
        size_t size_;

    public:
        ResourceManager(size_t size) : size_(size) {
            data_ = std::make_unique<int[]>(size);
            std::cout << "  Resource allocated for " << size << " elements\n";
        }

        ~ResourceManager() {
            std::cout << "  Resource automatically cleaned up\n";
        }

        void process() {
            // Simulate some processing that might throw
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = static_cast<int>(i * i);
            }
            std::cout << "  Processing completed successfully\n";
        }

        int* get_data() const { return data_.get(); }
        size_t size() const { return size_; }
    };

    try {
        ResourceManager manager(1000);
        manager.process();

        // Demonstrate exception safety
        std::cout << "\nException safety demonstration:\n";
        std::vector<int> safe_vector;
        safe_vector.reserve(
            100);  // Pre-allocate to avoid reallocation exceptions

        for (int i = 0; i < 100; ++i) {
            safe_vector.push_back(i);  // Strong exception safety guarantee
        }

        std::cout << "  Vector operations completed safely\n";

    } catch (const std::exception& e) {
        std::cout << "  Exception caught: " << e.what() << "\n";
    }

    // Demonstrate error codes vs exceptions
    std::cout << "\nError handling strategies:\n";

    auto safe_divide = [](double a, double b) -> std::optional<double> {
        if (b == 0.0) {
            return std::nullopt;  // Return empty optional for error
        }
        return a / b;
    };

    auto result1 = safe_divide(10.0, 2.0);
    auto result2 = safe_divide(10.0, 0.0);

    if (result1) {
        std::cout << "  Division result: " << *result1 << "\n";
    }

    if (!result2) {
        std::cout << "  Division by zero handled gracefully\n";
    }
}

/**
 * @brief Demonstrates algorithm customization and policy-based design
 */
void demonstrateAlgorithmCustomization() {
    printHeader("Algorithm Customization and Policy-Based Design");

    // Demonstrate custom comparators
    std::cout << "Custom comparator demonstration:\n";

    std::vector<std::string> words = {"apple", "Banana", "cherry", "Date"};

    // Case-insensitive comparison
    auto case_insensitive = [](const std::string& a, const std::string& b) {
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(), [](char c1, char c2) {
                return std::tolower(c1) < std::tolower(c2);
            });
    };

    std::cout << "  Original order: ";
    for (const auto& word : words) {
        std::cout << word << " ";
    }
    std::cout << "\n";

    std::sort(words.begin(), words.end(), case_insensitive);

    std::cout << "  Case-insensitive sorted: ";
    for (const auto& word : words) {
        std::cout << word << " ";
    }
    std::cout << "\n";

    // Demonstrate algorithm policies
    std::cout << "\nPolicy-based algorithm design:\n";

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto even_count =
        count_if_policy(numbers, [](int n) { return n % 2 == 0; });
    auto greater_than_5 = count_if_policy(numbers, [](int n) { return n > 5; });

    std::cout << "  Even numbers count: " << even_count << "\n";
    std::cout << "  Numbers > 5 count: " << greater_than_5 << "\n";
}

/**
 * @brief Main function demonstrating comprehensive core algorithm usage
 */
int main() {
    std::cout << "=== Atom Core Algorithm Comprehensive Example ===\n";
    std::cout << "Demonstrating fundamental algorithm concepts and modern C++ "
                 "features...\n";

    try {
        // Run all demonstration functions
        demonstrateConceptsUsage();
        demonstratePerformanceMeasurement();
        demonstrateModernAlgorithmPatterns();
        demonstrateErrorHandling();
        demonstrateAlgorithmCustomization();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Core Algorithm Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The core algorithm module provides:\n";
        std::cout << "  ✓ Modern C++20 concepts and constraints\n";
        std::cout << "  ✓ Performance measurement and analysis tools\n";
        std::cout << "  ✓ Exception-safe algorithm implementations\n";
        std::cout << "  ✓ Policy-based design patterns\n";
        std::cout << "  ✓ RAII and resource management utilities\n";
        std::cout << "  ✓ Composable algorithm building blocks\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in core algorithm example: "
                  << e.what() << "\n";
        return 1;
    }
}
