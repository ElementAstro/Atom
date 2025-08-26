/*
 * test_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Test utilities and helper functions for Atom Async tests
Provides reusable test utilities, fixtures, and helper functions to support 
consistent testing patterns across all components.

**************************************************/

#ifndef ATOM_ASYNC_TEST_UTILS_HPP
#define ATOM_ASYNC_TEST_UTILS_HPP

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <future>
#include <functional>
#include <memory>
#include <random>
#include <sstream>

namespace atom::async::test {

// ============================================================================
// Timing Utilities
// ============================================================================

/**
 * @brief Helper class for measuring execution time
 */
class TimingHelper {
public:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::microseconds;
    
    TimingHelper() : start_(Clock::now()) {}
    
    Duration elapsed() const {
        return std::chrono::duration_cast<Duration>(Clock::now() - start_);
    }
    
    void reset() {
        start_ = Clock::now();
    }
    
    bool elapsedAtLeast(Duration duration) const {
        return elapsed() >= duration;
    }
    
    bool elapsedAtMost(Duration duration) const {
        return elapsed() <= duration;
    }
    
private:
    Clock::time_point start_;
};

/**
 * @brief RAII helper for timing test sections
 */
class ScopedTimer {
public:
    explicit ScopedTimer(std::string name) 
        : name_(std::move(name)), timer_() {}
    
    ~ScopedTimer() {
        auto elapsed = timer_.elapsed();
        std::cout << "[TIMING] " << name_ << ": " 
                  << elapsed.count() << " microseconds" << std::endl;
    }
    
private:
    std::string name_;
    TimingHelper timer_;
};

// ============================================================================
// Threading Utilities
// ============================================================================

/**
 * @brief Helper for managing multiple test threads
 */
class ThreadManager {
public:
    template<typename Func>
    void addThread(Func&& func) {
        threads_.emplace_back(std::forward<Func>(func));
    }
    
    void joinAll() {
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads_.clear();
    }
    
    size_t size() const {
        return threads_.size();
    }
    
    ~ThreadManager() {
        joinAll();
    }
    
private:
    std::vector<std::thread> threads_;
};

/**
 * @brief Barrier for synchronizing multiple threads
 */
class TestBarrier {
public:
    explicit TestBarrier(size_t count) : count_(count), waiting_(0) {}
    
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++waiting_;
        if (waiting_ == count_) {
            cv_.notify_all();
        } else {
            cv_.wait(lock, [this] { return waiting_ == count_; });
        }
    }
    
private:
    size_t count_;
    size_t waiting_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// ============================================================================
// Concurrency Testing Utilities
// ============================================================================

/**
 * @brief Helper for testing concurrent operations
 */
template<typename Func>
void runConcurrentTest(size_t numThreads, Func&& func) {
    ThreadManager manager;
    TestBarrier barrier(numThreads);
    
    for (size_t i = 0; i < numThreads; ++i) {
        manager.addThread([&barrier, func = std::forward<Func>(func), i]() {
            barrier.wait(); // Synchronize start
            func(i);
        });
    }
    
    manager.joinAll();
}

/**
 * @brief Helper for stress testing with random delays
 */
template<typename Func>
void runStressTest(size_t numThreads, size_t operationsPerThread, Func&& func) {
    ThreadManager manager;
    std::atomic<size_t> completedOperations{0};
    
    for (size_t i = 0; i < numThreads; ++i) {
        manager.addThread([&completedOperations, operationsPerThread, 
                          func = std::forward<Func>(func), i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 10);
            
            for (size_t j = 0; j < operationsPerThread; ++j) {
                func(i, j);
                completedOperations.fetch_add(1);
                
                // Random small delay to increase contention
                std::this_thread::sleep_for(std::chrono::microseconds(dis(gen)));
            }
        });
    }
    
    manager.joinAll();
    EXPECT_EQ(completedOperations.load(), numThreads * operationsPerThread);
}

// ============================================================================
// Exception Testing Utilities
// ============================================================================

/**
 * @brief Helper class for testing exception scenarios
 */
class ExceptionTester {
public:
    static std::runtime_error createTestException(const std::string& message = "Test exception") {
        return std::runtime_error(message);
    }
    
    template<typename Func>
    static void expectNoThrow(Func&& func, const std::string& context = "") {
        try {
            func();
        } catch (const std::exception& e) {
            FAIL() << "Unexpected exception in " << context << ": " << e.what();
        } catch (...) {
            FAIL() << "Unexpected unknown exception in " << context;
        }
    }
    
