// 移除未使用的头文件
#include <chrono>
// #include <functional> - 移除未使用的头文件
// #include <iomanip> - 移除未使用的头文件
#include <iostream>
#include <memory> // 添加 <memory> 以使用 std::unique_ptr
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm> // 添加 <algorithm> 以使用 std::sort

#include "atom/async/thread_wrapper.hpp"  // Include the Thread wrapper header

// Mutex for thread-safe console output
std::mutex cout_mutex;

// Helper function for thread-safe printing
template <typename... Args>
void print_safe(Args&&... args) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    (std::cout << ... << std::forward<Args>(args)) << std::endl;
}

// Helper function to print section headers
void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

// Helper function to get current thread ID as string
std::string thread_id_string() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// Simple task that can be interrupted
void interruptible_task(std::stop_token stop_token, int id, int duration_ms) {
    print_safe("Task ", id, " started on thread ", thread_id_string());

    int elapsed = 0;
    while (elapsed < duration_ms && !stop_token.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        elapsed += 100;
        print_safe("Task ", id, " progress: ", elapsed, "/", duration_ms,
                   " ms");
    }

    if (stop_token.stop_requested()) {
        print_safe("Task ", id, " was interrupted at ", elapsed, " ms");
    } else {
        print_safe("Task ", id, " completed normally");
    }
}

// Task that returns a value
int compute_task(int value) {
    print_safe("Compute task started with value ", value, " on thread ",
               thread_id_string());
    // Simulate computation
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return value * value;
}

// Task that might throw an exception
void error_prone_task(bool should_throw) {
    print_safe("Error-prone task started on thread ", thread_id_string());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (should_throw) {
        print_safe("Task is about to throw an exception!");
        throw std::runtime_error("Deliberate exception from error-prone task");
    }

    print_safe("Error-prone task completed without errors");
}

