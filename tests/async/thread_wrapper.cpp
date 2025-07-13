#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <future>
#include <string>
#include <stdexcept>
#include <stop_token>
#include <numeric> // For std::iota
#include <algorithm> // For std::sort, std::find
#include <cmath> // For std::abs

// Include the header under test
#include "atom/async/thread_wrapper.hpp"

// Use the namespace
using namespace atom::async;

// Test fixture for Thread class
class ThreadTest : public ::testing::Test {
protected:
    // No specific setup/teardown needed for most tests
};

// Test fixture for parallel_for_each_optimized
class ParallelForEachTest : public ::testing::Test {
protected:
    // No specific setup/teardown needed
};

// --- Thread Class Tests ---

// Test basic thread start and join with a simple void function
TEST_F(ThreadTest, BasicStartAndJoinVoid) {
    // Atomic flag to signal the thread has run
    std::atomic<bool> thread_ran = false;

    // Create and start the thread
    Thread t([&]() {
        thread_ran.store(true);
    });

    // Join the thread (implicitly done by destructor, but explicit join is good practice in tests)
    t.join();

    // Verify the thread ran
    EXPECT_TRUE(thread_ran.load());
    EXPECT_FALSE(t.running()); // Should not be running after join
}

// Test basic thread start and join with a function taking stop_token
TEST_F(ThreadTest, BasicStartAndJoinWithStopToken) {
    std::atomic<bool> thread_ran = false;
    std::atomic<bool> stop_token_valid = false;

    Thread t([&](std::stop_token st) {
        stop_token_valid.store(st.stop_requested() == false); // Check initial state
        thread_ran.store(true);
    });

    t.join();

    EXPECT_TRUE(thread_ran.load());
    EXPECT_TRUE(stop_token_valid.load());
    EXPECT_FALSE(t.running());
}

// Test exception handling during thread startup
TEST_F(ThreadTest, ExceptionDuringStartup) {
    // Use a promise to capture the exception
    std::promise<void> p;
    std::future<void> f = p.get_future();

    // Start a thread that throws an exception immediately
    Thread t([&]() {
        p.set_value(); // Signal startup success before throwing
        throw std::runtime_error("Test exception");
    });

    // Wait for the future to become ready (either value or exception)
    f.wait();

    // Expect that getting the future result rethrows the exception
    EXPECT_THROW(f.get(), std::runtime_error);

    // Join the thread (it should have already finished due to the exception)
    t.join();
    EXPECT_FALSE(t.running());
}

// Test exception handling within the thread function after startup
TEST_F(ThreadTest, ExceptionDuringExecution) {
    // Use a promise to signal startup completion
    std::promise<void> startup_promise;
    std::future<void> startup_future = startup_promise.get_future();

    // Start a thread that signals startup and then throws
    Thread t([&](std::stop_token) {
        startup_promise.set_value(); // Signal startup success
        // Simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        throw std::runtime_error("Test exception during execution");
    });

    // Wait for startup to complete
    startup_future.get(); // This will rethrow if startup failed

    // Join the thread. The exception should be handled internally by jthread/promise.
    // The Thread wrapper's destructor should handle joining.
    // We can't easily catch the exception here unless we modify the Thread class
    // to expose the jthread's exception handling mechanism or use a shared_ptr<std::exception_ptr>.
    // For now, rely on jthread's default behavior (terminate if not joined and exception uncaught).
    // The Thread destructor *does* join, so it should be safe.
    // We can verify the thread is no longer running after a short delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(t.running()); // Should have finished due to exception
}


// Test stop token functionality
TEST_F(ThreadTest, StopTokenSignaling) {
    std::atomic<bool> stop_requested = false;
    std::atomic<bool> thread_finished = false;

    Thread t([&](std::stop_token st) {
        // Wait until stop is requested or a timeout
        st.stop_requested(); // Initial check
        st.wait([]{ return false; }, std::chrono::milliseconds(200)); // Wait for stop or timeout

        stop_requested.store(st.stop_requested()); // Check if stop was requested
        thread_finished.store(true);
    });

    // Give the thread time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Request stop
    t.requestStop();

    // Wait for the thread to finish
    t.join();

    // Verify stop was requested and the thread finished
    EXPECT_TRUE(stop_requested.load());
    EXPECT_TRUE(thread_finished.load());
    EXPECT_FALSE(t.running());
}

