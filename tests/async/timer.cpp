#include "atom/async/timer.hpp"
#include <gtest/gtest.h>

TEST(TimerTest, setTimeout) {
    atom::async::Timer timer;
    bool funcCalled = false;

    auto future = timer.setTimeout([&funcCalled]() { funcCalled = true; }, 100);

    // Wait for the task to complete
    future.wait();

    EXPECT_TRUE(funcCalled);
}

TEST(TimerTest, setInterval) {
    atom::async::Timer timer;
    std::atomic<int> funcCalls{0};

    timer.setInterval([&funcCalls]() { funcCalls.fetch_add(1); }, 50, 5, 0);

    // Wait for all executions to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    EXPECT_EQ(funcCalls.load(), 5);
}

TEST(TimerTest, cancelAllTasks) {
    atom::async::Timer timer;
    bool funcCalled = false;

    timer.setTimeout([&funcCalled]() { funcCalled = true; }, 100);
    timer.cancelAllTasks();

    // Wait for the task to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_FALSE(funcCalled);
}

TEST(TimerTest, pause) {
    atom::async::Timer timer;
    bool funcCalled = false;

    timer.setTimeout([&funcCalled]() { funcCalled = true; }, 100);
    timer.pause();

    // Wait for the task to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_FALSE(funcCalled);
}

TEST(TimerTest, resume) {
    atom::async::Timer timer;
    bool funcCalled = false;

    timer.setTimeout([&funcCalled]() { funcCalled = true; }, 100);
    timer.pause();
    timer.resume();

    // Wait for the task to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(funcCalled);
}

TEST(TimerTest, stop) {
    atom::async::Timer timer;
    bool funcCalled = false;

    timer.setTimeout([&funcCalled]() { funcCalled = true; }, 100);
    timer.stop();

    // Wait for the task to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_FALSE(funcCalled);
}

TEST(TimerTest, setCallback) {
    atom::async::Timer timer;
    bool callbackCalled = false;

    timer.setCallback([&callbackCalled]() { callbackCalled = true; });

    // Add a task to trigger the callback when it completes
    auto future = timer.setTimeout([]() {}, 50);

    // Wait for the task to complete, which should trigger the callback
    future.wait();

    EXPECT_TRUE(callbackCalled);
}

TEST(TimerTest, getTaskCount) {
    atom::async::Timer timer;

    EXPECT_EQ(timer.getTaskCount(), 0);

    auto future1 = timer.setTimeout([]() {}, 100);
    EXPECT_EQ(timer.getTaskCount(), 1);

    timer.setInterval([]() {}, 100, 5, 0);
    EXPECT_EQ(timer.getTaskCount(), 2);
}