// Long-running task that checks for stop requests
void long_running_task(std::stop_token stop_token) {
    print_safe("Long-running task started on thread ", thread_id_string());

    for (int i = 1; i <= 10; ++i) {
        if (stop_token.stop_requested()) {
            print_safe("Long-running task received stop request at iteration ",
                       i);
            return;
        }

        print_safe("Long-running task iteration ", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    print_safe("Long-running task completed all iterations");
}

// Task that simulates a CPU-bound operation
void cpu_bound_task(int iterations) {
    print_safe("CPU-bound task started on thread ", thread_id_string());

    // Simulate CPU-intensive work
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1, 1000);

    std::vector<int> data(10000);

    for (int i = 0; i < iterations && i < 10; ++i) {
        print_safe("CPU-bound task iteration ", i + 1);

        // Fill vector with random numbers
        for (auto& item : data) {
            item = distrib(gen);
        }

        // Sort the vector (CPU-intensive)
        std::sort(data.begin(), data.end());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    print_safe("CPU-bound task completed after ", iterations, " iterations");
}

// Function that always throws an exception
void always_throws() {
    print_safe("This function will throw immediately");
    throw std::runtime_error("Immediate exception");
}

// C++20 coroutine-based task example (if supported)
// 注意：协程的 'return_value'/'return_void' 错误需要在 atom::async::Task 的 promise_type 定义中修复（通常在 thread_wrapper.hpp 中）。
// 此处的用法对于 Task<int> 是正确的。
#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L
atom::async::Task<int> coroutine_task() {
    print_safe("Coroutine task started on thread ", thread_id_string());

    // Simulate some asynchronous work
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    print_safe("Coroutine task step 1 completed");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    print_safe("Coroutine task step 2 completed");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    print_safe("Coroutine task completed");

    // 使用co_return返回值
    co_return 42;
}
#endif

int main() {
    using namespace std::chrono_literals;
    print_safe("Main thread ID: ", thread_id_string());

    //==============================================================
    // 1. Basic Usage
    //==============================================================
    print_section("1. Basic Usage");

    // Example 1: Simple thread creation and execution
    {
        atom::async::Thread thread;

        print_safe("Starting a simple thread...");
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([]() {
            print_safe("Hello from thread ", thread_id_string());
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            print_safe("Thread execution completed");
        });

        print_safe("Thread started with ID: ", thread.getId());
        thread.join();
        print_safe("Thread joined");
    }

    // Example 2: Thread with arguments
    {
        atom::async::Thread thread;

        print_safe("\nStarting a thread with arguments...");
        std::string message = "Hello, World!";
        int count = 3;

        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start(
            [](const std::string& msg, int repeat) {
                for (int i = 0; i < repeat; ++i) {
                    print_safe("Message ", i + 1, ": ", msg);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            },
            message, count);

        thread.join();
        print_safe("Thread with arguments completed");
    }

    // Example 3: Thread with stop token
    {
        atom::async::Thread thread;

        print_safe("\nStarting an interruptible thread...");
        // 使用 lambda 适配 interruptible_task 的参数列表以匹配 start 的预期
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中，它可能无法正确处理带 stop_token 的 lambda
        thread.start(
            [](std::stop_token st) {
                // 将额外的参数传递给 interruptible_task
                interruptible_task(st, 1, 2000);
            });

        // Let it run for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        // Request stop
        print_safe("Requesting thread to stop");
        thread.requestStop();

        thread.join();
        print_safe("Interruptible thread completed");
    }

    // Example 4: Thread with return value
    {
        atom::async::Thread thread;

        print_safe("\nStarting a thread that returns a value...");
        // 注意：如果 startWithResult 报错，问题可能在 thread_wrapper.hpp 的 startWithResult 实现中
        auto future = thread.startWithResult<int>(compute_task, 7);

        print_safe("Waiting for result...");
        try {
            int result = future.get();
            print_safe("Computation result: ", result);
        } catch (const std::exception& e) {
            print_safe("Error getting result: ", e.what());
        }
        // 确保线程在 future.get() 之后被 join（如果 startWithResult 没有自动 join）
        if (thread.joinable()) {
             thread.join();
        }
    }

    //==============================================================
    // 2. Different Parameter Combinations
    //==============================================================
    print_section("2. Different Parameter Combinations");

    // Example 1: Multiple threads with different parameters
    {
        // 使用 unique_ptr 来管理不可复制的 Thread 对象
        std::vector<std::unique_ptr<atom::async::Thread>> threads;

        print_safe("Creating multiple threads with different parameters...");

        for (int i = 0; i < 5; ++i) {
            threads.push_back(std::make_unique<atom::async::Thread>());
            // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
            threads.back()->start(
                [](int id, int delay) {
                    print_safe("Thread ", id, " started with delay ", delay,
                               "ms");
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(delay));
                    print_safe("Thread ", id, " finished");
                },
                i, (i + 1) * 200);
        }

        // Join all threads
        print_safe("Waiting for all threads to complete...");
        for (auto& t : threads) {
            t->join();
        }

        print_safe("All threads completed");
    }

    // Example 2: Thread with lambda capturing variables
    {
        atom::async::Thread thread;

        print_safe("\nStarting thread with lambda capturing variables...");

        std::vector<int> data = {1, 2, 3, 4, 5};
        int sum = 0;

        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([&sum, data]() { // 按值捕获 data 以避免生命周期问题
            print_safe("Processing ", data.size(), " elements");
            for (int val : data) {
                sum += val;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                print_safe("Running sum: ", sum);
            }
        });

        thread.join();
        print_safe("Final sum: ", sum);
    }

    // Example 3: Using a thread to run a member function
    {
        class Worker {
        public:
            void process(int iterations) {
                print_safe("Worker::process started with ", iterations,
                           " iterations on thread ", thread_id_string());
                for (int i = 0; i < iterations; ++i) {
                    count_++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    print_safe("Worker count: ", count_);
                }
                print_safe("Worker::process completed");
            }

            int getCount() const { return count_; }

        private:
            int count_ = 0;
        };

        print_safe("\nStarting thread with class member function...");

        Worker worker;
        atom::async::Thread thread;

        // 使用 lambda 捕获 worker 引用并调用其成员函数
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([&worker](int iterations) { worker.process(iterations); },
                     3);

        thread.join();
        print_safe("Worker result: ", worker.getCount());
    }

    //==============================================================
    // 3. Edge Cases and Boundary Values
    //==============================================================
    print_section("3. Edge Cases and Boundary Values");

    // Example 1: Starting a new thread when one is already running
    {
        atom::async::Thread thread;

        print_safe("Starting first thread...");
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([]() {
            print_safe("First thread running");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            print_safe("First thread ending");
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        print_safe("Starting second thread (should stop first)...");
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        // 重新启动会隐式地请求停止并加入之前的线程（假设 Thread 实现如此）
        thread.start([]() {
            print_safe("Second thread running");
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            print_safe("Second thread ending");
        });

        thread.join(); // 等待第二个线程完成
        print_safe("Thread joined");
    }

    // Example 2: Zero-duration task
    {
        atom::async::Thread thread;

        print_safe("\nStarting zero-duration task...");
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([]() {
            print_safe("Zero-duration task executed");
            // No sleep, returns immediately
        });

        thread.join();
        print_safe("Zero-duration task completed");
    }

    // Example 3: Try joining with timeout
    {
        atom::async::Thread thread;

        print_safe("\nTesting tryJoinFor with long task...");
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([]() {
            print_safe("Long task started");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            print_safe("Long task completed");
        });

        // Try to join with a short timeout
        print_safe("Trying to join with 200ms timeout");
        bool joined = thread.tryJoinFor(200ms);
        print_safe("Join result: ", joined ? "Succeeded" : "Timed out");

        // Make sure the thread completes eventually
        if (!joined) {
            thread.join();
        }
        print_safe("Thread eventually joined");
    }

    // Example 4: Thread that's already stopped
    {
        atom::async::Thread thread;

        print_safe("\nTesting operations on already completed thread...");
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([]() { print_safe("Quick task"); });

        // Make sure it completes
        thread.join();

        // Try operations on already completed thread
        print_safe("Thread running after join: ",
                   thread.running() ? "Yes" : "No");
        print_safe("Requesting stop on completed thread");
        thread.requestStop();  // Should do nothing gracefully
        print_safe("Joining already joined thread");
        thread.join();  // Should do nothing gracefully
    }

    //==============================================================
    // 4. Error Handling
    //==============================================================
    print_section("4. Error Handling");

    // Example 1: Thread with exception
    {
        atom::async::Thread thread;

        print_safe("Starting thread that might throw...");
        try {
            // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
            thread.start(error_prone_task, true);
            thread.join();  // Thread 内部应捕获异常，join 不应抛出
            print_safe("Thread completed despite internal exception (assuming Thread catches it)");
        } catch (const std::exception& e) {
            // 如果 Thread::join 重新抛出异常，则会在此处捕获
            print_safe("Caught exception from thread join (if rethrown): ", e.what());
        }
    }

    // Example 2: Thread with exception in startWithResult
    {
        atom::async::Thread thread;

        print_safe("\nTesting exception propagation with startWithResult...");
        std::future<int> future;
        try {
            // 注意：如果 startWithResult 报错，问题可能在 thread_wrapper.hpp 的 startWithResult 实现中
             future = thread.startWithResult<int>([]() -> int {
                print_safe("Task that will throw exception");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                throw std::runtime_error("Exception in task with result");
                // return 42; // Never reached
            });

            // Exception will be propagated when we call get()
            print_safe("Waiting for result...");
            int result = future.get(); // 这行会抛出异常
            print_safe("Result: ", result);  // Should not be reached
        } catch (const std::exception& e) {
            print_safe("Correctly caught exception via future.get(): ", e.what());
        }
        // 确保线程在 future.get() 之后被 join（如果 startWithResult 没有自动 join）
        if (thread.joinable()) {
             thread.join();
        }
    }

    // Example 3: Thread that throws immediately
    {
        atom::async::Thread thread;

        print_safe("\nStarting thread that throws immediately...");
        try {
            // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
            thread.start(always_throws);
            thread.join(); // 假设 Thread 内部捕获异常
             print_safe("Thread completed despite immediate internal exception (assuming Thread catches it)");
        } catch (const std::exception& e) {
            // 如果 Thread::join 重新抛出异常，则会在此处捕获
            print_safe("Caught exception from thread join (if rethrown): ", e.what());
        }
    }

    // Example 4: Handling thread stop requests
    {
        atom::async::Thread thread;

        print_safe("\nTesting proper handling of stop requests...");
        // 使用 lambda 适配 long_running_task 的参数列表
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start([](std::stop_token st) {
            long_running_task(st);
        });

        // Let it run a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Check stop token (shouldStop() 可能是 Thread 的一个方法)
        // print_safe("Stop requested before requestStop: ", thread.shouldStop() ? "Yes" : "No"); // 假设有 shouldStop()

        // Request stop
        print_safe("Requesting thread to stop");
        thread.requestStop();

        // Check stop token after request
        // print_safe("Stop requested after requestStop: ", thread.shouldStop() ? "Yes" : "No"); // 假设有 shouldStop()

        thread.join();
        print_safe("Thread joined after stop request");
    }

    //==============================================================
    // 5. Advanced Features
    //==============================================================
    print_section("5. Advanced Features");

    // Example 1: Multiple threads and thread swapping
    {
        atom::async::Thread thread1;
        atom::async::Thread thread2;

        print_safe("Starting two threads and then swapping them...");

        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread1.start(
            [](int id) {
                print_safe("Thread ", id, " started on thread ",
                           thread_id_string());
                for (int i = 0; i < 5; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    print_safe("Thread ", id, " - iteration ", i + 1);
                }
                 print_safe("Thread ", id, " finished");
            },
            1);

        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread2.start(
            [](int id) {
                print_safe("Thread ", id, " started on thread ",
                           thread_id_string());
                for (int i = 0; i < 3; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                    print_safe("Thread ", id, " - iteration ", i + 1);
                }
                 print_safe("Thread ", id, " finished");
            },
            2);

        // Let them run a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Swap the threads
        print_safe("Swapping threads");
        thread1.swap(thread2); // 假设 swap() 正确实现

        // Wait for both threads to complete using their new handles
        print_safe("Waiting for thread1 (formerly thread2)...");
        thread1.join();

        print_safe("Waiting for thread2 (formerly thread1)...");
        thread2.join();

        print_safe("Both threads completed after swap");
    }

    // Example 2: CPU-bound tasks
    {
        atom::async::Thread thread;

        print_safe("\nLaunching CPU-bound task...");
        // 注意：如果 start 仍然报错，问题可能在 thread_wrapper.hpp 的 start 实现中
        thread.start(cpu_bound_task, 5);

        print_safe("Main thread continues executing while CPU task runs");
        for (int i = 0; i < 3; ++i) {
            print_safe("Main thread work iteration ", i + 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        thread.join();
        print_safe("CPU-bound task completed");
    }

    // Example 3: Using coroutines (if available)
#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L
    {
        print_safe("\nTesting C++20 coroutine support...");
        // 注意：协程的 'return_value'/'return_void' 错误需要在 atom::async::Task 的 promise_type 定义中修复
        auto task = coroutine_task(); // 启动协程
        print_safe("Coroutine launched");

        // Main thread continues while coroutine runs (协程通常在后台线程执行，具体取决于 Task 实现)
        print_safe("Main thread continues while coroutine runs");
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 等待协程完成

        // 如果 Task 需要显式获取结果或等待完成，需要添加相应代码
        // 例如: int result = co_await task; (如果在另一个协程中)
        // 或者: task.get_result(); (如果 Task 提供了阻塞获取结果的方法)
        // 这里假设 Task 在析构时或以其他方式确保完成

        print_safe("Main thread potentially completed before coroutine finished its output");
    }
#else
    print_safe("\nC++20 coroutine support not available or disabled");
#endif

    print_safe("\nAll examples completed");
    return 0;
}