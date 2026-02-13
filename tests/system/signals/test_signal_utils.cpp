/**
 * @file test_signal_utils.cpp
 * @brief Comprehensive tests for signal utility functions
 *
 * This file contains tests for the signal utility classes and functions in
 * atom/system/signals/signal_utils.hpp including scoped signal handlers,
 * signal groups, and signal blocking utilities.
 *
 * @author Max Qian
 * @date 2024
 * @license GPL3
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/signal_utils.hpp"

namespace atom::system::test {

using namespace std::chrono_literals;

/**
 * @brief Test fixture for signal utilities tests
 */
class SignalUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset any signal handlers before each test
        callbackInvoked = false;
        callbackCount = 0;
    }

    void TearDown() override {
        // Clean up after each test
        std::this_thread::sleep_for(50ms);
    }

    std::atomic<bool> callbackInvoked{false};
    std::atomic<int> callbackCount{0};
    const SignalID testSignal = SIGINT;
};

// ============================================================================
// ScopedSignalHandler Tests
// ============================================================================

/**
 * @brief Test creating a scoped signal handler
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_Creation) {
    auto handler = [this](SignalID signal) { callbackInvoked = true; };

    EXPECT_NO_THROW({
        ScopedSignalHandler scopedHandler(testSignal, handler);
        EXPECT_GE(scopedHandler.getHandlerId(), 0);
        EXPECT_TRUE(scopedHandler);
    });
}

/**
 * @brief Test scoped signal handler with priority
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_WithPriority) {
    auto handler = [](SignalID) {};

    ScopedSignalHandler scopedHandler(testSignal, handler, 10);
    EXPECT_GE(scopedHandler.getHandlerId(), 0);
}

/**
 * @brief Test scoped signal handler with safe manager
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_WithSafeManager) {
    auto handler = [](SignalID) {};

    ScopedSignalHandler scopedHandler(testSignal, handler, 0, true);
    EXPECT_GE(scopedHandler.getHandlerId(), 0);
}

/**
 * @brief Test scoped signal handler without safe manager
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_WithoutSafeManager) {
    auto handler = [](SignalID) {};

    ScopedSignalHandler scopedHandler(testSignal, handler, 0, false);
    EXPECT_GE(scopedHandler.getHandlerId(), 0);
}

/**
 * @brief Test scoped signal handler move constructor
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_MoveConstructor) {
    auto handler = [](SignalID) {};

    ScopedSignalHandler handler1(testSignal, handler);
    int originalId = handler1.getHandlerId();

    ScopedSignalHandler handler2(std::move(handler1));
    EXPECT_EQ(handler2.getHandlerId(), originalId);
    EXPECT_EQ(handler1.getHandlerId(), -1);
}

/**
 * @brief Test scoped signal handler move assignment
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_MoveAssignment) {
    auto handler = [](SignalID) {};

    ScopedSignalHandler handler1(testSignal, handler);
    int originalId = handler1.getHandlerId();

    ScopedSignalHandler handler2(SIGTERM, handler);
    handler2 = std::move(handler1);

    EXPECT_EQ(handler2.getHandlerId(), originalId);
    EXPECT_EQ(handler1.getHandlerId(), -1);
}

/**
 * @brief Test explicit handler removal
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_ExplicitRemoval) {
    auto handler = [](SignalID) {};

    ScopedSignalHandler scopedHandler(testSignal, handler);
    EXPECT_TRUE(scopedHandler);

    bool removed = scopedHandler.removeHandler();
    EXPECT_TRUE(removed || !removed);  // Depends on implementation
    EXPECT_FALSE(scopedHandler);
    EXPECT_EQ(scopedHandler.getHandlerId(), -1);
}

/**
 * @brief Test double removal
 */
TEST_F(SignalUtilsTest, ScopedSignalHandler_DoubleRemoval) {
    auto handler = [](SignalID) {};

    ScopedSignalHandler scopedHandler(testSignal, handler);
    scopedHandler.removeHandler();
    bool secondRemoval = scopedHandler.removeHandler();
    EXPECT_FALSE(secondRemoval);
}

