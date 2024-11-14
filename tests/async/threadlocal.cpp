#include "atom/async/threadlocal.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace atom::async;

TEST(ThreadLocalTest, DefaultConstructor) {
    ThreadLocal<int> threadLocal;
    EXPECT_FALSE(threadLocal.hasValue());
}

TEST(ThreadLocalTest, InitializerConstructor) {
    ThreadLocal<int> threadLocal([]() { return 42; });
    EXPECT_EQ(threadLocal.get(), 42);
}

TEST(ThreadLocalTest, GetAndSet) {
    ThreadLocal<int> threadLocal([]() { return 0; });
    threadLocal.get() = 10;
    EXPECT_EQ(threadLocal.get(), 10);
}

TEST(ThreadLocalTest, Reset) {
    ThreadLocal<int> threadLocal([]() { return 0; });
    threadLocal.reset(20);
    EXPECT_EQ(threadLocal.get(), 20);
}

TEST(ThreadLocalTest, HasValue) {
    ThreadLocal<int> threadLocal([]() { return 0; });
    EXPECT_FALSE(threadLocal.hasValue());
    threadLocal.get();
    EXPECT_TRUE(threadLocal.hasValue());
}

TEST(ThreadLocalTest, GetPointer) {
    ThreadLocal<int> threadLocal([]() { return 0; });
    EXPECT_EQ(threadLocal.getPointer(), nullptr);
    threadLocal.get();
    EXPECT_NE(threadLocal.getPointer(), nullptr);
}

TEST(ThreadLocalTest, ForEach) {
    ThreadLocal<int> threadLocal([]() { return 0; });
    threadLocal.get() = 10;

    std::thread t1([&threadLocal]() { threadLocal.get() = 20; });
    std::thread t2([&threadLocal]() { threadLocal.get() = 30; });

    t1.join();
    t2.join();

    std::vector<int> values;
    threadLocal.forEach([&values](int& value) { values.push_back(value); });

    EXPECT_EQ(values.size(), 3);
    EXPECT_NE(std::find(values.begin(), values.end(), 10), values.end());
    EXPECT_NE(std::find(values.begin(), values.end(), 20), values.end());
    EXPECT_NE(std::find(values.begin(), values.end(), 30), values.end());
}

TEST(ThreadLocalTest, Clear) {
    ThreadLocal<int> threadLocal([]() { return 0; });
    threadLocal.get() = 10;
    threadLocal.clear();
    EXPECT_FALSE(threadLocal.hasValue());
}
