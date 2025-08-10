#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <thread>
#include <type_traits>

#include "atom/async/limiter.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        limiter_ = std::make_unique<RateLimiter>();
    }

    void TearDown() override {
        limiter_.reset();
    }

    std::unique_ptr<RateLimiter> limiter_;
};

// Test basic settings and configuration
TEST_F(RateLimiterTest, BasicSettings) {
    std::string func_name = "test_func";
    
    // Test setting function limit
    EXPECT_NO_THROW(limiter_->setFunctionLimit(func_name, 2, 1s));
    
    // Test getting rejected requests (should be 0 initially)
    EXPECT_EQ(limiter_->getRejectedRequests(func_name), 0);
    
    // Test reset function
    EXPECT_NO_THROW(limiter_->resetFunction(func_name));
    
    // Test reset all
    EXPECT_NO_THROW(limiter_->resetAll());
}

// Test pause and resume functionality
TEST_F(RateLimiterTest, PauseResume) {
    // Test pause
    EXPECT_NO_THROW(limiter_->pause());
    
    // Test resume
    EXPECT_NO_THROW(limiter_->resume());
    
    // Test process waiters
    EXPECT_NO_THROW(limiter_->processWaiters());
}

// Test invalid parameters
TEST_F(RateLimiterTest, InvalidParameters) {
    std::string func_name = "test_func";
    
    // Invalid max_requests (0)
    EXPECT_THROW(limiter_->setFunctionLimit(func_name, 0, 1s), std::invalid_argument);
    
    // Invalid time_window (0s)
    EXPECT_THROW(limiter_->setFunctionLimit(func_name, 1, 0s), std::invalid_argument);
}

// Test batch function limits
TEST_F(RateLimiterTest, BatchFunctionLimits) {
    std::vector<std::pair<std::string_view, RateLimiter::Settings>> settings = {
        {"func_A", RateLimiter::Settings(1, 1s)},
        {"func_B", RateLimiter::Settings(2, 2s)},
        {"func_C", RateLimiter::Settings(3, 3s)}
    };
    
    EXPECT_NO_THROW(limiter_->setFunctionLimits(settings));
    
    // Test invalid settings in batch
    std::vector<std::pair<std::string_view, RateLimiter::Settings>> invalid_settings = {
        {"func_D", RateLimiter::Settings(1, 1s)},
        {"func_E", RateLimiter::Settings(0, 1s)}  // Invalid
    };
    EXPECT_THROW(limiter_->setFunctionLimits(invalid_settings), std::invalid_argument);
}

// Test Settings constructor
TEST_F(RateLimiterTest, SettingsConstructor) {
    // Valid settings
    EXPECT_NO_THROW(RateLimiter::Settings(5, 1s));
    EXPECT_NO_THROW(RateLimiter::Settings(10, 5s));
    
    // Invalid settings
    EXPECT_THROW(RateLimiter::Settings(0, 1s), std::invalid_argument);
    EXPECT_THROW(RateLimiter::Settings(5, 0s), std::invalid_argument);
}

// Test acquire awaiter creation (without actually using coroutines)
TEST_F(RateLimiterTest, AcquireAwaiter) {
    std::string func_name = "awaiter_func";
    limiter_->setFunctionLimit(func_name, 1, 1s);
    
    // Test creating awaiter
    auto awaiter = limiter_->acquire(func_name);
    EXPECT_FALSE(awaiter.await_ready());  // Should always return false
}

// Test batch acquire
TEST_F(RateLimiterTest, AcquireBatch) {
    std::vector<std::string> function_names = {"func1", "func2", "func3"};
    
    // Set limits for functions
    for (const auto& name : function_names) {
        limiter_->setFunctionLimit(name, 1, 1s);
    }
    
    // Test batch acquire
    auto awaiters = limiter_->acquireBatch(function_names);
    EXPECT_EQ(awaiters.size(), 3);
    
    for (auto& awaiter : awaiters) {
        EXPECT_FALSE(awaiter.await_ready());
    }
}