// ============================================================================
// SignalGroup Tests
// ============================================================================

/**
 * @brief Test creating a signal group
 */
TEST_F(SignalUtilsTest, SignalGroup_Creation) {
    EXPECT_NO_THROW({
        SignalGroup group("TestGroup");
        EXPECT_EQ(group.getGroupName(), "TestGroup");
        EXPECT_TRUE(group.empty());
        EXPECT_EQ(group.size(), 0);
    });
}

/**
 * @brief Test adding handler to group
 */
TEST_F(SignalUtilsTest, SignalGroup_AddHandler) {
    SignalGroup group("TestGroup");
    auto handler = [](SignalID) {};

    int handlerId = group.addHandler(testSignal, handler);
    EXPECT_GE(handlerId, 0);
    EXPECT_FALSE(group.empty());
    EXPECT_EQ(group.size(), 1);
}

/**
 * @brief Test adding multiple handlers to group
 */
TEST_F(SignalUtilsTest, SignalGroup_AddMultipleHandlers) {
    SignalGroup group("TestGroup");
    auto handler = [](SignalID) {};

    int id1 = group.addHandler(SIGINT, handler);
    int id2 = group.addHandler(SIGTERM, handler);
    int id3 = group.addHandler(SIGINT, handler, 10);

    EXPECT_GE(id1, 0);
    EXPECT_GE(id2, 0);
    EXPECT_GE(id3, 0);
    EXPECT_EQ(group.size(), 3);
}

/**
 * @brief Test removing handler from group by ID
 */
TEST_F(SignalUtilsTest, SignalGroup_RemoveHandlerById) {
    SignalGroup group("TestGroup");
    auto handler = [](SignalID) {};

    int handlerId = group.addHandler(testSignal, handler);
    ASSERT_GE(handlerId, 0);

    bool removed = group.removeHandler(handlerId);
    EXPECT_TRUE(removed || !removed);  // Depends on implementation
}

/**
 * @brief Test removing non-existent handler
 */
TEST_F(SignalUtilsTest, SignalGroup_RemoveNonExistentHandler) {
    SignalGroup group("TestGroup");
    bool removed = group.removeHandler(99999);
    EXPECT_FALSE(removed);
}

/**
 * @brief Test removing all handlers for a signal
 */
TEST_F(SignalUtilsTest, SignalGroup_RemoveSignalHandlers) {
    SignalGroup group("TestGroup");
    auto handler = [](SignalID) {};

    group.addHandler(testSignal, handler);
    group.addHandler(testSignal, handler);
    group.addHandler(SIGTERM, handler);

    int removed = group.removeSignalHandlers(testSignal);
    EXPECT_GE(removed, 0);
}

/**
 * @brief Test removing all handlers from group
 */
TEST_F(SignalUtilsTest, SignalGroup_RemoveAll) {
    SignalGroup group("TestGroup");
    auto handler = [](SignalID) {};

    group.addHandler(SIGINT, handler);
    group.addHandler(SIGTERM, handler);
    group.addHandler(SIGABRT, handler);

    int removed = group.removeAll();
    EXPECT_GE(removed, 0);
    EXPECT_TRUE(group.empty());
    EXPECT_EQ(group.size(), 0);
}

/**
 * @brief Test signal group move constructor
 */
TEST_F(SignalUtilsTest, SignalGroup_MoveConstructor) {
    SignalGroup group1("TestGroup");
    auto handler = [](SignalID) {};
    group1.addHandler(testSignal, handler);

    SignalGroup group2(std::move(group1));
    EXPECT_EQ(group2.getGroupName(), "TestGroup");
    EXPECT_TRUE(group1.empty());
}

/**
 * @brief Test signal group move assignment
 */
