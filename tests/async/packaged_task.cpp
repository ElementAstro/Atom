#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>       // For std::abs
#include <functional>  // For std::function
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Include the header under test
#include "atom/async/packaged_task.hpp"
#include "atom/error/exception.hpp"  // For checking exception types

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

// Use the namespace
using namespace atom::async;

// Test fixture for PackagedTask
class PackagedTaskTest : public ::testing::Test {
protected:
    // No specific setup/teardown needed for most tests
};

// --- PackagedTask (non-void) Tests ---

// Test construction with a valid task
TEST_F(PackagedTaskTest, ConstructorValidTask) {
    auto task_func = [](int a, int b) { return a + b; };
    PackagedTask<int, int, int> task(task_func);
    EXPECT_TRUE(task);  // operator bool()
}

// Test construction with an invalid task
TEST_F(PackagedTaskTest, ConstructorInvalidTask) {
    std::function<int(int)> invalid_func = nullptr;
    EXPECT_THROW(PackagedTask<int, int> task(invalid_func),
                 InvalidPackagedTaskException);
}

// Test getEnhancedFuture
TEST_F(PackagedTaskTest, GetEnhancedFuture) {
    auto task_func = []() { return 123; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();
    EXPECT_TRUE(future.valid());
}

// Test operator() execution - success
TEST_F(PackagedTaskTest, OperatorCallSuccess) {
    auto task_func = [](int x) { return x * 2; };
    PackagedTask<int, int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    task(5);  // Execute the task

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_EQ(future.get(), 10);

    // Calling again should do nothing
    PackagedTask<int, int> task2([](int x) { return x * 2; });
    EnhancedFuture<int> future2 = task2.getEnhancedFuture();
    task2(5);
    task2(10);  // This call should be ignored
    EXPECT_TRUE(future2.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_EQ(future2.get(), 10);  // Result should be from the first call
}

// Test operator() execution - exception
TEST_F(PackagedTaskTest, OperatorCallException) {
    auto task_func = [](int x) -> int {
        if (x > 0)
            throw std::runtime_error("Test error");
        return 0;
    };
    PackagedTask<int, int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    task(1);  // Execute the task, should throw

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_THROW(future.get(), std::runtime_error);
}

// Test onComplete - registered before execution
TEST_F(PackagedTaskTest, OnCompleteBeforeExecution) {
    auto task_func = []() { return 100; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    std::atomic<int> callback_result = 0;
    std::atomic<bool> callback_called = false;

    task.onComplete([&](std::shared_future<int>& fut) {
        callback_result.store(fut.get());
        callback_called.store(true);
    });

    EXPECT_FALSE(callback_called.load());  // Callback should not have run yet

    task();  // Execute the task

    // Wait for the task and callback to complete
    future.wait();
    // Give a moment for the continuation to run if it was posted asynchronously
    // (not the case here by default)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(callback_called.load());
    EXPECT_EQ(callback_result.load(), 100);
}

// Test onComplete - registered after execution
TEST_F(PackagedTaskTest, OnCompleteAfterExecution) {
    auto task_func = []() { return 200; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    task();  // Execute the task first

    // Wait for task to complete
    future.wait();

    std::atomic<int> callback_result = 0;
    std::atomic<bool> callback_called = false;

    task.onComplete([&](std::shared_future<int>& fut) {
        callback_result.store(fut.get());
        callback_called.store(true);
    });

    // Callback should run immediately because the task is already completed
    // Give a moment just in case
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(callback_called.load());
    EXPECT_EQ(callback_result.load(), 200);
}

// Test onComplete - multiple callbacks
TEST_F(PackagedTaskTest, OnCompleteMultipleCallbacks) {
    auto task_func = []() { return 300; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    std::vector<int> call_order;
    std::mutex order_mutex;

    task.onComplete([&](std::shared_future<int>& fut) {
        std::lock_guard<std::mutex> lock(order_mutex);
        call_order.push_back(1);
        EXPECT_EQ(fut.get(), 300);
    });
    task.onComplete([&](std::shared_future<int>& fut) {
        std::lock_guard<std::mutex> lock(order_mutex);
        call_order.push_back(2);
        EXPECT_EQ(fut.get(), 300);
    });
    task.onComplete([&](std::shared_future<int>& fut) {
        std::lock_guard<std::mutex> lock(order_mutex);
        call_order.push_back(3);
        EXPECT_EQ(fut.get(), 300);
    });

    task();  // Execute

    future.wait();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));  // Give time for callbacks

    // Callbacks should run in registration order (1, 2, 3) because
    // runContinuations reverses the list
    ASSERT_EQ(call_order.size(), 3);
    EXPECT_EQ(call_order[0], 1);
    EXPECT_EQ(call_order[1], 2);
    EXPECT_EQ(call_order[2], 3);
}

// Test onComplete - callback with task exception
TEST_F(PackagedTaskTest, OnCompleteWithTaskException) {
    auto task_func = []() -> int {
        throw std::runtime_error("Task error for callback");
        return 0;
    };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    std::atomic<bool> callback_called = false;
    std::atomic<bool> exception_caught_in_callback = false;

    task.onComplete([&](std::shared_future<int>& fut) {
        callback_called.store(true);
        try {
            fut.get();  // This should rethrow the exception
        } catch (const std::runtime_error& e) {
            exception_caught_in_callback.store(true);
            EXPECT_TRUE(std::string(e.what()).find("Task error for callback") !=
                        std::string::npos);
        } catch (...) {
            // Other exception
        }
    });

    task();  // Execute

    future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(callback_called.load());
    EXPECT_TRUE(exception_caught_in_callback.load());
    EXPECT_THROW(
        future.get(),
        std::runtime_error);  // Verify exception is still on the future
}

// Test onComplete - callback throws exception (should be caught internally)
TEST_F(PackagedTaskTest, OnCompleteCallbackThrows) {
    auto task_func = []() { return 400; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    std::atomic<bool> callback1_called = false;
    std::atomic<bool> callback2_called = false;

    task.onComplete([&](std::shared_future<int>& fut) {
        callback1_called.store(true);
        EXPECT_EQ(fut.get(), 400);
        throw std::runtime_error(
            "Callback 1 error");  // This exception should be caught internally
    });
    task.onComplete([&](std::shared_future<int>& fut) {
        callback2_called.store(true);
        EXPECT_EQ(fut.get(), 400);
    });

    task();  // Execute

    future.wait();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(50));  // Give time for callbacks

    EXPECT_TRUE(callback1_called.load());  // Callback 1 should be called
    EXPECT_TRUE(
        callback2_called
            .load());  // Callback 2 should also be called (execution continues)
    EXPECT_EQ(future.get(), 400);  // Task result should be unaffected
}

// Test cancel() - Pending state
TEST_F(PackagedTaskTest, CancelPending) {
    auto task_func = []() { return 500; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    std::atomic<bool> callback_called = false;
    std::atomic<bool> cancellation_exception_caught = false;

    task.onComplete([&](std::shared_future<int>& fut) {
        callback_called.store(true);
        try {
            fut.get();  // Should throw cancellation exception
        } catch (const InvalidPackagedTaskException& e) {
            cancellation_exception_caught.store(true);
            // Check part of the expected message
            EXPECT_TRUE(std::string(e.what()).find("Task has been cancelled") !=
                        std::string::npos);
        } catch (...) {
            // Other exception
        }
    });

    EXPECT_TRUE(task.cancel());  // Cancel the pending task

    // Task should now be cancelled, future should be ready with exception
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_THROW(future.get(), InvalidPackagedTaskException);
    EXPECT_TRUE(task.isCancelled());

    // Callback should have been run by cancel()
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(callback_called.load());
    EXPECT_TRUE(cancellation_exception_caught.load());

    // Calling operator() after cancel should do nothing
    task();
    // State should remain cancelled, future result unchanged (still exception)
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_THROW(future.get(), InvalidPackagedTaskException);
    EXPECT_TRUE(task.isCancelled());
}

// Test cancel() - Executing state (should fail)
TEST_F(PackagedTaskTest, CancelExecuting) {
    // Need a task that takes time to execute
    auto task_func = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 600;
    };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    // Execute the task in a separate thread to allow main thread to call cancel
    std::thread exec_thread([&]() { task(); });

    // Give the execution thread time to start but not finish
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_FALSE(task.cancel());  // Cancel should fail as state is Executing

    exec_thread.join();  // Wait for execution to finish

    // Task should complete normally, not be cancelled
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_EQ(future.get(), 600);
    EXPECT_FALSE(task.isCancelled());
}

// Test cancel() - Completed state (should fail)
TEST_F(PackagedTaskTest, CancelCompleted) {
    auto task_func = []() { return 700; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    task();  // Execute and complete

    future.wait();  // Wait for completion

    EXPECT_FALSE(task.cancel());  // Cancel should fail as state is Completed
    EXPECT_FALSE(task.isCancelled());
    EXPECT_EQ(future.get(), 700);  // Result should be unaffected
}

// Test cancel() - Cancelled state (should fail)
TEST_F(PackagedTaskTest, CancelCancelled) {
    auto task_func = []() { return 800; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    EXPECT_TRUE(task.cancel());  // Cancel first time

    EXPECT_FALSE(task.cancel());  // Cancel second time should fail
    EXPECT_TRUE(task.isCancelled());
    EXPECT_THROW(future.get(),
                 InvalidPackagedTaskException);  // Future still holds
                                                 // cancellation exception
}

// Test move constructor
TEST_F(PackagedTaskTest, MoveConstructor) {
    auto task_func = []() { return 900; };
    PackagedTask<int> task1(task_func);
    EnhancedFuture<int> future1 = task1.getEnhancedFuture();

    PackagedTask<int> task2 = std::move(task1);

    // task1 should be in a moved-from state (invalid)
    EXPECT_FALSE(task1);  // operator bool()

    // task2 should be valid and hold the task/promise
    EXPECT_TRUE(task2);
    EnhancedFuture<int> future2 =
        task2.getEnhancedFuture();  // Get future from moved task

    task2();  // Execute the task via the moved object

    EXPECT_TRUE(future2.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_EQ(future2.get(), 900);

    // The original future (future1) should also be ready with the same result
    EXPECT_TRUE(future1.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_EQ(future1.get(), 900);
}

// Test move assignment
TEST_F(PackagedTaskTest, MoveAssignment) {
    auto task_func1 = []() { return 1100; };
    auto task_func2 = []() { return 1200; };

    PackagedTask<int> task1(task_func1);
    EnhancedFuture<int> future1 = task1.getEnhancedFuture();

    PackagedTask<int> task2(task_func2);
    EnhancedFuture<int> future2 = task2.getEnhancedFuture();

    task2 = std::move(task1);  // Move task1 into task2

    // task1 should be in a moved-from state (invalid)
    EXPECT_FALSE(task1);

    // task2 should now hold the task from task1
    EXPECT_TRUE(task2);
    EnhancedFuture<int> future2_after_move =
        task2.getEnhancedFuture();  // Get future from task2 after assignment

    task2();  // Execute the task via task2 (should be task_func1)

    EXPECT_TRUE(future2_after_move.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_EQ(future2_after_move.get(),
              1100);  // Result should be from task_func1

    // The original future from task1 should also be ready
    EXPECT_TRUE(future1.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_EQ(future1.get(), 1100);

    // The original future from task2 (before assignment) should be unaffected
    // or invalid Depending on std::promise/future behavior after move
    // assignment of the task holding them. Let's check if it's still valid and
    // not ready. Note: Moving a PackagedTask moves the promise. The old future
    // (future2) is still valid and refers to the *original* promise, which is
    // now owned by the moved-to task (task2). So future2 should become ready
    // when task2 is executed.
    EXPECT_TRUE(future2.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_EQ(future2.get(),
              1100);  // It should also get the result from task_func1
}

// --- PackagedTask (void) Tests ---

// Test constructor for void task
TEST_F(PackagedTaskTest, VoidConstructorValidTask) {
    auto task_func = []() {};
    PackagedTask<void> task(task_func);
    EXPECT_TRUE(task);
}

// Test constructor for invalid void task
TEST_F(PackagedTaskTest, VoidConstructorInvalidTask) {
    std::function<void()> invalid_func = nullptr;
    EXPECT_THROW(
        { PackagedTask<void> task(invalid_func); },
        InvalidPackagedTaskException);
}

// Test getEnhancedFuture for void task
TEST_F(PackagedTaskTest, VoidGetEnhancedFuture) {
    auto task_func = []() {};
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();
    EXPECT_TRUE(future.valid());
}

// Test operator() execution - void success
TEST_F(PackagedTaskTest, VoidOperatorCallSuccess) {
    std::atomic<bool> task_ran = false;
    auto task_func = [&]() { task_ran.store(true); };
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    task();  // Execute

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    future.get();  // Should not throw
    EXPECT_TRUE(task_ran.load());
}

// Test operator() execution - void exception
TEST_F(PackagedTaskTest, VoidOperatorCallException) {
    auto task_func = []() { throw std::runtime_error("Void task error"); };
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    task();  // Execute

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_THROW(future.get(), std::runtime_error);
}

// Test onComplete for void task - registered before execution
TEST_F(PackagedTaskTest, VoidOnCompleteBeforeExecution) {
    std::atomic<bool> task_ran = false;
    auto task_func = [&]() { task_ran.store(true); };
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    std::atomic<bool> callback_called = false;
    task.onComplete([&]() {  // Void callback takes no args
        callback_called.store(true);
    });

    EXPECT_FALSE(callback_called.load());

    task();  // Execute

    future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(task_ran.load());
    EXPECT_TRUE(callback_called.load());
}

// Test onComplete for void task - registered after execution
TEST_F(PackagedTaskTest, VoidOnCompleteAfterExecution) {
    std::atomic<bool> task_ran = false;
    auto task_func = [&]() { task_ran.store(true); };
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    task();  // Execute first
    future.wait();

    std::atomic<bool> callback_called = false;
    task.onComplete([&]() {  // Void callback takes no args
        callback_called.store(true);
    });

    // Callback should run immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(task_ran.load());
    EXPECT_TRUE(callback_called.load());
}

// Test onComplete for void task - callback with task exception
TEST_F(PackagedTaskTest, VoidOnCompleteWithTaskException) {
    auto task_func = []() {
        throw std::runtime_error("Void task error for callback");
    };
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    std::atomic<bool> callback_called = false;
    std::atomic<bool> exception_caught_in_callback = false;

    task.onComplete(
        [&](std::shared_future<void>&
                fut) {  // Void callback can take shared_future<void>
            callback_called.store(true);
            try {
                fut.get();  // Should rethrow
            } catch (const std::runtime_error& e) {
                exception_caught_in_callback.store(true);
                EXPECT_TRUE(std::string(e.what()).find(
                                "Void task error for callback") !=
                            std::string::npos);
            } catch (...) {
                // Other exception
            }
        });

    task();  // Execute

    future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(callback_called.load());
    EXPECT_TRUE(exception_caught_in_callback.load());
    EXPECT_THROW(future.get(), std::runtime_error);
}

// Test cancel() for void task - Pending state
TEST_F(PackagedTaskTest, VoidCancelPending) {
    auto task_func = []() {};
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    std::atomic<bool> callback_called = false;
    std::atomic<bool> cancellation_exception_caught = false;

    task.onComplete([&](std::shared_future<void>& fut) {
        callback_called.store(true);
        try {
            fut.get();  // Should throw cancellation exception
        } catch (const InvalidPackagedTaskException& e) {
            cancellation_exception_caught.store(true);
            EXPECT_TRUE(std::string(e.what()).find("Task has been cancelled") !=
                        std::string::npos);
        } catch (...) {
            // Other exception
        }
    });

    EXPECT_TRUE(task.cancel());  // Cancel the pending task

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_THROW(future.get(), InvalidPackagedTaskException);
    EXPECT_TRUE(task.isCancelled());

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(callback_called.load());
    EXPECT_TRUE(cancellation_exception_caught.load());
}

// --- make_enhanced_task Tests ---

TEST_F(PackagedTaskTest, MakeEnhancedTaskValue) {
    auto task = make_enhanced_task([](int x, int y) { return x * y; });
    EnhancedFuture<int> future = task.getEnhancedFuture();
    task(3, 4);
    EXPECT_EQ(future.get(), 12);
}

TEST_F(PackagedTaskTest, MakeEnhancedTaskVoid) {
    std::atomic<bool> called = false;
    auto task = make_enhanced_task(
        [&](const std::string& s) { called.store(!s.empty()); });
    EnhancedFuture<void> future = task.getEnhancedFuture();
    task("hello");
    future.wait();
    EXPECT_TRUE(called.load());
}

TEST_F(PackagedTaskTest, MakeEnhancedTaskNoArgs) {
    auto task = make_enhanced_task([]() { return "no args"; });
    EnhancedFuture<std::string> future = task.getEnhancedFuture();
    task();
    EXPECT_EQ(future.get(), "no args");
}

TEST_F(PackagedTaskTest, MakeEnhancedTaskVoidNoArgs) {
    std::atomic<bool> called = false;
    auto task = make_enhanced_task([&]() { called.store(true); });
    EnhancedFuture<void> future = task.getEnhancedFuture();
    task();
    future.wait();
    EXPECT_TRUE(called.load());
}

// Test make_enhanced_task with explicit signature
TEST_F(PackagedTaskTest, MakeEnhancedTaskExplicitSignature) {
    auto task = make_enhanced_task<double(int, double)>(
        [](int i, double d) { return i + d; });
    EnhancedFuture<double> future = task.getEnhancedFuture();
    task(5, 3.14);
    EXPECT_DOUBLE_EQ(future.get(), 8.14);
}

// --- Concurrency Tests ---

TEST_F(PackagedTaskTest, ConcurrentOperatorCall) {
    std::atomic<int> execution_count = 0;
    auto task_func = [&]() {
        execution_count.fetch_add(1);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));  // Simulate work
    };
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    const int num_threads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            task();  // All threads try to execute the same task
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Only the first call should have resulted in execution
    EXPECT_EQ(execution_count.load(), 1);
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(1)) ==
                std::future_status::ready);
    EXPECT_NO_THROW(future.get());
}

TEST_F(PackagedTaskTest, ConcurrentOnCompleteBeforeExecution) {
    auto task_func = []() { return 1; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    const int num_callbacks = 100;
    std::atomic<int> callbacks_called_count = 0;
    std::vector<std::thread> threads;

    // Launch threads to register callbacks concurrently
    for (int i = 0; i < num_callbacks; ++i) {
        threads.emplace_back([&, i]() {
            task.onComplete([&, i](std::shared_future<int>& fut) {
                callbacks_called_count.fetch_add(1);
                EXPECT_EQ(fut.get(), 1);
            });
        });
    }

    // Join registration threads
    for (auto& t : threads) {
        t.join();
    }

    // Execute the task
    task();

    // Wait for task and callbacks
    future.wait();
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Give time for continuations

    EXPECT_EQ(callbacks_called_count.load(), num_callbacks);
    EXPECT_EQ(future.get(), 1);
}

TEST_F(PackagedTaskTest, ConcurrentOnCompleteAfterExecution) {
    auto task_func = []() { return 2; };
    PackagedTask<int> task(task_func);
    EnhancedFuture<int> future = task.getEnhancedFuture();

    // Execute the task first
    task();
    future.wait();

    const int num_callbacks = 100;
    std::atomic<int> callbacks_called_count = 0;
    std::vector<std::thread> threads;

    // Launch threads to register callbacks concurrently after execution
    for (int i = 0; i < num_callbacks; ++i) {
        threads.emplace_back([&, i]() {
            task.onComplete([&, i](std::shared_future<int>& fut) {
                callbacks_called_count.fetch_add(1);
                EXPECT_EQ(fut.get(), 2);
            });
        });
    }

    // Join registration threads
    for (auto& t : threads) {
        t.join();
    }

    // Callbacks should run immediately when registered after completion.
    // Give a moment for immediate execution.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(callbacks_called_count.load(), num_callbacks);
    EXPECT_EQ(future.get(), 2);
}

TEST_F(PackagedTaskTest, ConcurrentCancelAndExecute) {
    std::atomic<bool> task_started = false;
    std::atomic<bool> task_finished = false;
    auto task_func = [&]() {
        task_started.store(true);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));  // Simulate work
        task_finished.store(true);
    };
    PackagedTask<void> task(task_func);
    EnhancedFuture<void> future = task.getEnhancedFuture();

    // Thread to execute the task
    std::thread exec_thread([&]() { task(); });

    // Thread to cancel the task
    std::thread cancel_thread([&]() {
        // Wait a moment to increase chance of hitting Executing state
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        task.cancel();
    });

    exec_thread.join();
    cancel_thread.join();

    // Check the outcome
    if (task.isCancelled()) {
        // Cancel succeeded (must have been Pending)
        EXPECT_FALSE(task_started.load());  // Task should not have started
        EXPECT_FALSE(task_finished.load());
        EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
        EXPECT_THROW(future.get(), InvalidPackagedTaskException);
    } else {
        // Cancel failed (task was Executing or Completed)
        EXPECT_TRUE(task_started.load());   // Task should have started
        EXPECT_TRUE(task_finished.load());  // Task should have finished
        EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready);
        EXPECT_NO_THROW(future.get());  // Task completed normally
    }
}

#ifdef ATOM_USE_ASIO
// --- ASIO Integration Tests ---

TEST_F(PackagedTaskTest, AsioConstructorAndExecution) {
    asio::io_context io_context;
    std::atomic<bool> task_ran = false;

    auto task = make_enhanced_task_with_asio([&]() { task_ran.store(true); },
                                             &io_context);

    EnhancedFuture<void> future = task.getEnhancedFuture();

    // Running the task should post to the io_context
    task();

    // Task should not have run yet
    EXPECT_FALSE(task_ran.load());
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::timeout);

    // Run the io_context
    io_context.run_for(std::chrono::seconds(1));

    // Task should have run now
    EXPECT_TRUE(task_ran.load());
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_NO_THROW(future.get());
}

TEST_F(PackagedTaskTest, AsioExecutionWithArgsAndReturn) {
    asio::io_context io_context;
    std::atomic<int> result = 0;

    auto task = make_enhanced_task_with_asio([](int a, int b) { return a + b; },
                                             &io_context);

    EnhancedFuture<int> future = task.getEnhancedFuture();

    task(10, 20);

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::timeout);

    io_context.run_for(std::chrono::seconds(1));

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_EQ(future.get(), 30);
}

TEST_F(PackagedTaskTest, AsioExecutionWithException) {
    asio::io_context io_context;

    auto task = make_enhanced_task_with_asio(
        []() -> int {
            throw std::runtime_error("ASIO task error");
            return 0;
        },
        &io_context);

    EnhancedFuture<int> future = task.getEnhancedFuture();

    task();

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::timeout);

    io_context.run_for(std::chrono::seconds(1));

    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(PackagedTaskTest, AsioOnComplete) {
    asio::io_context io_context;
    std::atomic<bool> task_ran = false;
    std::atomic<bool> callback_ran = false;

    auto task = make_enhanced_task_with_asio([&]() { task_ran.store(true); },
                                             &io_context);

    EnhancedFuture<void> future = task.getEnhancedFuture();

    task.onComplete([&]() { callback_ran.store(true); });

    // Neither task nor callback should have run yet
    EXPECT_FALSE(task_ran.load());
    EXPECT_FALSE(callback_ran.load());

    // Running the task posts it to io_context
    task();

    // Task is posted, not run yet
    EXPECT_FALSE(task_ran.load());
    EXPECT_FALSE(callback_ran.load());

    // Run io_context - task executes, then callback executes
    io_context.run_for(std::chrono::seconds(1));

    EXPECT_TRUE(task_ran.load());
    EXPECT_TRUE(callback_ran.load());
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_NO_THROW(future.get());
}

TEST_F(PackagedTaskTest, AsioOnCompleteAfterExecution) {
    asio::io_context io_context;
    std::atomic<bool> task_ran = false;
    std::atomic<bool> callback_ran = false;

    auto task = make_enhanced_task_with_asio([&]() { task_ran.store(true); },
                                             &io_context);

    EnhancedFuture<void> future = task.getEnhancedFuture();

    // Run the task first
    task();
    io_context.run_for(std::chrono::seconds(1));  // Execute the task

    EXPECT_TRUE(task_ran.load());
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);

    // Register callback after execution
    task.onComplete([&]() { callback_ran.store(true); });

    // Callback should be posted to io_context and run when context is run again
    EXPECT_FALSE(callback_ran.load());

    io_context.restart();  // Need to restart context to run more handlers
    io_context.run_for(std::chrono::seconds(1));

    EXPECT_TRUE(callback_ran.load());
}

TEST_F(PackagedTaskTest, AsioCancelPending) {
    asio::io_context io_context;
    std::atomic<bool> task_ran = false;
    std::atomic<bool> callback_ran = false;
    std::atomic<bool> cancellation_exception_caught = false;

    auto task = make_enhanced_task_with_asio([&]() { task_ran.store(true); },
                                             &io_context);

    EnhancedFuture<void> future = task.getEnhancedFuture();

    task.onComplete([&](std::shared_future<void>& fut) {
        callback_ran.store(true);
        try {
            fut.get();  // Should throw cancellation exception
        } catch (const InvalidPackagedTaskException& e) {
            cancellation_exception_caught.store(true);
        } catch (...) {
        }
    });

    // Task is pending, not posted yet
    EXPECT_FALSE(task_ran.load());
    EXPECT_FALSE(callback_ran.load());

    EXPECT_TRUE(task.cancel());  // Cancel the pending task

    // Cancelling should set exception and run continuations (which are posted
    // to ASIO context)
    EXPECT_TRUE(task.isCancelled());
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
    EXPECT_THROW(future.get(), InvalidPackagedTaskException);

    // Callback should be posted but not run yet
    EXPECT_FALSE(callback_ran.load());

    // Run io_context to execute the posted callback
    io_context.run_for(std::chrono::seconds(1));

    EXPECT_TRUE(callback_ran.load());
    EXPECT_TRUE(cancellation_exception_caught.load());
    EXPECT_FALSE(task_ran.load());  // Task itself should not have run
}

TEST_F(PackagedTaskTest, AsioSetAsioContext) {
    asio::io_context io_context1;
    asio::io_context io_context2;
    std::atomic<bool> task_ran = false;

    // Create task without context initially
    PackagedTask<void> task([&]() { task_ran.store(true); });

    EnhancedFuture<void> future = task.getEnhancedFuture();

    // Set context 1
    task.setAsioContext(&io_context1);
    EXPECT_EQ(task.getAsioContext(), &io_context1);

    // Execute - should post to context 1
    task();

    // Run context 1 - task should execute
    io_context1.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(task_ran.load());
    EXPECT_TRUE(future.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);

    // Reset for next test
    task_ran.store(false);
    // Cannot reuse a completed task, need a new one
    PackagedTask<void> task2([&]() { task_ran.store(true); });
    EnhancedFuture<void> future2 = task2.getEnhancedFuture();

    // Set context 2
    task2.setAsioContext(&io_context2);
    EXPECT_EQ(task2.getAsioContext(), &io_context2);

    // Execute - should post to context 2
    task2();

    // Run context 1 - task should NOT execute
    io_context1.restart();
    io_context1.run_for(std::chrono::seconds(1));
    EXPECT_FALSE(task_ran.load());
    EXPECT_TRUE(future2.wait_for(std::chrono::seconds(0)) ==
                std::future_status::timeout);

    // Run context 2 - task should execute
    io_context2.run_for(std::chrono::seconds(1));
    EXPECT_TRUE(task_ran.load());
    EXPECT_TRUE(future2.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready);
}

#endif  // ATOM_USE_ASIO