// Test RateLimitExceededException
TEST_F(RateLimiterTest, RateLimitException) {
    RateLimitExceededException ex("Test message");
    std::string what_str = ex.what();
    EXPECT_NE(what_str.find("Test message"), std::string::npos);
}

// Test singleton
TEST_F(RateLimiterTest, Singleton) {
    auto& instance1 = RateLimiterSingleton::instance();
    auto& instance2 = RateLimiterSingleton::instance();
    
    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);
    
    // Test basic functionality
    EXPECT_NO_THROW(instance1.setFunctionLimit("singleton_func", 1, 1s));
    EXPECT_EQ(instance1.getRejectedRequests("singleton_func"), 0);
}

// Test multiple function limits
TEST_F(RateLimiterTest, MultipleFunctions) {
    limiter_->setFunctionLimit("func1", 1, 1s);
    limiter_->setFunctionLimit("func2", 2, 2s);
    limiter_->setFunctionLimit("func3", 3, 3s);
    
    // All should start with 0 rejected requests
    EXPECT_EQ(limiter_->getRejectedRequests("func1"), 0);
    EXPECT_EQ(limiter_->getRejectedRequests("func2"), 0);
    EXPECT_EQ(limiter_->getRejectedRequests("func3"), 0);
    
    // Reset specific function
    limiter_->resetFunction("func1");
    EXPECT_EQ(limiter_->getRejectedRequests("func1"), 0);
    
    // Reset all functions
    limiter_->resetAll();
    EXPECT_EQ(limiter_->getRejectedRequests("func1"), 0);
    EXPECT_EQ(limiter_->getRejectedRequests("func2"), 0);
    EXPECT_EQ(limiter_->getRejectedRequests("func3"), 0);
}

// Test edge cases
TEST_F(RateLimiterTest, EdgeCases) {
    // Test with empty function name
    EXPECT_NO_THROW(limiter_->setFunctionLimit("", 1, 1s));
    EXPECT_EQ(limiter_->getRejectedRequests(""), 0);
    
    // Test with very large limits
    EXPECT_NO_THROW(limiter_->setFunctionLimit("large_func", 1000000, 3600s));
    
    // Test with very small time window
    EXPECT_NO_THROW(limiter_->setFunctionLimit("small_window", 1, 1s));
    
    // Test reset on non-existent function
    EXPECT_NO_THROW(limiter_->resetFunction("non_existent"));
    EXPECT_EQ(limiter_->getRejectedRequests("non_existent"), 0);
}

// Test concurrent access safety (basic)
TEST_F(RateLimiterTest, ConcurrentAccess) {
    const int num_threads = 4;
    const int operations_per_thread = 10;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string func_name = "thread_" + std::to_string(i) + "_func_" + std::to_string(j);
                limiter_->setFunctionLimit(func_name, 1, 1s);
                [[maybe_unused]] auto rejected = limiter_->getRejectedRequests(func_name);
                limiter_->resetFunction(func_name);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Test should complete without crashes
    EXPECT_TRUE(true);
}

// Test move semantics
TEST_F(RateLimiterTest, MoveSemantics) {
    RateLimiter limiter1;
    limiter1.setFunctionLimit("test_func", 5, 2s);
    
    // Move constructor
    RateLimiter limiter2 = std::move(limiter1);
    EXPECT_EQ(limiter2.getRejectedRequests("test_func"), 0);
    
    // Move assignment
    RateLimiter limiter3;
    limiter3 = std::move(limiter2);
    EXPECT_EQ(limiter3.getRejectedRequests("test_func"), 0);
}

// Test that copy operations are properly deleted
TEST_F(RateLimiterTest, CopyOperationsDeleted) {
    // This test verifies that copy operations are properly deleted
    // by checking that the class is not copyable
    EXPECT_FALSE(std::is_copy_constructible_v<RateLimiter>);
    EXPECT_FALSE(std::is_copy_assignable_v<RateLimiter>);

    // But move operations should be available
    EXPECT_TRUE(std::is_move_constructible_v<RateLimiter>);
    EXPECT_TRUE(std::is_move_assignable_v<RateLimiter>);
}