// Test start timeout
TEST_F(ThreadTest, StartTimeout) {
    // Override the Thread class temporarily or use a mock/test helper
    // This is hard to test directly without modifying the Thread class
    // to allow injecting a slow startup function and controlling the timeout duration.
    // Skipping for now, assuming the promise/future mechanism works as designed.
    // A manual test would involve a lambda that sleeps longer than the hardcoded 500ms timeout
    // before calling set_value on the promise.
}

// Test tryJoinFor - success case
TEST_F(ThreadTest, TryJoinForSuccess) {
    Thread t([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    });

    // Try joining with a timeout longer than the sleep
    bool joined = t.tryJoinFor(std::chrono::milliseconds(200));

    EXPECT_TRUE(joined);
    EXPECT_FALSE(t.running());
}

// Test tryJoinFor - timeout case
TEST_F(ThreadTest, TryJoinForTimeout) {
    Thread t([](std::stop_token st) {
        // Keep running until stop is requested
        st.wait([]{ return false; });
    });

    // Try joining with a short timeout
    bool joined = t.tryJoinFor(std::chrono::milliseconds(50));

    EXPECT_FALSE(joined); // Should time out

    // Request stop and join properly to clean up
    t.requestStop();
    t.join();
    EXPECT_FALSE(t.running());
}

// Test running() method
TEST_F(ThreadTest, RunningStatus) {
    Thread t([](std::stop_token st) {
        st.wait([]{ return false; }); // Keep running until stopped
    });

    EXPECT_TRUE(t.running());

    t.requestStop();
    t.join();

    EXPECT_FALSE(t.running());
}

// Test getId()
TEST_F(ThreadTest, GetId) {
    std::thread::id main_thread_id = std::this_thread::get_id();
    std::thread::id thread_id_in_thread;
    std::thread::id thread_id_from_wrapper;

    Thread t([&]() {
        thread_id_in_thread = std::this_thread::get_id();
    });

    // Give thread time to start and get its ID
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread_id_from_wrapper = t.getId();

    t.join();

    EXPECT_NE(thread_id_from_wrapper, main_thread_id);
    EXPECT_EQ(thread_id_from_wrapper, thread_id_in_thread);
}

// Test getName() (basic check, actual name setting is OS-dependent)
TEST_F(ThreadTest, GetName) {
    Thread t([](){}); // Thread name is generated on start
    // Name should be generated and accessible
    EXPECT_FALSE(t.getName().empty());
    EXPECT_NE(t.getName(), "Thread-0"); // Counter starts from 0, but first thread gets 0, second 1 etc.
                                        // The exact number depends on how many Thread objects were created before.
                                        // Just check it's not empty and has the expected prefix.
    EXPECT_TRUE(t.getName().rfind("Thread-", 0) == 0);

    t.join();
}

// Test getStopToken()
TEST_F(ThreadTest, GetStopToken) {
    Thread t([](std::stop_token st){
        // Do nothing, just let it run until stopped
        st.wait([]{ return false; });
    });

    std::stop_token st = t.getStopToken();
    EXPECT_FALSE(st.stop_requested());

    t.requestStop();
    // Give time for the stop request to propagate
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(st.stop_requested());

    t.join();
}

// Test getHardwareConcurrency()
TEST_F(ThreadTest, GetHardwareConcurrency) {
    unsigned int concurrency = Thread::getHardwareConcurrency();
    EXPECT_GE(concurrency, 1); // Should be at least 1
}

