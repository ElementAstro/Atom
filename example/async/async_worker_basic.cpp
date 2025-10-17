/**
 * @file async_worker_basic.cpp
 * @brief Comprehensive demonstration of atom::async::AsyncWorker functionality
 *
 * @details This example demonstrates:
 * - Basic AsyncWorker creation and task execution
 * - Task state management (INITIAL, RUNNING, COMPLETED, CANCELLED)
 * - Result retrieval and validation
 * - Task cancellation and timeout handling
 * - Callback registration and execution
 * - Error handling and exception propagation
 * - WorkerContainer for managing multiple workers
 *
 * @level Intermediate
 * @prerequisites Basic understanding of async programming, promises/futures
 * @related_examples promise.cpp, future.cpp, async_executor.cpp
 *
 * @note AsyncWorker is a core component for async task management
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/async.hpp"

#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// UTILITY FUNCTIONS AND HELPERS
// ============================================================================

// Print mutex for thread-safe output
std::mutex print_mutex;

// Thread-safe print function with timestamp
template <typename... Args>
void print_safe(Args&&... args) {
    std::lock_guard<std::mutex> lock(print_mutex);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    (std::cout << ... << args) << std::endl;
}

// Print section divider with enhanced formatting
void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n" << std::endl;
}

// Helper function to get thread ID as string
std::string get_thread_id() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// Performance timer for measuring operations
class PerformanceTimer {
public:
    void start(const std::string& operation) {
        current_operation_ = operation;
        start_time_ = std::chrono::high_resolution_clock::now();
        print_safe("⏱️  Starting: ", operation);
    }

    void stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                            end_time - start_time_)
                            .count();

        print_safe("⏱️  Completed: ", current_operation_, " in ", duration,
                   " μs");
    }

private:
    std::string current_operation_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

// Validation helper
template <typename T>
void validate_result(const T& result, const T& expected,
                     const std::string& test_name) {
    if (result == expected) {
        print_safe("✅ ", test_name, " PASSED");
    } else {
        print_safe("❌ ", test_name, " FAILED: expected ", expected, ", got ",
                   result);
    }
}

// ============================================================================
// SECTION 1: BASIC ASYNCWORKER USAGE
// ============================================================================
/**
 * @section basic_usage Basic AsyncWorker Usage
 *
 * This section demonstrates fundamental AsyncWorker operations including:
 * - Creating AsyncWorker instances for different return types
 * - Starting async tasks and managing execution
 * - Retrieving results and checking task states
 * - Basic error handling patterns
 *
 * Key concepts:
 * - AsyncWorker<T>: Manages async execution of tasks returning type T
 * - State management: INITIAL -> RUNNING -> COMPLETED/CANCELLED
 * - Thread safety: AsyncWorker is thread-safe for state queries
 *
 * @see async_executor.cpp for higher-level task execution
 */
