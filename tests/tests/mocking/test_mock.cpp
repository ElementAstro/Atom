/*
 * test_mock.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for mock framework in atom/tests/mocking/test_mock.hpp

**************************************************/

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

#include "atom/tests/mocking/test_mock.hpp"

namespace atom::test::mocking::tests {

// ============================================================================
// MockFunction Basic Tests
// ============================================================================

class MockFunctionBasicTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockFunctionBasicTest, DefaultConstruction) {
    MockFunction<int(int)> mock;
    EXPECT_EQ(mock.callCount(), 0);
}

TEST_F(MockFunctionBasicTest, CallCountTracking) {
    MockFunction<void()> mock;
    mock.expect();

    mock();
    EXPECT_EQ(mock.callCount(), 1);

    mock();
    EXPECT_EQ(mock.callCount(), 2);
}

TEST_F(MockFunctionBasicTest, ReturnValue) {
    MockFunction<int()> mock;
    mock.expect().willReturn(42);

    EXPECT_EQ(mock(), 42);
}

TEST_F(MockFunctionBasicTest, MultipleReturnValues) {
    MockFunction<int()> mock;
    mock.expect().willReturn(1);
    mock.expect().willReturn(2);
    mock.expect().willReturn(3);

    EXPECT_EQ(mock(), 1);
    EXPECT_EQ(mock(), 2);
    EXPECT_EQ(mock(), 3);
}

// ============================================================================
// MockFunction with Arguments Tests
// ============================================================================

class MockFunctionArgsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockFunctionArgsTest, SingleArgument) {
    MockFunction<int(int)> mock;
    mock.expect().willReturn(10);

    EXPECT_EQ(mock(5), 10);
}

TEST_F(MockFunctionArgsTest, MultipleArguments) {
    MockFunction<int(int, int)> mock;
    mock.expect().willReturn(100);

    EXPECT_EQ(mock(10, 20), 100);
}

TEST_F(MockFunctionArgsTest, StringArgument) {
    MockFunction<size_t(const std::string&)> mock;
    mock.expect().willReturn(5);

    EXPECT_EQ(mock("hello"), 5);
}

// ============================================================================
// MockFunction with WillInvoke Tests
// ============================================================================

class MockFunctionInvokeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockFunctionInvokeTest, InvokeWithLambda) {
    MockFunction<int(int)> mock;
    mock.expect().willInvoke([](int x) { return x * 2; });

    EXPECT_EQ(mock(5), 10);
    EXPECT_EQ(mock(3), 6);
}

TEST_F(MockFunctionInvokeTest, InvokeWithCapture) {
    int multiplier = 3;
    MockFunction<int(int)> mock;
    mock.expect().willInvoke([multiplier](int x) { return x * multiplier; });

    EXPECT_EQ(mock(5), 15);
}

TEST_F(MockFunctionInvokeTest, InvokeVoidFunction) {
    std::string captured;
    MockFunction<void(std::string)> mock;
    mock.expect().willInvoke(
        [&captured](const std::string& s) { captured = s; });

    mock("test");
    EXPECT_EQ(captured, "test");
}

// ============================================================================
// MockFunction Expectation Tests
// ============================================================================

class MockFunctionExpectationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockFunctionExpectationTest, TimesExact) {
    MockFunction<void()> mock;
    mock.expect().times(3);

    mock();
    mock();
    mock();

    EXPECT_TRUE(mock.verify());
}

TEST_F(MockFunctionExpectationTest, AtLeast) {
    MockFunction<void()> mock;
    mock.expect().atLeast(2);

    mock();
    mock();
    mock();

    EXPECT_TRUE(mock.verify());
}

TEST_F(MockFunctionExpectationTest, AtMost) {
    MockFunction<void()> mock;
    mock.expect().atMost(5);

    mock();
    mock();

    EXPECT_TRUE(mock.verify());
}

TEST_F(MockFunctionExpectationTest, Between) {
    MockFunction<void()> mock;
    mock.expect().between(2, 5);

    mock();
    mock();
    mock();

    EXPECT_TRUE(mock.verify());
}

// ============================================================================
// MockFunction with Exceptions Tests
// ============================================================================

class MockFunctionExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockFunctionExceptionTest, ThrowException) {
    MockFunction<int()> mock;
    mock.expect().willThrow(std::runtime_error("Mock error"));

    EXPECT_THROW(mock(), std::runtime_error);
}

TEST_F(MockFunctionExceptionTest, ThrowSpecificException) {
    MockFunction<void(int)> mock;
    mock.expect().willThrow(std::invalid_argument("Invalid"));

    EXPECT_THROW(mock(42), std::invalid_argument);
}

// ============================================================================
// MockFunction Call History Tests
// ============================================================================

class MockFunctionHistoryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockFunctionHistoryTest, RecordsCallHistory) {
    MockFunction<void(int)> mock;
    mock.expect();