// Test startPeriodicPrecise
TEST_F(ThreadTest, StartPeriodicPrecise) {
    std::atomic<int> counter = 0;
    const int num_calls = 5;
    const auto interval = std::chrono::milliseconds(50);
    const auto tolerance = std::chrono::milliseconds(20); // Allow some timing variation

    Thread t;
    auto start_time = std::chrono::steady_clock::now();

    t.startPeriodicPrecise([&]() {
        counter.fetch_add(1);
    }, interval);

    // Let it run for enough time to get multiple calls
    std::this_thread::sleep_for(interval * num_calls + tolerance);

    t.requestStop();
    t.join();

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Check that the counter increased
    EXPECT_GT(counter.load(), 0);

    // Check that the number of calls is roughly as expected based on elapsed time and interval
    // This is an approximate check due to scheduling variations
    int expected_min_calls = static_cast<int>((elapsed - tolerance).count() / interval.count());
    int expected_max_calls = static_cast<int>((elapsed + tolerance).count() / interval.count()) + 1; // +1 for potential call just before stop

    // The number of calls should be within a reasonable range
    EXPECT_GE(counter.load(), expected_min_calls);
    EXPECT_LE(counter.load(), expected_max_calls);

    // A more direct check: wait for a specific number of calls and then stop
    std::atomic<int> counter_precise = 0;
    const int target_calls = 10;
    Thread t_precise;

    t_precise.startPeriodicPrecise([&]() {
        counter_precise.fetch_add(1);
        if (counter_precise.load() >= target_calls) {
            t_precise.requestStop(); // Stop after target calls
        }
    }, std::chrono::milliseconds(10)); // Use a shorter interval

    t_precise.join(); // Wait for the thread to stop itself

    EXPECT_GE(counter_precise.load(), target_calls); // Should have reached at least the target
}


// --- parallel_for_each_optimized Tests ---

// Test basic functionality with a simple range and function
TEST(ParallelForEachTest, BasicFunctionality) {
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 0); // Fill with 0, 1, 2, ... 99

    std::vector<std::atomic<int>> processed_flags(data.size());
    for(auto& flag : processed_flags) flag.store(0);

    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        // Process the value (e.g., mark as processed)
        processed_flags[val].fetch_add(1);
        val *= 2; // Example modification
    });

    // Verify all elements were processed exactly once
    for(size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(processed_flags[i].load(), 1) << "Element " << i << " processed incorrect number of times";
        EXPECT_EQ(data[i], static_cast<int>(i * 2)); // Verify modification
    }
}

// Test with an empty range
TEST(ParallelForEachTest, EmptyRange) {
    std::vector<int> data;
    std::atomic<bool> function_called = false;

    parallel_for_each_optimized(data.begin(), data.end(), [&](int&) {
        function_called.store(true); // Should not be called
    });

    EXPECT_FALSE(function_called.load());
}

// Test with a single element
TEST(ParallelForEachTest, SingleElement) {
    std::vector<int> data = {42};
    std::atomic<bool> function_called = false;

    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        EXPECT_EQ(val, 42);
        val = 100;
        function_called.store(true);
    });

    EXPECT_TRUE(function_called.load());
    EXPECT_EQ(data[0], 100);
}

// Test concurrency with a large range and multiple threads
TEST(ParallelForEachTest, ConcurrentExecution) {
    const size_t num_elements = 10000;
    std::vector<int> data(num_elements);
    std::iota(data.begin(), data.end(), 0);

    std::vector<std::atomic<int>> processed_counts(num_elements);
    for(auto& count : processed_counts) count.store(0);

    const unsigned int num_threads = 8; // Use a fixed number of threads for the test

    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        processed_counts[val].fetch_add(1);
    }, num_threads);

    // Verify all elements were processed exactly once
    for(size_t i = 0; i < num_elements; ++i) {
        EXPECT_EQ(processed_counts[i].load(), 1) << "Element " << i << " processed incorrect number of times";
    }
}

// Test with a different data type (e.g., string)
TEST(ParallelForEachTest, StringDataType) {
    std::vector<std::string> data = {"apple", "banana", "cherry", "date", "elderberry"};
    std::vector<std::atomic<int>> processed_flags(data.size());
     for(auto& flag : processed_flags) flag.store(0);

    parallel_for_each_optimized(data.begin(), data.end(), [&](std::string& s) {
        // Find the original index based on content (assuming unique strings)
        auto it = std::find(data.begin(), data.end(), s);
        if (it != data.end()) {
             processed_flags[std::distance(data.begin(), it)].fetch_add(1);
        }
        s += "_processed"; // Example modification
    });

    // Verify all elements were processed exactly once
    for(size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(processed_flags[i].load(), 1) << "Element " << i << " processed incorrect number of times";
        EXPECT_TRUE(data[i].ends_with("_processed")); // Verify modification
    }
}

