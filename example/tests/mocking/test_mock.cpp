/**
 * @file test_mock.cpp
 * @brief Test Mock framework functionality in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

#include <functional>
#include <string>

using namespace atom::test;

// ============================================================================
// Basic Mock Function Tests
// ============================================================================

TEST(MockTests, BasicMockFunction) {
    MockFunction<int(int, int)> mockAdd;

    mockAdd.expect().willReturn(5);

    int result = mockAdd(2, 3);
    expect_eq(result, 5);
    expect_eq(mockAdd.callCount(), 1);
}

TEST(MockTests, MockWithMultipleCalls) {
    MockFunction<int()> mockFunc;

    mockFunc.expect().willReturn(1);
    mockFunc.expect().willReturn(2);
    mockFunc.expect().willReturn(3);

    expect_eq(mockFunc(), 1);
    expect_eq(mockFunc(), 2);
    expect_eq(mockFunc(), 3);
    expect_eq(mockFunc.callCount(), 3);
}

TEST(MockTests, MockWithInvoke) {
    MockFunction<int(int)> mockSquare;

    mockSquare.expect().willInvoke([](int x) { return x * x; });

    expect_eq(mockSquare(5), 25);
    expect_eq(mockSquare(3), 9);
}

TEST(MockTests, MockVoidFunction) {
    MockFunction<void(std::string)> mockLog;
    std::string lastMessage;

    mockLog.expect().willInvoke(
        [&lastMessage](const std::string& msg) { lastMessage = msg; });

    mockLog("Hello, World!");
    expect_eq(lastMessage, "Hello, World!");
}

// ============================================================================
// Mock Expectation Tests
// ============================================================================

TEST(MockTests, ExpectCallCount) {
    MockFunction<void()> mockFunc;

    mockFunc.expect().times(3);

    mockFunc();
    mockFunc();
    mockFunc();

    expect_true(mockFunc.verify());
}

TEST(MockTests, ExpectAtLeast) {
    MockFunction<void()> mockFunc;

    mockFunc.expect().atLeast(2);

    mockFunc();
    mockFunc();
    mockFunc();

    expect_true(mockFunc.verify());
}

TEST(MockTests, ExpectAtMost) {
    MockFunction<void()> mockFunc;

    mockFunc.expect().atMost(5);

    mockFunc();
    mockFunc();

    expect_true(mockFunc.verify());
}

// ============================================================================
// Mock with Exception Tests
// ============================================================================

TEST(MockTests, MockThrowsException) {
    MockFunction<int()> mockFunc;

    mockFunc.expect().willThrow(std::runtime_error("Mock error"));

    expect_throws([&mockFunc]() { mockFunc(); });
}

TEST(MockTests, MockThrowsSpecificException) {
    MockFunction<void(int)> mockFunc;

    mockFunc.expect().willThrow(std::invalid_argument("Invalid value"));

    expect_throws_with_message([&mockFunc]() { mockFunc(42); }, "Invalid");
}

// ============================================================================
// Spy Tests
// ============================================================================

TEST(SpyTests, BasicSpy) {
    auto realAdd = [](int a, int b) { return a + b; };
    Spy<int(int, int)> spyAdd(realAdd);

    int result = spyAdd(2, 3);

    expect_eq(result, 5);
    expect_eq(spyAdd.callCount(), 1);
}

TEST(SpyTests, SpyRecordsAllCalls) {
    auto realFunc = [](int x) { return x * 2; };
    Spy<int(int)> spy(realFunc);

    spy(1);
    spy(2);
    spy(3);

    expect_eq(spy.callCount(), 3);
    expect_true(spy.wasCalledWith(1));
    expect_true(spy.wasCalledWith(2));
    expect_true(spy.wasCalledWith(3));
}

TEST(SpyTests, SpyWithStringFunction) {
    auto realFunc = [](const std::string& s) { return s.length(); };
    Spy<size_t(const std::string&)> spy(realFunc);

    expect_eq(spy("hello"), 5);
    expect_eq(spy("world"), 5);
    expect_eq(spy.callCount(), 2);
}

// ============================================================================
// Call History Tests
// ============================================================================

TEST(MockTests, CallHistoryTracking) {
    MockFunction<void(int, std::string)> mockFunc;

    mockFunc.expect();

    mockFunc(1, "first");
    mockFunc(2, "second");
    mockFunc(3, "third");

    const auto& history = mockFunc.getCallHistory();
    expect_eq(history.size(), 3);
}

TEST(MockTests, WasCalledWith) {
    MockFunction<void(int)> mockFunc;

    mockFunc.expect();

    mockFunc(10);
    mockFunc(20);
    mockFunc(30);

    expect_true(mockFunc.wasCalledWith(10));
    expect_true(mockFunc.wasCalledWith(20));
    expect_true(mockFunc.wasCalledWith(30));
    expect_false(mockFunc.wasCalledWith(40));
}

// ============================================================================
// Mock Reset Tests
// ============================================================================

TEST(MockTests, ResetMock) {
    MockFunction<int()> mockFunc;

    mockFunc.expect().willReturn(42);
    mockFunc();

    expect_eq(mockFunc.callCount(), 1);

    mockFunc.reset();

    expect_eq(mockFunc.callCount(), 0);
}

// ============================================================================
// Mock as std::function Tests
// ============================================================================

TEST(MockTests, MockAsStdFunction) {
    MockFunction<int(int)> mockFunc;
    mockFunc.expect().willInvoke([](int x) { return x + 1; });

    std::function<int(int)> func = mockFunc.asFunction();

    expect_eq(func(5), 6);
    expect_eq(mockFunc.callCount(), 1);
}

// ============================================================================
// Complex Mock Scenarios
// ============================================================================

class Calculator {
public:
    using AddFunc = std::function<int(int, int)>;
    using MultiplyFunc = std::function<int(int, int)>;

    Calculator(AddFunc add, MultiplyFunc multiply)
        : add_(std::move(add)), multiply_(std::move(multiply)) {}

    int compute(int a, int b, int c) {
        return multiply_(add_(a, b), c);
    }

private:
    AddFunc add_;
    MultiplyFunc multiply_;
};

TEST(MockTests, MockDependencyInjection) {
    MockFunction<int(int, int)> mockAdd;
    MockFunction<int(int, int)> mockMultiply;

    mockAdd.expect().willInvoke([](int a, int b) { return a + b; });
    mockMultiply.expect().willInvoke([](int a, int b) { return a * b; });

    Calculator calc(mockAdd.asFunction(), mockMultiply.asFunction());

    int result = calc.compute(2, 3, 4);  // (2 + 3) * 4 = 20

    expect_eq(result, 20);
    expect_eq(mockAdd.callCount(), 1);
    expect_eq(mockMultiply.callCount(), 1);
}

// ============================================================================
// Mock Verification Macro Test
// ============================================================================

TEST(MockTests, MockVerificationMacro) {
    MockFunction<void()> mockFunc;

    mockFunc.expect().times(2);

    mockFunc();
    mockFunc();

    expect_mock_verified(mockFunc);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    return runAllTests(argc, argv);
}