    mock(1);
    mock(2);
    mock(3);

    EXPECT_EQ(mock.callCount(), 3);
}

TEST_F(MockFunctionHistoryTest, WasCalledWith) {
    MockFunction<void(int)> mock;
    mock.expect();

    mock(10);
    mock(20);

    EXPECT_TRUE(mock.wasCalledWith(10));
    EXPECT_TRUE(mock.wasCalledWith(20));
    EXPECT_FALSE(mock.wasCalledWith(30));
}

TEST_F(MockFunctionHistoryTest, GetCallHistory) {
    MockFunction<void(int, std::string)> mock;
    mock.expect();

    mock(1, "first");
    mock(2, "second");

    const auto& history = mock.getCallHistory();
    EXPECT_EQ(history.size(), 2);
}

// ============================================================================
// MockFunction Reset Tests
// ============================================================================

class MockFunctionResetTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockFunctionResetTest, ResetCallCount) {
    MockFunction<void()> mock;
    mock.expect();

    mock();
    mock();
    EXPECT_EQ(mock.callCount(), 2);

    mock.reset();
    EXPECT_EQ(mock.callCount(), 0);
}

TEST_F(MockFunctionResetTest, ResetExpectations) {
    MockFunction<int()> mock;
    mock.expect().willReturn(42);

    EXPECT_EQ(mock(), 42);

    mock.reset();
    mock.expect().willReturn(100);

    EXPECT_EQ(mock(), 100);
}

// ============================================================================
// Spy Tests
// ============================================================================

class SpyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SpyTest, SpyCallsRealFunction) {
    auto realFunc = [](int x) { return x * x; };
    Spy<int(int)> spy(realFunc);

    EXPECT_EQ(spy(4), 16);
    EXPECT_EQ(spy(5), 25);
}

TEST_F(SpyTest, SpyRecordsCallCount) {
    auto realFunc = [](int x) { return x + 1; };
    Spy<int(int)> spy(realFunc);

    spy(1);
    spy(2);
    spy(3);

    EXPECT_EQ(spy.callCount(), 3);
}

TEST_F(SpyTest, SpyWasCalledWith) {
    auto realFunc = [](int x) { return x; };
    Spy<int(int)> spy(realFunc);

    spy(10);
    spy(20);

    EXPECT_TRUE(spy.wasCalledWith(10));
    EXPECT_TRUE(spy.wasCalledWith(20));
    EXPECT_FALSE(spy.wasCalledWith(30));
}

TEST_F(SpyTest, SpyReset) {
    auto realFunc = []() {};
    Spy<void()> spy(realFunc);

    spy();
    spy();
    EXPECT_EQ(spy.callCount(), 2);

    spy.reset();
    EXPECT_EQ(spy.callCount(), 0);
}

// ============================================================================
// Stub Tests
// ============================================================================

class StubTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StubTest, StubReturnsValue) {
    Stub<int()> stub(42);

    EXPECT_EQ(stub(), 42);
    EXPECT_EQ(stub(), 42);
}

TEST_F(StubTest, StubWithDifferentTypes) {
    Stub<std::string()> stub("hello");

    EXPECT_EQ(stub(), "hello");
}

// ============================================================================
// Fake Tests
// ============================================================================

class FakeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FakeTest, FakeImplementation) {
    Fake<int(int)> fake([](int x) { return x * 10; });

    EXPECT_EQ(fake(5), 50);
    EXPECT_EQ(fake(3), 30);
}

// ============================================================================
// MockFunction as std::function Tests
// ============================================================================

class MockAsFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockAsFunctionTest, ConvertToStdFunction) {
    MockFunction<int(int)> mock;
    mock.expect().willInvoke([](int x) { return x + 1; });

    std::function<int(int)> func = mock.asFunction();

    EXPECT_EQ(func(5), 6);
    EXPECT_EQ(mock.callCount(), 1);
}

TEST_F(MockAsFunctionTest, UseWithAlgorithm) {
    MockFunction<bool(int)> mock;
    mock.expect().willInvoke([](int x) { return x > 5; });

    std::vector<int> vec = {1, 3, 7, 9, 2};
    auto func = mock.asFunction();

    auto count = std::count_if(vec.begin(), vec.end(), func);
    EXPECT_EQ(count, 2);
}

// ============================================================================
// Mock Verification Tests
// ============================================================================

class MockVerificationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MockVerificationTest, VerifySuccess) {
    MockFunction<void()> mock;
    mock.expect().times(2);

    mock();
    mock();

    EXPECT_TRUE(mock.verify());
}

TEST_F(MockVerificationTest, VerifyFailure) {
    MockFunction<void()> mock;
    mock.expect().times(3);

    mock();
    mock();

    EXPECT_FALSE(mock.verify());
}

}  // namespace atom::test::mocking::tests