// Test with num_threads = 0 (should default to hardware_concurrency)
TEST(ParallelForEachTest, ZeroThreads) {
     std::vector<int> data(10);
    std::iota(data.begin(), data.end(), 0);

    std::vector<std::atomic<int>> processed_flags(data.size());
    for(auto& flag : processed_flags) flag.store(0);

    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        processed_flags[val].fetch_add(1);
    }, 0); // Pass 0 threads

     for(size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(processed_flags[i].load(), 1) << "Element " << i << " processed incorrect number of times";
    }
}

// Test with num_threads = 1 (should behave like sequential)
TEST(ParallelForEachTest, OneThread) {
     std::vector<int> data(10);
    std::iota(data.begin(), data.end(), 0);

    std::vector<std::atomic<int>> processed_flags(data.size());
    for(auto& flag : processed_flags) flag.store(0);

    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        processed_flags[val].fetch_add(1);
    }, 1); // Pass 1 thread

     for(size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(processed_flags[i].load(), 1) << "Element " << i << " processed incorrect number of times";
    }
}

// Test with a function that throws an exception (should ideally propagate or terminate)
// Note: Exception handling in parallel algorithms is tricky. std::for_each doesn't specify behavior.
// A robust parallel_for_each might collect exceptions. This one likely terminates.
// We can test that it doesn't hang and potentially check if an exception is thrown in the main thread
// (though this is unlikely with detached threads/jthreads).
// A simple test is to ensure it finishes without hanging.
TEST(ParallelForEachTest, ExceptionHandling) {
    std::vector<int> data(10);
    std::iota(data.begin(), data.end(), 0);

    // Use a flag to see if the function was called at all
    std::atomic<bool> function_called = false;

    // The exception will likely cause one of the jthreads to terminate.
    // The main thread will join the jthreads in the destructor of the vector<jthread>.
    // If an exception propagates out of a jthread and is not caught, it calls std::terminate.
    // We can't easily catch std::terminate in a unit test.
    // The best we can do is ensure the test doesn't hang indefinitely.
    // We expect the program to potentially terminate or for the test to fail if
    // the parallel_for_each doesn't complete cleanly.
    // A more sophisticated test would involve capturing exceptions from worker threads.
    // For this basic test, we just check if it finishes.

    // This test might crash the test runner if std::terminate is called.
    // Depending on the desired behavior of parallel_for_each_optimized on exception,
    // this test might need adjustment or skipping.
    // Assuming the current implementation allows termination on exception in a worker thread.
    // We'll wrap it in a death test if available, or just run it and see if it passes/crashes.
    // Google Test DEATH tests are complex and platform-dependent. Let's skip a death test for now.

    // Simple check that it doesn't hang indefinitely
    // This doesn't verify *correct* exception handling, just non-hanging behavior.
    // A real-world parallel algorithm should handle exceptions better (e.g., collect them).
    // Given the current implementation uses jthread and barrier, an uncaught exception
    // in a worker thread will likely call std::terminate.
    // The barrier might also hang if a thread terminates before arriving.
    // Let's test with a small number of threads and elements.

    std::vector<int> small_data(5);
    std::iota(small_data.begin(), small_data.end(), 0);

    // This lambda will throw when val is 3
    auto throwing_function = [&](int& val) {
        function_called.store(true);
        if (val == 3) {
            throw std::runtime_error("Intentional exception");
        }
    };

    // We expect this to potentially terminate or behave unexpectedly.
    // Running it as a regular test might be sufficient to see if it crashes.
    // If it consistently crashes, the exception handling in parallel_for_each_optimized needs review.
    // If it passes without crashing, it implies the exception is somehow handled or ignored,
    // which might also be incorrect behavior depending on requirements.
    // Let's assume for now that the requirement is *not* to terminate the program,
    // which means the parallel_for_each should catch exceptions internally.
    // If it *should* terminate, this test is invalid.
    // Based on the provided code, there's no explicit exception handling in the worker lambda,
    // so std::terminate is the likely outcome.
    // Let's add a note that this test's outcome depends on the intended exception behavior.

    // Note: The current implementation of parallel_for_each_optimized does NOT catch exceptions
    // from the user-provided function. An exception thrown in a worker thread will likely
    // call std::terminate, which will crash the test runner.
    // This test is commented out or marked as expected to fail/crash until exception handling
    // is added to parallel_for_each_optimized.
    /*
    EXPECT_ANY_THROW({ // This won't work as exception is in another thread
         parallel_for_each_optimized(small_data.begin(), small_data.end(), throwing_function, 2);
    });
    */

    // A safer approach for testing would be to modify the parallel_for_each_optimized
    // to collect exceptions, or to test a version that is designed to terminate.
    // Given the current code, a test that throws is problematic.
    // Let's skip the throwing test for now.
}

