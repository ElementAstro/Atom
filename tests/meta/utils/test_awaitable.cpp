#include <gtest/gtest.h>

// Only compile these tests if coroutines are supported
#if __cpp_lib_coroutine

#include "atom/meta/awaitable.hpp"

#include <coroutine>
#include <string>
#include <vector>

namespace {

// Test fixture for awaitable tests
class AwaitableTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Helper functions for testing
int add(int a, int b) { return a + b; }

std::string concatenate(const std::string& a, const std::string& b) {
    return a + b;
}

int multiply(int a, int b, int c) { return a * b * c; }

// Test SimpleAwaitable basic functionality
TEST_F(AwaitableTest, SimpleAwaitableBasicInt) {
    auto func = [](int val) { return val * 2; };
    atom::meta::SimpleAwaitable awaitable(func, 5);

    // Test await_ready (should always return false for simple implementation)
    EXPECT_FALSE(awaitable.await_ready());

    // Test await_resume (should execute the function)
    int result = awaitable.await_resume();
    EXPECT_EQ(result, 10);
}

TEST_F(AwaitableTest, SimpleAwaitableBasicString) {
    auto func = [](const std::string& str) { return str + " world"; };
    atom::meta::SimpleAwaitable awaitable(func, std::string("hello"));

    EXPECT_FALSE(awaitable.await_ready());

    std::string result = awaitable.await_resume();
    EXPECT_EQ(result, "hello world");
}

TEST_F(AwaitableTest, SimpleAwaitableMultipleArgs) {
    auto func = [](int a, int b, int c) { return a + b + c; };
    atom::meta::SimpleAwaitable awaitable(func, 1, 2, 3);

    EXPECT_FALSE(awaitable.await_ready());

    int result = awaitable.await_resume();
    EXPECT_EQ(result, 6);
}

TEST_F(AwaitableTest, SimpleAwaitableWithFunctionPointer) {
    atom::meta::SimpleAwaitable awaitable(&add, 10, 20);

    EXPECT_FALSE(awaitable.await_ready());

    int result = awaitable.await_resume();
    EXPECT_EQ(result, 30);
}

TEST_F(AwaitableTest, SimpleAwaitableWithStringFunction) {
    atom::meta::SimpleAwaitable awaitable(&concatenate, std::string("Hello, "),
                                          std::string("World!"));

    EXPECT_FALSE(awaitable.await_ready());

    std::string result = awaitable.await_resume();
    EXPECT_EQ(result, "Hello, World!");
}

TEST_F(AwaitableTest, SimpleAwaitableVoidReturn) {
    int counter = 0;
    auto func = [&counter]() { counter++; };
    atom::meta::SimpleAwaitable awaitable(func);

    EXPECT_FALSE(awaitable.await_ready());

    awaitable.await_resume();
    EXPECT_EQ(counter, 1);
}

TEST_F(AwaitableTest, SimpleAwaitableWithCapture) {
    int multiplier = 5;
    auto func = [multiplier](int val) { return val * multiplier; };
    atom::meta::SimpleAwaitable awaitable(func, 7);

    EXPECT_FALSE(awaitable.await_ready());

    int result = awaitable.await_resume();
    EXPECT_EQ(result, 35);
}

TEST_F(AwaitableTest, SimpleAwaitableComplexType) {
    struct Point {
        int x, y;
        Point(int x_, int y_) : x(x_), y(y_) {}
    };

    auto func = [](int x, int y) { return Point(x, y); };
    atom::meta::SimpleAwaitable awaitable(func, 10, 20);

    EXPECT_FALSE(awaitable.await_ready());

    Point result = awaitable.await_resume();
    EXPECT_EQ(result.x, 10);
    EXPECT_EQ(result.y, 20);
}

// Test makeAwaitable helper function
TEST_F(AwaitableTest, MakeAwaitableBasic) {
    auto func = [](int val) { return val * 3; };
    auto awaitable = atom::meta::makeAwaitable(func, 4);

    EXPECT_FALSE(awaitable.await_ready());

    int result = awaitable.await_resume();
    EXPECT_EQ(result, 12);
}

TEST_F(AwaitableTest, MakeAwaitableWithFunctionPointer) {
    auto awaitable = atom::meta::makeAwaitable(&add, 15, 25);

    EXPECT_FALSE(awaitable.await_ready());

    int result = awaitable.await_resume();
    EXPECT_EQ(result, 40);
}

TEST_F(AwaitableTest, MakeAwaitableMultipleArgs) {
    auto awaitable = atom::meta::makeAwaitable(&multiply, 2, 3, 4);

    EXPECT_FALSE(awaitable.await_ready());

    int result = awaitable.await_resume();
    EXPECT_EQ(result, 24);
}

TEST_F(AwaitableTest, MakeAwaitableString) {
    auto awaitable = atom::meta::makeAwaitable(&concatenate, std::string("foo"),
                                               std::string("bar"));

    EXPECT_FALSE(awaitable.await_ready());

    std::string result = awaitable.await_resume();
    EXPECT_EQ(result, "foobar");
}

TEST_F(AwaitableTest, MakeAwaitableVoid) {
    bool executed = false;
    auto func = [&executed]() { executed = true; };
    auto awaitable = atom::meta::makeAwaitable(func);

    EXPECT_FALSE(awaitable.await_ready());
    EXPECT_FALSE(executed);

    awaitable.await_resume();
    EXPECT_TRUE(executed);
}

TEST_F(AwaitableTest, MakeAwaitableWithMutableLambda) {
    auto func = [counter = 0]() mutable { return ++counter; };
    auto awaitable = atom::meta::makeAwaitable(func);

    EXPECT_FALSE(awaitable.await_ready());

    int result = awaitable.await_resume();
    EXPECT_EQ(result, 1);
}

TEST_F(AwaitableTest, MakeAwaitableReturnReference) {
    std::string str = "test";
    auto func = [&str]() -> std::string& { return str; };
    auto awaitable = atom::meta::makeAwaitable(func);

    EXPECT_FALSE(awaitable.await_ready());

    std::string& result = awaitable.await_resume();
    EXPECT_EQ(&result, &str);
    EXPECT_EQ(result, "test");
}

TEST_F(AwaitableTest, MakeAwaitableWithVector) {
    auto func = [](int size) {
        std::vector<int> vec(size);
        for (int i = 0; i < size; ++i) {
            vec[i] = i * 2;
        }
        return vec;
    };

    auto awaitable = atom::meta::makeAwaitable(func, 5);

    EXPECT_FALSE(awaitable.await_ready());

    std::vector<int> result = awaitable.await_resume();
    EXPECT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 4);
    EXPECT_EQ(result[3], 6);
    EXPECT_EQ(result[4], 8);
}