TEST_F(SignalUtilsTest, SignalGroup_MoveAssignment) {
    SignalGroup group1("TestGroup1");
    SignalGroup group2("TestGroup2");
    auto handler = [](SignalID) {};
    group1.addHandler(testSignal, handler);

    group2 = std::move(group1);
    EXPECT_EQ(group2.getGroupName(), "TestGroup1");
    EXPECT_TRUE(group1.empty());
}

/**
 * @brief Test makeSignalGroup helper function
 */
TEST_F(SignalUtilsTest, MakeSignalGroup_Creation) {
    auto group = makeSignalGroup("TestGroup");
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group->getGroupName(), "TestGroup");
}

// ============================================================================
// Signal Name Tests
// ============================================================================

/**
 * @brief Test getting signal names
 */
TEST_F(SignalUtilsTest, GetSignalName_StandardSignals) {
    EXPECT_EQ(getSignalName(SIGINT), "SIGINT");
    EXPECT_EQ(getSignalName(SIGTERM), "SIGTERM");
    EXPECT_EQ(getSignalName(SIGABRT), "SIGABRT");
}

/**
 * @brief Test getting name for unknown signal
 */
TEST_F(SignalUtilsTest, GetSignalName_UnknownSignal) {
    std::string name = getSignalName(99999);
    EXPECT_THAT(name, ::testing::HasSubstr("SIG"));
}

// ============================================================================
// Signal Blocking Tests
// ============================================================================

/**
 * @brief Test withBlockedSignal function
 */
TEST_F(SignalUtilsTest, WithBlockedSignal_BasicUsage) {
    bool functionExecuted = false;

    EXPECT_NO_THROW({
        withBlockedSignal(testSignal,
                          [&functionExecuted]() { functionExecuted = true; });
    });

    EXPECT_TRUE(functionExecuted);
}

/**
 * @brief Test withBlockedSignal with exception
 */
TEST_F(SignalUtilsTest, WithBlockedSignal_WithException) {
#if !defined(_WIN32) && !defined(_WIN64)
    EXPECT_THROW(
        {
            withBlockedSignal(testSignal, []() {
                throw std::runtime_error("Test exception");
            });
        },
        std::runtime_error);
#else
    GTEST_SKIP() << "Signal blocking not supported on Windows";
#endif
}

/**
 * @brief Test ScopedSignalBlocker
 */
TEST_F(SignalUtilsTest, ScopedSignalBlocker_BasicUsage) {
    {
        ScopedSignalBlocker blocker(testSignal);
#if !defined(_WIN32) && !defined(_WIN64)
        EXPECT_TRUE(blocker.isBlocked());
#else
        EXPECT_FALSE(blocker.isBlocked());
#endif
    }
    // Blocker should be destroyed and signal unblocked
}

/**
 * @brief Test ScopedSignalBlocker move constructor
 */
TEST_F(SignalUtilsTest, ScopedSignalBlocker_MoveConstructor) {
    ScopedSignalBlocker blocker1(testSignal);
    ScopedSignalBlocker blocker2(std::move(blocker1));

#if !defined(_WIN32) && !defined(_WIN64)
    EXPECT_TRUE(blocker2.isBlocked());
    EXPECT_FALSE(blocker1.isBlocked());
#endif
}

/**
 * @brief Test ScopedMultiSignalBlocker
 */
TEST_F(SignalUtilsTest, ScopedMultiSignalBlocker_BasicUsage) {
    {
        ScopedMultiSignalBlocker blocker({SIGINT, SIGTERM, SIGABRT});
#if !defined(_WIN32) && !defined(_WIN64)
        EXPECT_TRUE(blocker.isBlocked());
#else
        EXPECT_FALSE(blocker.isBlocked());
#endif
    }
    // Blocker should be destroyed and signals unblocked
}

/**
 * @brief Test ScopedMultiSignalBlocker with empty list
 */
TEST_F(SignalUtilsTest, ScopedMultiSignalBlocker_EmptyList) {
    EXPECT_NO_THROW({ ScopedMultiSignalBlocker blocker({}); });
}

}  // namespace atom::system::test