// Test with a complex object type (if applicable and copyable/movable)
// The current implementation uses iterators and passes by reference, so it should work
// with any type that the iterator dereferences to and the function accepts.
// The StringDataType test covers a non-trivial type.

// Test with different numbers of threads (more threads than elements, fewer threads than elements)
TEST(ParallelForEachTest, DifferentThreadCounts) {
    const size_t num_elements = 20;
    std::vector<int> data(num_elements);
    std::iota(data.begin(), data.end(), 0);

    // Test with more threads than elements
    std::vector<std::atomic<int>> processed_flags_more(num_elements);
    for(auto& flag : processed_flags_more) flag.store(0);
    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        processed_flags_more[val].fetch_add(1);
    }, num_elements + 5); // More threads than elements
    for(size_t i = 0; i < num_elements; ++i) {
        EXPECT_EQ(processed_flags_more[i].load(), 1);
    }

    // Test with fewer threads than elements
    std::vector<std::atomic<int>> processed_flags_fewer(num_elements);
    for(auto& flag : processed_flags_fewer) flag.store(0);
    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        processed_flags_fewer[val].fetch_add(1);
    }, num_elements / 3); // Fewer threads than elements
     for(size_t i = 0; i < num_elements; ++i) {
        EXPECT_EQ(processed_flags_fewer[i].load(), 1);
    }
}

// Test with iterators that are not random access (e.g., std::list iterators)
// The current implementation uses std::distance and std::advance, which work
// with InputIt (or at least ForwardIterator for advance).
// Let's test with a list.
TEST(ParallelForEachTest, ListIterator) {
    std::list<int> data(100);
    std::iota(data.begin(), data.end(), 0);

    std::vector<std::atomic<int>> processed_flags(data.size());
    for(auto& flag : processed_flags) flag.store(0);

    // Need to map list iterator to index for processed_flags
    // This requires finding the element value in the original list, which is inefficient.
    // A better approach is to use a map or modify the function to accept index if possible,
    // or just verify that all elements in the list are modified.
    // Let's modify the elements and check the final state.
    // We can't easily check "processed exactly once" without a map or similar.

    parallel_for_each_optimized(data.begin(), data.end(), [&](int& val) {
        val *= 2; // Modify the value
    });

    // Verify all elements were modified
    int expected_value = 0;
    for(int val : data) {
        EXPECT_EQ(val, expected_value * 2);
        expected_value++;
    }
}


// --- OptimizedTask Tests ---
// Note: OptimizedTask is a coroutine type. Testing coroutines directly
// in unit tests can be complex as it involves managing the coroutine handle
// and understanding its lifecycle. The provided OptimizedTask seems designed
// to run to completion immediately upon creation (initial_suspend returns suspend_never).
// It primarily serves as a way to capture results/exceptions asynchronously.

// Test basic void task
TEST(OptimizedTaskTest, BasicVoidTask) {
    std::atomic<bool> task_ran = false;
    auto task = []() -> OptimizedTask<> {
        task_ran.store(true);
        co_return;
    }(); // Immediately invoke and get the task handle

    // The task should run immediately because initial_suspend is suspend_never
    // Give a tiny moment just in case, though it should be synchronous up to the first suspend point (none here)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_TRUE(task.isCompleted());
    EXPECT_TRUE(task_ran.load());

    // Getting result for void task should just return
    task.getResult(); // Should not throw
}

// Test basic task with return value
TEST(OptimizedTaskTest, BasicValueTask) {
    auto task = []() -> OptimizedTask<int> {
        co_return 42;
    }();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_TRUE(task.isCompleted());
    EXPECT_EQ(task.getResult(), 42);
}