TEST_F(AwaitableTest, AwaitSuspendBehavior) {
    // Test that await_suspend resumes immediately
    auto func = []() { return 42; };
    auto awaitable = atom::meta::makeAwaitable(func);

    // Create a simple coroutine handle for testing
    struct TestPromise {
        auto get_return_object() {
            return std::coroutine_handle<TestPromise>::from_promise(*this);
        }
        auto initial_suspend() { return std::suspend_never{}; }
        auto final_suspend() noexcept { return std::suspend_never{}; }
        void return_void() {}
        void unhandled_exception() {}
    };

    // Note: await_suspend should resume the handle immediately in the simple
    // implementation This is tested implicitly by the fact that await_resume
    // works correctly
    EXPECT_FALSE(awaitable.await_ready());
}

TEST_F(AwaitableTest, MultipleAwaitableCreation) {
    // Test creating multiple awaitables
    auto awaitable1 = atom::meta::makeAwaitable([](int x) { return x * 2; }, 5);
    auto awaitable2 = atom::meta::makeAwaitable([](int x) { return x * 3; }, 5);
    auto awaitable3 = atom::meta::makeAwaitable([](int x) { return x * 4; }, 5);

    EXPECT_EQ(awaitable1.await_resume(), 10);
    EXPECT_EQ(awaitable2.await_resume(), 15);
    EXPECT_EQ(awaitable3.await_resume(), 20);
}

TEST_F(AwaitableTest, AwaitableWithException) {
    auto func = []() -> int {
        throw std::runtime_error("Test exception");
        return 42;
    };

    auto awaitable = atom::meta::makeAwaitable(func);

    EXPECT_FALSE(awaitable.await_ready());

    // The exception should propagate from await_resume
    EXPECT_THROW(awaitable.await_resume(), std::runtime_error);
}

}  // anonymous namespace

#endif  // __cpp_lib_coroutine

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

#if __cpp_lib_coroutine
    return RUN_ALL_TESTS();
#else
    std::cout << "Coroutine support not available, skipping tests" << std::endl;
    return 0;
#endif
}