void basic_asyncworker_examples() {
    print_section("SECTION 1: Basic AsyncWorker Usage Examples");

    // Example 1.1: Basic integer computation
    print_safe("Example 1.1: Basic integer computation");
    {
        PerformanceTimer timer;
        timer.start("Basic AsyncWorker");

        atom::async::AsyncWorker<int> worker;

        // Check initial state
        print_safe("🔍 Initial state - isDone: ", worker.isDone(),
                   ", isActive: ", worker.isActive());

        // Start async task
        worker.startAsync([]() -> int {
            print_safe("🔧 Worker thread [", get_thread_id(), "] computing...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return 42;
        });

        print_safe("🔍 After start - isDone: ", worker.isDone(),
                   ", isActive: ", worker.isActive());

        // Wait for completion and get result
        worker.waitForCompletion();
        int result = worker.getResult();

        print_safe("🔍 After completion - isDone: ", worker.isDone(),
                   ", isActive: ", worker.isActive());
        print_safe("🏠 Result: ", result);

        validate_result(result, 42, "Basic AsyncWorker");
        timer.stop();
    }
}

// ============================================================================
// SECTION 2: ASYNCWORKER STATE MANAGEMENT
// ============================================================================
/**
 * @section state_management AsyncWorker State Management
 *
 * This section demonstrates AsyncWorker state management:
 * - Monitoring task states during execution
 * - State transitions and their meanings
 * - Thread-safe state queries
 * - State-based decision making
 *
 * Key concepts:
 * - State enum: INITIAL, RUNNING, COMPLETED, CANCELLED
 * - isDone(): Returns true when task is completed or cancelled
 * - isActive(): Returns true when task is currently running
 *
 * @see threading examples for advanced state management
 */
void state_management_examples() {
    print_section("SECTION 2: AsyncWorker State Management");

    // Example 2.1: Monitoring state transitions
    print_safe("Example 2.1: Monitoring state transitions");
    {
        PerformanceTimer timer;
        timer.start("State monitoring");

        atom::async::AsyncWorker<std::string> worker;

        // Monitor states during execution
        std::thread monitor([&worker]() {
            for (int i = 0; i < 10; ++i) {
                print_safe("📊 Monitor - isDone: ", worker.isDone(),
                           ", isActive: ", worker.isActive());
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });

        // Start task after a brief delay
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        worker.startAsync([]() -> std::string {
            print_safe("🔧 Task executing...");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return "Task completed";
        });

        worker.waitForCompletion();
        monitor.join();

        std::string result = worker.getResult();
        print_safe("🏠 Final result: ", result);

        timer.stop();
    }
}

// ============================================================================
// SECTION 3: ASYNCWORKER CANCELLATION AND TIMEOUTS
// ============================================================================
/**
 * @section cancellation AsyncWorker Cancellation and Timeouts
 *
 * This section demonstrates AsyncWorker cancellation and timeout features:
 * - Manual task cancellation
 * - Timeout-based automatic cancellation
 * - Handling cancellation in running tasks
 * - Cleanup after cancellation
 *
 * Key concepts:
 * - cancel(): Manually cancel a running task
 * - setTimeout(): Set automatic timeout for task execution
 * - Cancellation is cooperative - tasks must check for cancellation
 *
 * @see promise.cpp for promise-based cancellation patterns
 */
void cancellation_and_timeout_examples() {
    print_section("SECTION 3: AsyncWorker Cancellation and Timeouts");

    // Example 3.1: Manual cancellation
    print_safe("Example 3.1: Manual task cancellation");
    {
        PerformanceTimer timer;
        timer.start("Manual cancellation");

        atom::async::AsyncWorker<int> worker;

        // Start a long-running task
        worker.startAsync([]() -> int {
            print_safe("🔧 Long task started...");
            for (int i = 0; i < 10; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                print_safe("🔧 Task progress: ", i + 1, "/10");
            }
            return 100;
        });

        // Cancel after a short delay
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        print_safe("🚫 Cancelling task...");
        worker.cancel();

        // Check if cancelled
        print_safe("🔍 After cancel - isDone: ", worker.isDone());

        timer.stop();
    }

    // Example 3.2: Timeout-based cancellation
    print_safe("\nExample 3.2: Timeout-based cancellation");
    {
        PerformanceTimer timer;
        timer.start("Timeout cancellation");

        atom::async::AsyncWorker<std::string> worker;

        // Set timeout before starting task
        worker.setTimeout(std::chrono::seconds(1));

        worker.startAsync([]() -> std::string {
            print_safe("🔧 Task with timeout started...");
            // This task would take 2 seconds, but timeout is 1 second
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return "Should not complete";
        });

        try {
            worker.waitForCompletion();
            print_safe("❌ Task should have timed out!");
        } catch (const std::exception& e) {
            print_safe("✅ Expected timeout exception: ", e.what());
        }

        timer.stop();
    }
}

// ============================================================================
// SECTION 4: ASYNCWORKER CALLBACKS AND VALIDATION
// ============================================================================
/**
 * @section callbacks AsyncWorker Callbacks and Validation
 *
 * This section demonstrates AsyncWorker callback and validation features:
 * - Setting completion callbacks
 * - Result validation functions
 * - Error handling in callbacks
 * - Callback execution timing
 *
 * Key concepts:
 * - setCallback(): Register callback for task completion
 * - validate(): Check if result meets criteria
 * - Callbacks execute after task completion
 *
 * @see promise.cpp for promise callback patterns
 */
void callback_and_validation_examples() {
    print_section("SECTION 4: AsyncWorker Callbacks and Validation");

    // Example 4.1: Completion callbacks
    print_safe("Example 4.1: Completion callbacks");
    {
        PerformanceTimer timer;
        timer.start("Completion callbacks");

        atom::async::AsyncWorker<int> worker;

        // Set completion callback
        worker.setCallback([](int result) {
            print_safe("🔔 Callback received result: ", result);
            print_safe("🔔 Callback thread: ", get_thread_id());
        });

        worker.startAsync([]() -> int {
            print_safe("🔧 Computing result...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return 42;
        });

        worker.waitForCompletion();
        int result = worker.getResult();
        print_safe("🏠 Main thread result: ", result);

        timer.stop();
    }

    // Example 4.2: Result validation
    print_safe("\nExample 4.2: Result validation");
    {
        PerformanceTimer timer;
        timer.start("Result validation");

        atom::async::AsyncWorker<int> worker;

        worker.startAsync([]() -> int { return 42; });

        worker.waitForCompletion();

        // Validate result meets criteria
        bool isValid =
            worker.validate([](int value) { return value > 0 && value < 100; });

        print_safe("🔍 Result validation: ", isValid ? "PASSED" : "FAILED");

        timer.stop();
    }
}

// Main function
int main() {
    try {
        std::cout << "====== AsyncWorker Usage Examples ======" << std::endl;

        basic_asyncworker_examples();
        state_management_examples();
        cancellation_and_timeout_examples();
        callback_and_validation_examples();

        std::cout << "\n====== All AsyncWorker Examples Completed ======"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown unhandled exception in main" << std::endl;
        return 1;
    }

    return 0;
}
