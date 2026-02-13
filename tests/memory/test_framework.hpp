#ifndef ATOM_MEMORY_TEST_FRAMEWORK_HPP
#define ATOM_MEMORY_TEST_FRAMEWORK_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <cassert>

namespace atom::memory::test {

/**
 * @brief Test result status
 */
enum class TestStatus {
    PASSED,
    FAILED,
    SKIPPED,
    TIMEOUT
};

/**
 * @brief Performance benchmark result
 */
struct BenchmarkResult {
    std::string name;
    std::chrono::nanoseconds duration;
    size_t iterations;
    size_t memory_used;
    double operations_per_second;
    std::string additional_info;

    double getAverageTimeNs() const {
        return static_cast<double>(duration.count()) / iterations;
    }
};

/**
 * @brief Memory leak detection result
 */
struct LeakDetectionResult {
    size_t leaked_allocations;
    size_t leaked_bytes;
    std::vector<std::string> leak_locations;
    bool has_leaks() const { return leaked_allocations > 0; }
};

/**
 * @brief Thread safety test result
 */
struct ThreadSafetyResult {
    size_t thread_count;
    std::chrono::nanoseconds total_duration;
    size_t race_conditions_detected;
    size_t deadlocks_detected;
    bool is_thread_safe() const { return race_conditions_detected == 0 && deadlocks_detected == 0; }
};

/**
 * @brief Test case information
 */
struct TestCase {
    std::string name;
    std::string description;
    std::function<void()> test_function;
    std::function<BenchmarkResult()> benchmark_function;
    std::function<LeakDetectionResult()> leak_test_function;
    std::function<ThreadSafetyResult()> thread_safety_function;
    bool enabled = true;
    std::chrono::milliseconds timeout{30000}; // 30 second default timeout
};

/**
 * @brief Test result
 */
struct TestResult {
    std::string test_name;
    TestStatus status;
    std::string error_message;
    std::chrono::nanoseconds execution_time;
    BenchmarkResult benchmark;
    LeakDetectionResult leak_detection;
    ThreadSafetyResult thread_safety;
    std::string additional_info;
};

/**
 * @brief Comprehensive test framework for memory components
 */
class TestFramework {
private:
    std::vector<TestCase> test_cases_;
    std::vector<TestResult> results_;
    std::mutex results_mutex_;
    std::atomic<size_t> passed_tests_{0};
    std::atomic<size_t> failed_tests_{0};
    std::atomic<size_t> skipped_tests_{0};
    bool verbose_output_ = true;

public:
    /**
     * @brief Add a test case
     */
    void addTest(const std::string& name, const std::string& description,
                 std::function<void()> test_func) {
        TestCase test_case;
        test_case.name = name;
        test_case.description = description;
        test_case.test_function = std::move(test_func);
        test_cases_.push_back(std::move(test_case));
    }

    /**
     * @brief Add a benchmark test
     */
    void addBenchmark(const std::string& name, const std::string& description,
                      std::function<BenchmarkResult()> benchmark_func) {
        TestCase test_case;
        test_case.name = name;
        test_case.description = description;
        test_case.benchmark_function = std::move(benchmark_func);
        test_cases_.push_back(std::move(test_case));
    }

    /**
     * @brief Add a memory leak detection test
     */
    void addLeakTest(const std::string& name, const std::string& description,
                     std::function<LeakDetectionResult()> leak_func) {
        TestCase test_case;
        test_case.name = name;
        test_case.description = description;
        test_case.leak_test_function = std::move(leak_func);
        test_cases_.push_back(std::move(test_case));
    }

    /**
     * @brief Add a thread safety test
     */
    void addThreadSafetyTest(const std::string& name, const std::string& description,
                             std::function<ThreadSafetyResult()> thread_func) {
        TestCase test_case;
        test_case.name = name;
        test_case.description = description;
        test_case.thread_safety_function = std::move(thread_func);
        test_cases_.push_back(std::move(test_case));
    }

    /**
     * @brief Run all tests
     */
    void runAllTests() {
        std::cout << "=== Memory System Test Framework ===" << std::endl;
        std::cout << "Running " << test_cases_.size() << " test cases..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        for (const auto& test_case : test_cases_) {
            if (!test_case.enabled) {
                recordResult(test_case.name, TestStatus::SKIPPED, "Test disabled", {});
                continue;
            }

            runSingleTest(test_case);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        printSummary(total_duration);
    }

    /**
     * @brief Get test results
     */
    const std::vector<TestResult>& getResults() const {
        return results_;
    }