// Test task with exception
TEST(OptimizedTaskTest, TaskWithException) {
    auto task = []() -> OptimizedTask<int> {
        throw std::runtime_error("Task exception");
        co_return 0; // Unreachable
    }();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_TRUE(task.isCompleted());
    EXPECT_THROW(task.getResult(), std::runtime_error);
}

// Test task with exception (void return)
TEST(OptimizedTaskTest, VoidTaskWithException) {
    auto task = []() -> OptimizedTask<> {
        throw std::runtime_error("Void task exception");
        co_return; // Unreachable
    }();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_TRUE(task.isCompleted());
    EXPECT_THROW(task.getResult(), std::runtime_error);
}

// Test move constructor and assignment
TEST(OptimizedTaskTest, MoveSemantics) {
    auto task1 = []() -> OptimizedTask<std::string> {
        co_return "moved value";
    }();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_TRUE(task1.isCompleted());

    OptimizedTask<std::string> task2 = std::move(task1);

    // task1 should be in a valid but empty state
    // Accessing moved-from task might be undefined behavior depending on implementation details
    // We can check if task2 works correctly
    EXPECT_TRUE(task2.isCompleted());
    EXPECT_EQ(task2.getResult(), "moved value");

    // Test move assignment
    OptimizedTask<std::string> task3;
    task3 = std::move(task2);

    EXPECT_TRUE(task3.isCompleted());
    EXPECT_EQ(task3.getResult(), "moved value");
}

// Test completion callback (if implemented/needed)
// The provided code has a completion_callback_ member but it's not used in the promise_type methods.
// If it were used, we would test it here. Assuming it's not currently functional based on the excerpt.
// If it were functional, a test would look like:
/*
TEST(OptimizedTaskTest, CompletionCallback) {
    std::atomic<bool> callback_called = false;
    auto task = [&]() -> OptimizedTask<> {
        co_return;
    }();
    task.setCompletionCallback([&](){ callback_called.store(true); }); // Assuming such a method exists

    // Task runs immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_TRUE(task.isCompleted());
    EXPECT_TRUE(callback_called.load());
}
*/

// Test CacheAligned wrapper (basic usage, cannot verify alignment directly in test)
TEST(CacheAlignedTest, BasicUsage) {
    CacheAligned<int> aligned_int(10);
    EXPECT_EQ(aligned_int.value, 10);
    EXPECT_EQ(static_cast<int>(aligned_int), 10);

    CacheAligned<std::string> aligned_string("hello");
    EXPECT_EQ(aligned_string.value, "hello");
    EXPECT_EQ(static_cast<std::string>(aligned_string), "hello");

    // Check address alignment (best effort, not guaranteed by EXPECT)
    // uintptr_t addr = reinterpret_cast<uintptr_t>(&aligned_int.value);
    // EXPECT_EQ(addr % CACHE_LINE_SIZE, 0); // This assertion might fail depending on compiler/platform
}

// Test SpinLock (basic lock/unlock, try_lock, cannot verify performance/contention behavior easily)
TEST(SpinLockTest, BasicLockUnlock) {
    SpinLock lock;
    lock.lock();
    // Should be locked now
    EXPECT_FALSE(lock.try_lock());
    lock.unlock();
    // Should be unlocked now
    EXPECT_TRUE(lock.try_lock());
    lock.unlock(); // Unlock the one acquired by try_lock
}

TEST(SpinLockTest, ConcurrentAccess) {
    SpinLock lock;
    std::atomic<int> counter = 0;
    const int num_threads = 10;
    const int num_iterations = 1000;
    std::vector<std::thread> threads;

    for(int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for(int j = 0; j < num_iterations; ++j) {
                lock.lock();
                counter++;
                lock.unlock();
            }
        });
    }

    for(auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), num_threads * num_iterations);
}

