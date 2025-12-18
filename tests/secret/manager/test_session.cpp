/*
 * test_session.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <thread>

#include "atom/secret/manager/session.hpp"

namespace atom::secret::test {

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SessionManagerTest, DefaultConstruction) {
    SessionManager session;
    EXPECT_FALSE(session.isUnlocked());
}

TEST_F(SessionManagerTest, UnlockAndLock) {
    SessionManager session;

    session.unlock();
    EXPECT_TRUE(session.isUnlocked());

    session.lock();
    EXPECT_FALSE(session.isUnlocked());
}

TEST_F(SessionManagerTest, RecordActivity) {
    SessionManager session;
    session.unlock();

    auto before = session.getLastActivity();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    session.recordActivity();
    auto after = session.getLastActivity();

    EXPECT_GT(after, before);
}

TEST_F(SessionManagerTest, SessionTimeout) {
    SessionConfig config;
    config.timeoutSeconds = 1;  // 1 second timeout

    SessionManager session(config);
    session.unlock();

    EXPECT_TRUE(session.isUnlocked());

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check if expired (may auto-lock)
    EXPECT_TRUE(session.isExpired());
}

TEST_F(SessionManagerTest, SessionNotExpired) {
    SessionConfig config;
    config.timeoutSeconds = 60;

    SessionManager session(config);
    session.unlock();

    EXPECT_FALSE(session.isExpired());
}

TEST_F(SessionManagerTest, ExtendSession) {
    SessionConfig config;
    config.timeoutSeconds = 2;

    SessionManager session(config);
    session.unlock();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    session.recordActivity();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_FALSE(session.isExpired());
}

TEST_F(SessionManagerTest, UpdateConfig) {
    SessionManager session;

    SessionConfig newConfig;
    newConfig.timeoutSeconds = 120;
    newConfig.autoLockOnIdle = true;

    session.updateConfig(newConfig);

    EXPECT_EQ(session.getConfig().timeoutSeconds, 120);
    EXPECT_TRUE(session.getConfig().autoLockOnIdle);
}

TEST_F(SessionManagerTest, LockCallback) {
    SessionManager session;

    bool callbackCalled = false;
    session.setLockCallback([&callbackCalled]() { callbackCalled = true; });

    session.unlock();
    session.lock();

    EXPECT_TRUE(callbackCalled);
}

TEST_F(SessionManagerTest, UnlockCallback) {
    SessionManager session;

    bool callbackCalled = false;
    session.setUnlockCallback([&callbackCalled]() { callbackCalled = true; });

    session.unlock();

    EXPECT_TRUE(callbackCalled);
}

TEST_F(SessionManagerTest, GetSessionDuration) {
    SessionManager session;
    session.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto duration = session.getSessionDuration();
    EXPECT_GE(duration.count(), 100);
}

TEST_F(SessionManagerTest, DefaultConfig) {
    auto config = SessionConfig::defaults();

    EXPECT_GT(config.timeoutSeconds, 0);
}

}  // namespace atom::secret::test