    /**
     * @brief Set verbose output
     */
    void setVerbose(bool verbose) {
        verbose_output_ = verbose;
    }

private:
    void runSingleTest(const TestCase& test_case) {
        if (verbose_output_) {
            std::cout << "Running: " << test_case.name << " - " << test_case.description << std::endl;
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = test_case.name;
        result.status = TestStatus::PASSED;

        try {
            // Run basic test function
            if (test_case.test_function) {
                auto test_future = std::async(std::launch::async, test_case.test_function);
                if (test_future.wait_for(test_case.timeout) == std::future_status::timeout) {
                    result.status = TestStatus::TIMEOUT;
                    result.error_message = "Test timed out after " +
                        std::to_string(test_case.timeout.count()) + "ms";
                } else {
                    test_future.get(); // This will throw if the test failed
                }
            }

            // Run benchmark if available
            if (test_case.benchmark_function && result.status == TestStatus::PASSED) {
                result.benchmark = test_case.benchmark_function();
            }

            // Run leak detection if available
            if (test_case.leak_test_function && result.status == TestStatus::PASSED) {
                result.leak_detection = test_case.leak_test_function();
                if (result.leak_detection.has_leaks()) {
                    result.additional_info += "Memory leaks detected! ";
                }
            }

            // Run thread safety test if available
            if (test_case.thread_safety_function && result.status == TestStatus::PASSED) {
                result.thread_safety = test_case.thread_safety_function();
                if (!result.thread_safety.is_thread_safe()) {
                    result.additional_info += "Thread safety issues detected! ";
                }
            }

        } catch (const std::exception& e) {
            result.status = TestStatus::FAILED;
            result.error_message = e.what();
        } catch (...) {
            result.status = TestStatus::FAILED;
            result.error_message = "Unknown exception occurred";
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time);

        recordResult(result);

        if (verbose_output_) {
            printTestResult(result);
        }
    }

    void recordResult(const TestResult& result) {
        std::lock_guard<std::mutex> lock(results_mutex_);
        results_.push_back(result);

        switch (result.status) {
            case TestStatus::PASSED:
                passed_tests_.fetch_add(1);
                break;
            case TestStatus::FAILED:
            case TestStatus::TIMEOUT:
                failed_tests_.fetch_add(1);
                break;
            case TestStatus::SKIPPED:
                skipped_tests_.fetch_add(1);
                break;
        }
    }

    void recordResult(const std::string& name, TestStatus status,
                     const std::string& error, std::chrono::nanoseconds duration) {
        TestResult result;
        result.test_name = name;
        result.status = status;
        result.error_message = error;
        result.execution_time = duration;
        recordResult(result);
    }

    void printTestResult(const TestResult& result) {
        std::string status_str;
        switch (result.status) {
            case TestStatus::PASSED: status_str = "PASSED"; break;
            case TestStatus::FAILED: status_str = "FAILED"; break;
            case TestStatus::SKIPPED: status_str = "SKIPPED"; break;
            case TestStatus::TIMEOUT: status_str = "TIMEOUT"; break;
        }

        std::cout << "  " << status_str << " ("
                  << std::chrono::duration_cast<std::chrono::milliseconds>(result.execution_time).count()
                  << "ms)";

        if (!result.error_message.empty()) {
            std::cout << " - " << result.error_message;
        }

        if (!result.additional_info.empty()) {
            std::cout << " - " << result.additional_info;
        }

        std::cout << std::endl;

        // Print benchmark results if available
        if (result.benchmark.iterations > 0) {
            std::cout << "    Benchmark: " << result.benchmark.operations_per_second
                      << " ops/sec, avg: " << result.benchmark.getAverageTimeNs() << "ns" << std::endl;
        }
    }

    void printSummary(std::chrono::milliseconds total_duration) {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Total tests: " << test_cases_.size() << std::endl;
        std::cout << "Passed: " << passed_tests_.load() << std::endl;
        std::cout << "Failed: " << failed_tests_.load() << std::endl;
        std::cout << "Skipped: " << skipped_tests_.load() << std::endl;
        std::cout << "Total time: " << total_duration.count() << "ms" << std::endl;

        if (failed_tests_.load() == 0) {
            std::cout << "All tests PASSED! ✅" << std::endl;
        } else {
            std::cout << "Some tests FAILED! ❌" << std::endl;
        }
    }
};

/**
 * @brief Test assertion macros and utilities
 */
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            throw std::runtime_error("Assertion failed: " #condition " at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            throw std::runtime_error("Assertion failed: !(" #condition ") at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::stringstream ss; \
            ss << "Assertion failed: " #expected " == " #actual \
               << " (expected: " << (expected) << ", actual: " << (actual) << ") at " \
               << __FILE__ ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while(0)

#define ASSERT_NE(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            std::stringstream ss; \
            ss << "Assertion failed: " #expected " != " #actual \
               << " (both values: " << (expected) << ") at " \
               << __FILE__ ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while(0)

#define ASSERT_LT(left, right) \
    do { \
        if (!((left) < (right))) { \
            std::stringstream ss; \
            ss << "Assertion failed: " #left " < " #right \
               << " (" << (left) << " >= " << (right) << ") at " \
               << __FILE__ ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while(0)

#define ASSERT_LE(left, right) \
    do { \
        if (!((left) <= (right))) { \
            std::stringstream ss; \
            ss << "Assertion failed: " #left " <= " #right \
               << " (" << (left) << " > " << (right) << ") at " \
               << __FILE__ ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while(0)

#define ASSERT_GT(left, right) \
    do { \
        if (!((left) > (right))) { \
            std::stringstream ss; \
            ss << "Assertion failed: " #left " > " #right \
               << " (" << (left) << " <= " << (right) << ") at " \
               << __FILE__ ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while(0)

#define ASSERT_GE(left, right) \
    do { \
        if (!((left) >= (right))) { \
            std::stringstream ss; \
            ss << "Assertion failed: " #left " >= " #right \
               << " (" << (left) << " < " << (right) << ") at " \
               << __FILE__ ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while(0)

#define ASSERT_THROW(expression, exception_type) \
    do { \
        bool caught = false; \
        try { \
            (expression); \
        } catch (const exception_type&) { \
            caught = true; \
        } catch (...) { \
            throw std::runtime_error("Expected " #exception_type " but got different exception at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
        if (!caught) { \
            throw std::runtime_error("Expected " #exception_type " but no exception was thrown at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

#define ASSERT_NO_THROW(expression) \
    do { \
        try { \
            (expression); \
        } catch (...) { \
            throw std::runtime_error("Expected no exception but got one at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while(0)

/**
 * @brief Performance measurement utilities
 */
class PerformanceMeasurer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::string operation_name_;

public:
    explicit PerformanceMeasurer(const std::string& operation_name)
        : operation_name_(operation_name) {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    ~PerformanceMeasurer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_);
        std::cout << "Performance: " << operation_name_ << " took "
                  << duration.count() << " microseconds" << std::endl;
    }

    std::chrono::nanoseconds elapsed() const {
        auto current_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            current_time - start_time_);
    }
};

/**
 * @brief Memory usage tracker for leak detection
 */
class MemoryUsageTracker {
private:
    static std::atomic<size_t> allocated_bytes_;
    static std::atomic<size_t> allocation_count_;
    static std::mutex allocation_map_mutex_;
    static std::unordered_map<void*, size_t> allocation_map_;

public:
    static void recordAllocation(void* ptr, size_t size) {
        allocated_bytes_.fetch_add(size);
        allocation_count_.fetch_add(1);

        std::lock_guard<std::mutex> lock(allocation_map_mutex_);
        allocation_map_[ptr] = size;
    }

    static void recordDeallocation(void* ptr) {
        std::lock_guard<std::mutex> lock(allocation_map_mutex_);
        auto it = allocation_map_.find(ptr);
        if (it != allocation_map_.end()) {
            allocated_bytes_.fetch_sub(it->second);
            allocation_count_.fetch_sub(1);
            allocation_map_.erase(it);
        }
    }

    static size_t getCurrentAllocatedBytes() {
        return allocated_bytes_.load();
    }

    static size_t getCurrentAllocationCount() {
        return allocation_count_.load();
    }

    static LeakDetectionResult checkForLeaks() {
        std::lock_guard<std::mutex> lock(allocation_map_mutex_);
        LeakDetectionResult result;
        result.leaked_allocations = allocation_map_.size();
        result.leaked_bytes = allocated_bytes_.load();

        for (const auto& [ptr, size] : allocation_map_) {
            result.leak_locations.push_back(
                "Leaked " + std::to_string(size) + " bytes at " +
                std::to_string(reinterpret_cast<uintptr_t>(ptr)));
        }

        return result;
    }

    static void reset() {
        std::lock_guard<std::mutex> lock(allocation_map_mutex_);
        allocated_bytes_.store(0);
        allocation_count_.store(0);
        allocation_map_.clear();
    }
};

// Static member definitions - using inline to avoid multiple definition errors
inline std::atomic<size_t> MemoryUsageTracker::allocated_bytes_{0};
inline std::atomic<size_t> MemoryUsageTracker::allocation_count_{0};
inline std::mutex MemoryUsageTracker::allocation_map_mutex_;
inline std::unordered_map<void*, size_t> MemoryUsageTracker::allocation_map_;

/**
 * @brief Thread safety testing utilities
 */
class ThreadSafetyTester {
public:
    template <typename Func>
    static ThreadSafetyResult testConcurrentAccess(Func&& func, size_t thread_count,
                                                   size_t iterations_per_thread) {
        ThreadSafetyResult result;
        result.thread_count = thread_count;

        std::vector<std::thread> threads;
        std::atomic<size_t> race_conditions{0};
        std::atomic<size_t> deadlocks{0};

        auto start_time = std::chrono::high_resolution_clock::now();

        // Launch threads
        for (size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&func, iterations_per_thread, &race_conditions, &deadlocks]() {
                try {
                    for (size_t j = 0; j < iterations_per_thread; ++j) {
                        func();
                    }
                } catch (const std::exception&) {
                    race_conditions.fetch_add(1);
                } catch (...) {
                    deadlocks.fetch_add(1);
                }
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time);
        result.race_conditions_detected = race_conditions.load();
        result.deadlocks_detected = deadlocks.load();

        return result;
    }
};

} // namespace atom::memory::test

#endif // ATOM_MEMORY_TEST_FRAMEWORK_HPP
