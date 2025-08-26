#pragma once

// Test configuration header for Atom Components test suite
// This file defines compile-time configuration for tests

// ============================================================================
// Feature Flags
// ============================================================================

// Enable/disable specific test suites based on available features
#ifndef ATOM_ENABLE_LUA
#define ATOM_ENABLE_LUA 0
#endif

#ifndef ATOM_ENABLE_PYTHON
#define ATOM_ENABLE_PYTHON 0
#endif

#ifndef ATOM_ENABLE_CHAISCRIPT
#define ATOM_ENABLE_CHAISCRIPT 0
#endif

#ifndef ATOM_ENABLE_SIMD
#define ATOM_ENABLE_SIMD 1
#endif

#ifndef ATOM_ENABLE_THREADING
#define ATOM_ENABLE_THREADING 1
#endif

// ============================================================================
// Test Configuration Constants
// ============================================================================

namespace atom::test {

// Performance test configuration
constexpr int PERFORMANCE_TEST_ITERATIONS = 1000;
constexpr int LARGE_DATASET_SIZE = 10000;
constexpr int THREAD_SAFETY_THREAD_COUNT = 4;
constexpr int STRESS_TEST_DURATION_MS = 5000;

// Memory test configuration
constexpr size_t TEST_MEMORY_LIMIT = 1024 * 1024; // 1MB
constexpr size_t LARGE_ALLOCATION_SIZE = 1024 * 1024 * 10; // 10MB

// Timeout configuration
constexpr int DEFAULT_TEST_TIMEOUT_MS = 5000;
constexpr int LONG_RUNNING_TEST_TIMEOUT_MS = 30000;
constexpr int PERFORMANCE_TEST_TIMEOUT_MS = 60000;

// Precision for floating point comparisons
constexpr double FLOAT_PRECISION = 1e-6;
constexpr double DOUBLE_PRECISION = 1e-12;

// String constants for testing
constexpr const char* TEST_COMPONENT_NAME = "TestComponent";
constexpr const char* TEST_SCRIPT_CONTENT = "return 42";
constexpr const char* INVALID_SCRIPT_CONTENT = "invalid syntax !!!";

// ============================================================================
// Test Helper Macros
// ============================================================================

// Conditional test execution based on feature availability
#define ATOM_TEST_SKIP_IF_NO_LUA() \
    do { \
        if (!ATOM_ENABLE_LUA) { \
            GTEST_SKIP() << "Lua engine is not enabled in this build"; \
        } \
    } while(0)

#define ATOM_TEST_SKIP_IF_NO_PYTHON() \
    do { \
        if (!ATOM_ENABLE_PYTHON) { \
            GTEST_SKIP() << "Python engine is not enabled in this build"; \
        } \
    } while(0)

#define ATOM_TEST_SKIP_IF_NO_SIMD() \
    do { \
        if (!ATOM_ENABLE_SIMD) { \
            GTEST_SKIP() << "SIMD operations are not enabled in this build"; \
        } \
    } while(0)

#define ATOM_TEST_SKIP_IF_NO_THREADING() \
    do { \
        if (!ATOM_ENABLE_THREADING) { \
            GTEST_SKIP() << "Threading is not enabled in this build"; \
        } \
    } while(0)

// Performance test helpers
#define ATOM_PERFORMANCE_TEST(test_name) \
    TEST(PerformanceTest, test_name)

#define ATOM_BENCHMARK_START() \
    auto benchmark_start = std::chrono::high_resolution_clock::now()

#define ATOM_BENCHMARK_END_AND_CHECK(max_duration_ms) \
    do { \
        auto benchmark_end = std::chrono::high_resolution_clock::now(); \
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(benchmark_end - benchmark_start); \
        EXPECT_LT(duration.count(), max_duration_ms) << "Performance test exceeded expected duration"; \
    } while(0)

// Memory test helpers
#define ATOM_EXPECT_MEMORY_USAGE_BELOW(max_bytes) \
    do { \
        /* Implementation would depend on available memory profiling tools */ \
        /* This is a placeholder for memory usage checking */ \
    } while(0)

// Thread safety test helpers
#define ATOM_THREAD_SAFETY_TEST(test_name, thread_count, iterations) \
    TEST(ThreadSafetyTest, test_name) { \
        ATOM_TEST_SKIP_IF_NO_THREADING(); \
        std::vector<std::thread> threads; \
        std::atomic<int> success_count{0}; \
        for (int t = 0; t < thread_count; ++t) { \
            threads.emplace_back([&success_count, iterations]() { \
                for (int i = 0; i < iterations; ++i) {

#define ATOM_THREAD_SAFETY_TEST_END() \
                } \
            }); \
        } \
        for (auto& thread : threads) { \
            thread.join(); \
        } \
    }

// ============================================================================
// Test Data Generators
// ============================================================================

// Generate test data for various scenarios
template<typename T>
std::vector<T> generateTestData(size_t count);

// Specializations for common types
template<>
inline std::vector<int> generateTestData<int>(size_t count) {
    std::vector<int> data;
    data.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        data.push_back(static_cast<int>(i));
    }
    return data;
}