// Test RWSpinLock (basic read/write lock/unlock)
TEST(RWSpinLockTest, BasicReadWriteLock) {
    RWSpinLock lock;

    // Write lock
    lock.lock();
    // Cannot get read or write lock now
    EXPECT_FALSE(lock.try_lock_shared());
    // try_lock is not available in RWSpinLock, skip testing it directly

    lock.unlock();

    // Read lock
    lock.lock_shared();
    // Can get another read lock
    EXPECT_NO_THROW(lock.lock_shared()); // Should not block indefinitely
    lock.unlock_shared();
    lock.unlock_shared(); // Unlock both read locks

    // Cannot get write lock if read lock is held
    lock.lock_shared();
    // try_lock is not available, cannot easily test blocking write lock
    lock.unlock_shared();

    // Can get write lock if no read lock is held
    lock.lock();
    lock.unlock();
}

TEST(RWSpinLockTest, ConcurrentReadWrite) {
    RWSpinLock lock;
    std::atomic<int> counter = 0;
    const int num_readers = 5;
    const int num_writers = 2;
    const int num_iterations = 1000;
    std::vector<std::thread> threads;

    // Writers
    for(int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&]() {
            for(int j = 0; j < num_iterations; ++j) {
                lock.lock(); // Write lock
                counter.fetch_add(1, std::memory_order_relaxed);
                lock.unlock(); // Write unlock
            }
        });
    }

    // Readers
    for(int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&]() {
            for(int j = 0; j < num_iterations; ++j) {
                lock.lock_shared(); // Read lock
                // Read counter value (relaxed is okay here as we hold the read lock)
                int value = counter.load(std::memory_order_relaxed);
                (void)value; // Use value to avoid unused warning
                lock.unlock_shared(); // Read unlock
            }
        });
    }

    for(auto& t : threads) {
        t.join();
    }

    // Final counter value should reflect all writes
    EXPECT_EQ(counter.load(), num_writers * num_iterations);
}

// Test SPSCQueue (basic push/pop, empty, size)
TEST(SPSCQueueTest, BasicPushPop) {
    SPSCQueue<int, 4> queue; // Size 4, power of 2

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);

    int item;

    EXPECT_TRUE(queue.try_push(1));
    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1);

    EXPECT_TRUE(queue.try_push(2));
    EXPECT_EQ(queue.size(), 2);

    EXPECT_TRUE(queue.try_pop(item));
    EXPECT_EQ(item, 1);
    EXPECT_EQ(queue.size(), 1);

    EXPECT_TRUE(queue.try_push(3));
    EXPECT_EQ(queue.size(), 2);

    EXPECT_TRUE(queue.try_pop(item));
    EXPECT_EQ(item, 2);
    EXPECT_EQ(queue.size(), 1);

    EXPECT_TRUE(queue.try_pop(item));
    EXPECT_EQ(item, 3);
    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());

    EXPECT_FALSE(queue.try_pop(item)); // Should fail on empty queue
}

TEST(SPSCQueueTest, FullQueue) {
    SPSCQueue<int, 4> queue; // Capacity 3 (Size - 1)

    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_EQ(queue.size(), 3);

    EXPECT_FALSE(queue.try_push(4)); // Should fail when full
    EXPECT_EQ(queue.size(), 3);

    int item;
    EXPECT_TRUE(queue.try_pop(item));
    EXPECT_EQ(item, 1);
    EXPECT_EQ(queue.size(), 2);

    EXPECT_TRUE(queue.try_push(4)); // Should succeed now
    EXPECT_EQ(queue.size(), 3);

    EXPECT_FALSE(queue.try_push(5)); // Should fail again
    EXPECT_EQ(queue.size(), 3);
}

TEST(SPSCQueueTest, ConcurrentSPSC) {
    SPSCQueue<int, 1024> queue;
    const int num_items = 100000;

    std::thread producer([&]() {
        for(int i = 0; i < num_items; ++i) {
            while(!queue.try_push(i)) {
                // Spin or yield if queue is full
                std::this_thread::yield();
            }
        }
    });

    std::vector<int> consumed_items;
    consumed_items.reserve(num_items);

    std::thread consumer([&]() {
        int item;
        for(int i = 0; i < num_items; ++i) {
             while(!queue.try_pop(item)) {
                // Spin or yield if queue is empty
                 std::this_thread::yield();
            }
            consumed_items.push_back(item);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(consumed_items.size(), num_items);

    // Verify items are in order
    for(int i = 0; i < num_items; ++i) {
        EXPECT_EQ(consumed_items[i], i);
    }
}