    template<typename ExceptionType, typename Func>
    static void expectThrow(Func&& func, const std::string& expectedMessage = "") {
        try {
            func();
            FAIL() << "Expected exception of type " << typeid(ExceptionType).name() 
                   << " but none was thrown";
        } catch (const ExceptionType& e) {
            if (!expectedMessage.empty()) {
                EXPECT_THAT(e.what(), ::testing::HasSubstr(expectedMessage));
            }
        } catch (const std::exception& e) {
            FAIL() << "Expected exception of type " << typeid(ExceptionType).name() 
                   << " but got " << typeid(e).name() << ": " << e.what();
        } catch (...) {
            FAIL() << "Expected exception of type " << typeid(ExceptionType).name() 
                   << " but got unknown exception";
        }
    }
};

// ============================================================================
// Memory and Resource Testing Utilities
// ============================================================================

/**
 * @brief Helper for testing resource cleanup
 */
class ResourceTracker {
public:
    ResourceTracker() : allocated_(0), deallocated_(0) {}
    
    void allocate() {
        allocated_.fetch_add(1);
    }
    
    void deallocate() {
        deallocated_.fetch_add(1);
    }
    
    size_t getAllocated() const {
        return allocated_.load();
    }
    
    size_t getDeallocated() const {
        return deallocated_.load();
    }
    
    size_t getLeaked() const {
        return allocated_.load() - deallocated_.load();
    }
    
    void expectNoLeaks() const {
        EXPECT_EQ(getLeaked(), 0) << "Resource leak detected: " 
                                  << getLeaked() << " resources not deallocated";
    }
    
private:
    std::atomic<size_t> allocated_;
    std::atomic<size_t> deallocated_;
};

/**
 * @brief RAII helper for tracking resource usage
 */
class ScopedResourceTracker {
public:
    explicit ScopedResourceTracker(ResourceTracker& tracker) 
        : tracker_(tracker) {
        tracker_.allocate();
    }
    
    ~ScopedResourceTracker() {
        tracker_.deallocate();
    }
    
private:
    ResourceTracker& tracker_;
};

// ============================================================================
// Test Data Generators
// ============================================================================

/**
 * @brief Helper for generating test data
 */
class TestDataGenerator {
public:
    static std::vector<int> generateIntegers(size_t count, int min = 0, int max = 1000) {
        std::vector<int> data;
        data.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);
        
        for (size_t i = 0; i < count; ++i) {
            data.push_back(dis(gen));
        }
        
        return data;
    }
    
    static std::vector<std::string> generateStrings(size_t count, size_t minLength = 5, size_t maxLength = 20) {
        std::vector<std::string> data;
        data.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> lengthDis(minLength, maxLength);
        std::uniform_int_distribution<> charDis('a', 'z');
        
        for (size_t i = 0; i < count; ++i) {
            size_t length = lengthDis(gen);
            std::string str;
            str.reserve(length);
            
            for (size_t j = 0; j < length; ++j) {
                str.push_back(static_cast<char>(charDis(gen)));
            }
            
            data.push_back(std::move(str));
        }
        
        return data;
    }
    
    template<typename T>
    static std::vector<T> generateSequence(size_t count, T start = T{}, T increment = T{1}) {
        std::vector<T> data;
        data.reserve(count);
        
        T current = start;
        for (size_t i = 0; i < count; ++i) {
            data.push_back(current);
            current += increment;
        }
        
        return data;
    }
};

// ============================================================================
// Assertion Helpers
// ============================================================================

/**
 * @brief Helper macros for common test assertions
 */
#define EXPECT_TIMING_RANGE(actual, min_duration, max_duration) \
    do { \
        auto actual_duration = (actual); \
        EXPECT_GE(actual_duration, (min_duration)) \
            << "Duration " << actual_duration.count() << " is less than minimum " \
            << (min_duration).count(); \
        EXPECT_LE(actual_duration, (max_duration)) \
            << "Duration " << actual_duration.count() << " is greater than maximum " \
            << (max_duration).count(); \
    } while(0)

#define EXPECT_EVENTUALLY_TRUE(condition, timeout) \
    do { \
        auto start = std::chrono::steady_clock::now(); \
        while (!(condition) && \
               std::chrono::steady_clock::now() - start < (timeout)) { \
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); \
        } \
        EXPECT_TRUE(condition) << "Condition did not become true within timeout"; \
    } while(0)

#define EXPECT_EVENTUALLY_FALSE(condition, timeout) \
    do { \
        auto start = std::chrono::steady_clock::now(); \
        while ((condition) && \
               std::chrono::steady_clock::now() - start < (timeout)) { \
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); \
        } \
        EXPECT_FALSE(condition) << "Condition did not become false within timeout"; \
    } while(0)

}  // namespace atom::async::test

#endif  // ATOM_ASYNC_TEST_UTILS_HPP