template<>
inline std::vector<std::string> generateTestData<std::string>(size_t count) {
    std::vector<std::string> data;
    data.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        data.push_back("TestString" + std::to_string(i));
    }
    return data;
}

// ============================================================================
// Test Environment Configuration
// ============================================================================

class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Global test setup
        setupLogging();
        setupMemoryTracking();
        setupPerformanceCounters();
    }
    
    void TearDown() override {
        // Global test cleanup
        cleanupPerformanceCounters();
        cleanupMemoryTracking();
        cleanupLogging();
    }
    
private:
    void setupLogging() {
        // Configure logging for tests
        // Implementation depends on logging framework
    }
    
    void cleanupLogging() {
        // Cleanup logging resources
    }
    
    void setupMemoryTracking() {
        // Setup memory leak detection
        // Implementation depends on memory tracking tools
    }
    
    void cleanupMemoryTracking() {
        // Report memory leaks if any
    }
    
    void setupPerformanceCounters() {
        // Initialize performance monitoring
    }
    
    void cleanupPerformanceCounters() {
        // Report performance statistics
    }
};

// ============================================================================
// Test Utilities
// ============================================================================

namespace test_utils {

// Timeout wrapper for long-running tests
template<typename Func>
bool runWithTimeout(Func&& func, std::chrono::milliseconds timeout) {
    std::atomic<bool> completed{false};
    std::thread worker([&]() {
        func();
        completed = true;
    });
    
    std::this_thread::sleep_for(timeout);
    
    if (completed) {
        worker.join();
        return true;
    } else {
        worker.detach(); // Let it finish in background
        return false;
    }
}

// Random data generation
class RandomDataGenerator {
public:
    RandomDataGenerator() : rng_(std::random_device{}()) {}
    
    int randomInt(int min = 0, int max = 100) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng_);
    }
    
    double randomDouble(double min = 0.0, double max = 1.0) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng_);
    }
    
    std::string randomString(size_t length = 10) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::string result;
        result.reserve(length);
        
        std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dist(rng_)];
        }
        return result;
    }
    
private:
    std::mt19937 rng_;
};

// Singleton access to random generator
inline RandomDataGenerator& getRandom() {
    static RandomDataGenerator instance;
    return instance;
}

} // namespace test_utils

} // namespace atom::test

// ============================================================================
// Global Test Setup
// ============================================================================

// Register test environment
inline void setupTestEnvironment() {
    ::testing::AddGlobalTestEnvironment(new atom::test::TestEnvironment);
}

// Automatic test environment setup
namespace {
    struct TestEnvironmentSetup {
        TestEnvironmentSetup() {
            setupTestEnvironment();
        }
    };
    static TestEnvironmentSetup test_env_setup;
}
