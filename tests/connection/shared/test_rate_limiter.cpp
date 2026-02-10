/*
 * test_rate_limiter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-29

Description: Unit tests for RateLimiter component

**************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "atom/connection/shared/rate_limiter.hpp"

using namespace atom::connection;
using namespace std::chrono_literals;

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        limiter_ = std::make_unique<RateLimiter>(3, 10);  // 3 connections, 10 msgs/min
    }

    void TearDown() override { limiter_.reset(); }

    std::unique_ptr<RateLimiter> limiter_;
};

TEST_F(RateLimiterTest, AllowsConnectionsWithinLimit) {
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
}

TEST_F(RateLimiterTest, BlocksConnectionsOverLimit) {
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_FALSE(limiter_->canConnect("192.168.1.1"));  // 4th connection blocked
}

TEST_F(RateLimiterTest, DifferentIPsHaveSeparateLimits) {
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_FALSE(limiter_->canConnect("192.168.1.1"));

    // Different IP should still be allowed
    EXPECT_TRUE(limiter_->canConnect("192.168.1.2"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.2"));
}

TEST_F(RateLimiterTest, ReleaseConnectionAllowsNewConnection) {
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
    EXPECT_FALSE(limiter_->canConnect("192.168.1.1"));

    limiter_->releaseConnection("192.168.1.1");
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
}

TEST_F(RateLimiterTest, GetConnectionCount) {
    EXPECT_EQ(limiter_->getConnectionCount("192.168.1.1"), 0);

    limiter_->canConnect("192.168.1.1");
    EXPECT_EQ(limiter_->getConnectionCount("192.168.1.1"), 1);

    limiter_->canConnect("192.168.1.1");
    EXPECT_EQ(limiter_->getConnectionCount("192.168.1.1"), 2);

    limiter_->releaseConnection("192.168.1.1");
    EXPECT_EQ(limiter_->getConnectionCount("192.168.1.1"), 1);
}

TEST_F(RateLimiterTest, AllowsMessagesWithinLimit) {
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter_->canSendMessage("192.168.1.1"));
    }
}

TEST_F(RateLimiterTest, BlocksMessagesOverLimit) {
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(limiter_->canSendMessage("192.168.1.1"));
    }
    EXPECT_FALSE(limiter_->canSendMessage("192.168.1.1"));  // 11th message blocked
}

TEST_F(RateLimiterTest, ClearResetsAllLimits) {
    limiter_->canConnect("192.168.1.1");
    limiter_->canConnect("192.168.1.1");
    limiter_->canSendMessage("192.168.1.1");

    limiter_->clear();

    EXPECT_EQ(limiter_->getConnectionCount("192.168.1.1"), 0);
    EXPECT_TRUE(limiter_->canConnect("192.168.1.1"));
}

TEST_F(RateLimiterTest, ThreadSafety) {
    std::vector<std::thread> threads;
    std::atomic<int> successful_connections{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &successful_connections]() {
            if (limiter_->canConnect("192.168.1.1")) {
                successful_connections++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Only 3 should succeed
    EXPECT_EQ(successful_connections.load(), 3);
}
