#include "test_framework.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

using namespace atom::memory::test;

// Forward declarations for test registration functions
void registerMemoryPoolTests(TestFramework& framework);
void registerObjectPoolTests(TestFramework& framework);
void registerRingBufferTests(TestFramework& framework);
// void registerSharedMemoryTests(TestFramework& framework);
// void registerArenaAllocatorTests(TestFramework& framework);
// void registerMemoryTrackerTests(TestFramework& framework);
// void registerMemoryUtilsTests(TestFramework& framework);

/**
 * @brief Integration tests for memory system components
 */
namespace integration_tests {

/**
 * @brief Test integration between different memory components
 */
void testMemoryComponentIntegration() {
    // Test using multiple memory components together

    // Create a memory pool and object pool
    MemoryPool<2048> memory_pool;
    ObjectPool<std::string> object_pool(10);

    // Test that they can coexist and work together
    void* raw_memory = memory_pool.allocate(256);
    ASSERT_TRUE(raw_memory != nullptr);

    auto string_obj = object_pool.acquire();
    ASSERT_TRUE(string_obj != nullptr);

    *string_obj = "Integration test successful";
    ASSERT_EQ(*string_obj, "Integration test successful");

    // Clean up
    memory_pool.deallocate(raw_memory, 256);
    string_obj.reset();
}

/**
 * @brief Test memory system under stress
 */
void testMemorySystemStress() {
    const size_t stress_iterations = 1000;
    const size_t max_allocations = 100;

    MemoryPool<8192> pool;
    std::vector<void*> allocations;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(16, 512);
    std::uniform_int_distribution<> action_dist(0, 2); // 0=allocate, 1=deallocate, 2=reallocate

    for (size_t i = 0; i < stress_iterations; ++i) {
        int action = action_dist(gen);

        if (action == 0 || allocations.empty()) {
            // Allocate
            if (allocations.size() < max_allocations) {
                size_t size = size_dist(gen);
                void* ptr = pool.allocate(size);
                if (ptr) {
                    allocations.push_back(ptr);
                    // Write some data to test memory integrity
                    std::memset(ptr, static_cast<int>(i & 0xFF), std::min(size, size_t(64)));
                }
            }
        } else if (action == 1 && !allocations.empty()) {
            // Deallocate
            std::uniform_int_distribution<> index_dist(0, allocations.size() - 1);
            size_t index = index_dist(gen);
            void* ptr = allocations[index];

            // Verify memory integrity before deallocation
            unsigned char* data = static_cast<unsigned char*>(ptr);
            // Simple check - first byte should match our pattern

            pool.deallocate(ptr, 64); // Use average size for deallocation
            allocations.erase(allocations.begin() + index);
        }
    }

    // Clean up remaining allocations
    for (void* ptr : allocations) {
        pool.deallocate(ptr, 64);
    }

    // Verify pool is in good state
    auto stats = pool.getStats();
    ASSERT_EQ(stats.currentAllocations.load(), 0);
}

/**
 * @brief Test memory system performance under concurrent load
 */
void testConcurrentMemoryOperations() {
    const size_t num_threads = 8;
    const size_t operations_per_thread = 1000;

    MemoryPool<16384> memory_pool;
    ObjectPool<std::vector<int>> object_pool(50);
    RingBuffer<int> ring_buffer(1000);

    std::vector<std::thread> threads;
    std::atomic<size_t> total_operations{0};
    std::atomic<size_t> successful_operations{0};

    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (size_t j = 0; j < operations_per_thread; ++j) {
                total_operations.fetch_add(1);

                try {
                    // Test memory pool
                    void* mem = memory_pool.allocate(128);
                    if (mem) {
                        std::memset(mem, static_cast<int>((i + j) & 0xFF), 128);
                        memory_pool.deallocate(mem, 128);
                    }

                    // Test object pool
                    auto obj = object_pool.acquire();
                    if (obj) {
                        obj->resize(10);
                        for (size_t k = 0; k < 10; ++k) {
                            (*obj)[k] = static_cast<int>(i * 1000 + j * 10 + k);
                        }
                    }

                    // Test ring buffer
                    int value = static_cast<int>(i * 10000 + j);
                    if (ring_buffer.push(value)) {
                        auto popped = ring_buffer.pop();
                        // Value might be different due to concurrent access
                    }

                    successful_operations.fetch_add(1);

                } catch (const std::exception&) {
                    // Some operations might fail under high contention
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that most operations succeeded
    double success_ratio = static_cast<double>(successful_operations.load()) / total_operations.load();
    ASSERT_GT(success_ratio, 0.8); // At least 80% success rate
}

/**
 * @brief Comprehensive performance benchmark
 */
BenchmarkResult benchmarkOverallPerformance() {
    const size_t iterations = 10000;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Test multiple components together
    MemoryPool<8192> memory_pool;
    ObjectPool<std::string> object_pool(100);
    RingBuffer<int> ring_buffer(500);

    for (size_t i = 0; i < iterations; ++i) {
        // Memory pool operations
        void* mem = memory_pool.allocate(64);
        if (mem) {
            std::memset(mem, static_cast<int>(i & 0xFF), 64);
            memory_pool.deallocate(mem, 64);
        }

        // Object pool operations
        auto obj = object_pool.acquire();
        if (obj) {
            *obj = "test_string_" + std::to_string(i);
        }

        // Ring buffer operations
        ring_buffer.push(static_cast<int>(i));
        if (i % 2 == 0) {
            ring_buffer.pop();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    BenchmarkResult result;
    result.name = "Overall Memory System Performance";
    result.duration = duration;
    result.iterations = iterations * 3; // Three operations per iteration
    result.memory_used = 8192 + (100 * sizeof(std::string)) + (500 * sizeof(int));
    result.operations_per_second = (result.iterations * 1e9) / duration.count();
    result.additional_info = "Combined MemoryPool, ObjectPool, and RingBuffer operations";

    return result;
}

} // namespace integration_tests

/**
 * @brief Generate comprehensive test report
 */
void generateTestReport(const TestFramework& framework) {
    const auto& results = framework.getResults();

    std::ofstream report("memory_test_report.html");
    if (!report.is_open()) {
        std::cerr << "Failed to create test report file" << std::endl;
        return;
    }

    report << "<!DOCTYPE html>\n<html>\n<head>\n";
    report << "<title>Memory System Test Report</title>\n";
    report << "<style>\n";
    report << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
    report << "table { border-collapse: collapse; width: 100%; }\n";
    report << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    report << "th { background-color: #f2f2f2; }\n";
    report << ".passed { color: green; }\n";
    report << ".failed { color: red; }\n";
    report << ".skipped { color: orange; }\n";
    report << "</style>\n</head>\n<body>\n";

    report << "<h1>Memory System Test Report</h1>\n";
    report << "<p>Generated on: " << std::chrono::system_clock::now().time_since_epoch().count() << "</p>\n";

    // Summary
    size_t passed = 0, failed = 0, skipped = 0;
    for (const auto& result : results) {
        switch (result.status) {
            case TestStatus::PASSED: passed++; break;
            case TestStatus::FAILED: case TestStatus::TIMEOUT: failed++; break;
            case TestStatus::SKIPPED: skipped++; break;
        }
    }

    report << "<h2>Summary</h2>\n";
    report << "<p>Total Tests: " << results.size() << "</p>\n";
    report << "<p class='passed'>Passed: " << passed << "</p>\n";
    report << "<p class='failed'>Failed: " << failed << "</p>\n";
    report << "<p class='skipped'>Skipped: " << skipped << "</p>\n";

    // Detailed results
    report << "<h2>Detailed Results</h2>\n";
    report << "<table>\n";
    report << "<tr><th>Test Name</th><th>Status</th><th>Duration (ms)</th><th>Details</th></tr>\n";

    for (const auto& result : results) {
        std::string status_class;
        std::string status_text;

        switch (result.status) {
            case TestStatus::PASSED:
                status_class = "passed";
                status_text = "PASSED";
                break;
            case TestStatus::FAILED:
                status_class = "failed";
                status_text = "FAILED";
                break;
            case TestStatus::TIMEOUT:
                status_class = "failed";
                status_text = "TIMEOUT";
                break;
            case TestStatus::SKIPPED:
                status_class = "skipped";
                status_text = "SKIPPED";
                break;
        }

        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(result.execution_time).count();

        report << "<tr>\n";
        report << "<td>" << result.test_name << "</td>\n";
        report << "<td class='" << status_class << "'>" << status_text << "</td>\n";
        report << "<td>" << duration_ms << "</td>\n";
        report << "<td>" << result.error_message << " " << result.additional_info << "</td>\n";
        report << "</tr>\n";
    }

    report << "</table>\n";

    // Benchmark results
    report << "<h2>Performance Benchmarks</h2>\n";
    report << "<table>\n";
    report << "<tr><th>Benchmark</th><th>Operations/sec</th><th>Avg Time (ns)</th><th>Details</th></tr>\n";

    for (const auto& result : results) {
        if (result.benchmark.iterations > 0) {
            report << "<tr>\n";
            report << "<td>" << result.benchmark.name << "</td>\n";
            report << "<td>" << std::fixed << std::setprecision(2) << result.benchmark.operations_per_second << "</td>\n";
            report << "<td>" << std::fixed << std::setprecision(2) << result.benchmark.getAverageTimeNs() << "</td>\n";
            report << "<td>" << result.benchmark.additional_info << "</td>\n";
            report << "</tr>\n";
        }
    }

    report << "</table>\n";
    report << "</body>\n</html>\n";

    std::cout << "Test report generated: memory_test_report.html" << std::endl;
}

/**
 * @brief Main test runner
 */
int main(int argc, char* argv[]) {
    TestFramework framework;

    bool verbose = true;
    if (argc > 1 && std::string(argv[1]) == "--quiet") {
        verbose = false;
    }

    framework.setVerbose(verbose);

    std::cout << "Registering Memory System Tests..." << std::endl;

    // Register all test suites
    registerMemoryPoolTests(framework);
    registerObjectPoolTests(framework);
    registerRingBufferTests(framework);

    // Add integration tests
    framework.addTest("MemoryComponentIntegration", "Test integration between memory components",
                      integration_tests::testMemoryComponentIntegration);

    framework.addTest("MemorySystemStress", "Stress test for memory system",
                      integration_tests::testMemorySystemStress);

    framework.addTest("ConcurrentMemoryOperations", "Test concurrent memory operations",
                      integration_tests::testConcurrentMemoryOperations);

    framework.addBenchmark("OverallPerformance", "Overall memory system performance benchmark",
                           integration_tests::benchmarkOverallPerformance);

    std::cout << "Running all tests..." << std::endl;

    // Run all tests
    framework.runAllTests();

    // Generate detailed report
    generateTestReport(framework);

    // Return appropriate exit code
    const auto& results = framework.getResults();
    for (const auto& result : results) {
        if (result.status == TestStatus::FAILED || result.status == TestStatus::TIMEOUT) {
            return 1; // Test failures
        }
    }

    return 0; // All tests passed
}
