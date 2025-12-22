/**
 * @file async_worker_advanced.cpp
 * @brief Advanced AsyncWorker patterns and AsyncWorkerManager usage
 *
 * @details This example demonstrates:
 * - AsyncWorkerManager for managing multiple AsyncWorkers
 * - Batch task execution and coordination
 * - Advanced error handling and recovery patterns
 * - Complex task dependencies and chaining
 * - Resource management and cleanup
 * - Performance optimization techniques
 *
 * @level Advanced
 * @prerequisites async_worker_basic.cpp, understanding of RAII
 * @related_examples promise.cpp, async_executor.cpp, parallel.cpp
 *
 * @note Demonstrates enterprise-level async task management
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/async.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// UTILITY FUNCTIONS AND HELPERS
// ============================================================================

// Print mutex for thread-safe outputstd::mutex print_mutex;

// Thread-safe print function with timestamptemplate <typename... Args>
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

// Print section dividervoid print_section(const std::string& title) {
std::lock_guard<std::mutex> lock(print_mutex);
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n" << std::endl;
}

// Performance timerclass PerformanceTimer {
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

    print_safe("⏱️  Completed: ", current_operation_, " in ", duration, " μs");
}

private:
std::string current_operation_;
std::chrono::high_resolution_clock::time_point start_time_;
}
;

// Random number generator for testingclass RandomGenerator {
public:
RandomGenerator() : gen_(rd_()) {}

int getInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen_);
}

double getDouble(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(gen_);
}

private:
std::random_device rd_;
std::mt19937 gen_;
}
;

// ============================================================================
// SECTION 1: ASYNCWORKERMANAGER BASICS
// ============================================================================
/**
 * @section worker_manager AsyncWorkerManager for Multiple Workers
 *
 * This section demonstrates AsyncWorkerManager usage for managing multiple
 * AsyncWorker instances:
 * - Creating and managing multiple workers
 * - Batch task execution
 * - Coordinated worker lifecycle management
 * - Resource cleanup and error handling
 *
 * Key concepts:
 * - AsyncWorkerManager<T>: Manages multiple AsyncWorker<T> instances
 * - Batch operations: Execute multiple tasks concurrently
 * - Resource management: Automatic cleanup of worker resources
 *
 * @see async_executor.cpp for alternative task execution approaches
 */
void worker_container_examples() {
    print_section("SECTION 1: AsyncWorkerManager for Multiple Workers");

    // Example 1.1: Basic AsyncWorkerManager usage
    print_safe("Example 1.1: Basic AsyncWorkerManager usage");
    {
        PerformanceTimer timer;
        timer.start("AsyncWorkerManager basics");

        atom::async::AsyncWorkerManager<int> container;
        RandomGenerator rng;

        // Create multiple workers with different tasks
        std::vector<std::shared_ptr<atom::async::AsyncWorker<int>>> workers;

        for (int i = 0; i < 5; ++i) {
            auto worker = container.createWorker([i, &rng]() -> int {
                int delay = rng.getInt(50, 200);
                print_safe("🔧 Worker ", i, " processing for ", delay, "ms");
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                return i * 10;
            });
            workers.push_back(worker);
        }

        // Wait for all workers to complete
        print_safe("🏠 Waiting for all workers to complete...");
        for (auto& worker : workers) {
            worker->waitForCompletion();
            int result = worker->getResult();
            print_safe("🏠 Worker result: ", result);
        }

        timer.stop();
    }
}

// ============================================================================
// SECTION 2: ADVANCED ERROR HANDLING
// ============================================================================
/**
 * @section error_handling Advanced Error Handling Patterns
 *
 * This section demonstrates sophisticated error handling:
 * - Exception propagation through workers
 * - Error recovery and retry mechanisms
 * - Graceful degradation strategies
 * - Resource cleanup on failure
 *
 * Key concepts:
 * - Exception safety: Workers properly propagate exceptions
 * - Error recovery: Implementing retry logic
 * - Resource management: RAII for cleanup
 *
 * @see error_handling_patterns.cpp for comprehensive error strategies
 */
void advanced_error_handling_examples() {
    print_section("SECTION 2: Advanced Error Handling Patterns");

    // Example 2.1: Exception handling and recovery
    print_safe("Example 2.1: Exception handling and recovery");
    {
        PerformanceTimer timer;
        timer.start("Error handling");

        atom::async::AsyncWorker<int> worker;
        RandomGenerator rng;

        // Task that might fail
        worker.startAsync([&rng]() -> int {
            int chance = rng.getInt(1, 100);
            print_safe("🔧 Task running, failure chance: ", chance, "%");

            if (chance <= 30) {  // 30% chance of failure
                throw std::runtime_error("Simulated task failure");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return 42;
        });

        try {
            worker.waitForCompletion();
            int result = worker.getResult();
            print_safe("✅ Task succeeded with result: ", result);
        } catch (const std::exception& e) {
            print_safe("❌ Task failed with exception: ", e.what());
            print_safe("🔄 Implementing recovery strategy...");

            // Recovery: Create new worker with simpler task
            atom::async::AsyncWorker<int> recoveryWorker;
            recoveryWorker.startAsync([]() -> int {
                print_safe("🔧 Recovery task executing...");
                return -1;  // Default/fallback value
            });

            recoveryWorker.waitForCompletion();
            int fallbackResult = recoveryWorker.getResult();
            print_safe("🔄 Recovery completed with result: ", fallbackResult);
        }

        timer.stop();
    }
}

// ============================================================================
// SECTION 3: TASK DEPENDENCIES AND CHAINING
// ============================================================================
/**
 * @section task_chaining Task Dependencies and Chaining
 *
 * This section demonstrates complex task coordination:
 * - Sequential task dependencies
 * - Parallel task execution with synchronization
 * - Result passing between dependent tasks
 * - Complex workflow orchestration
 *
 * Key concepts:
 * - Task dependencies: Tasks that depend on other task results
 * - Workflow orchestration: Managing complex task relationships
 * - Data flow: Passing results between tasks
 *
 * @see parallel.cpp for parallel execution patterns
 */
void task_dependency_examples() {
    print_section("SECTION 3: Task Dependencies and Chaining");

    // Example 3.1: Sequential task chain
    print_safe("Example 3.1: Sequential task chain");
    {
        PerformanceTimer timer;
        timer.start("Task chaining");

        // Task 1: Generate initial data
        atom::async::AsyncWorker<int> task1;
        task1.startAsync([]() -> int {
            print_safe("🔧 Task 1: Generating initial data...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return 10;
        });

        task1.waitForCompletion();
        int result1 = task1.getResult();
        print_safe("📊 Task 1 result: ", result1);

        // Task 2: Process data from Task 1
        atom::async::AsyncWorker<int> task2;
        task2.startAsync([result1]() -> int {
            print_safe("🔧 Task 2: Processing data from Task 1 (", result1,
                       ")");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return result1 * 2;
        });

        task2.waitForCompletion();
        int result2 = task2.getResult();
        print_safe("📊 Task 2 result: ", result2);

        // Task 3: Finalize with results from Task 2
        atom::async::AsyncWorker<std::string> task3;
        task3.startAsync([result2]() -> std::string {
            print_safe("🔧 Task 3: Finalizing with result ", result2);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Final result: " + std::to_string(result2);
        });

        task3.waitForCompletion();
        std::string finalResult = task3.getResult();
        print_safe("🎯 Final result: ", finalResult);

        timer.stop();
    }

    // Example 3.2: Parallel tasks with synchronization
    print_safe("\nExample 3.2: Parallel tasks with synchronization");
    {
        PerformanceTimer timer;
        timer.start("Parallel synchronization");

        // Start multiple parallel tasks
        std::vector<std::shared_ptr<atom::async::AsyncWorker<int>>>
            parallelTasks;

        for (int i = 0; i < 3; ++i) {
            auto worker = std::make_shared<atom::async::AsyncWorker<int>>();
            worker->startAsync([i]() -> int {
                int delay = (i + 1) * 100;
                print_safe("🔧 Parallel task ", i, " running for ", delay,
                           "ms");
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                return (i + 1) * 10;
            });
            parallelTasks.push_back(worker);
        }

        // Wait for all parallel tasks and collect results
        std::vector<int> results;
        for (auto& task : parallelTasks) {
            task->waitForCompletion();
            results.push_back(task->getResult());
        }

        // Aggregation task using all parallel results
        atom::async::AsyncWorker<int> aggregator;
        aggregator.startAsync([results]() -> int {
            print_safe("🔧 Aggregating results...");
            int sum = std::accumulate(results.begin(), results.end(), 0);
            print_safe("🔧 Sum of parallel results: ", sum);
            return sum;
        });

        aggregator.waitForCompletion();
        int aggregatedResult = aggregator.getResult();
        print_safe("🎯 Aggregated result: ", aggregatedResult);

        timer.stop();
    }
}

// ============================================================================
// SECTION 4: PERFORMANCE OPTIMIZATION
// ============================================================================
/**
 * @section performance Performance Optimization Techniques
 *
 * This section demonstrates performance optimization strategies:
 * - Worker pooling and reuse
 * - Memory management optimization
 * - CPU-intensive vs I/O-intensive task handling
 * - Load balancing strategies
 *
 * Key concepts:
 * - Resource pooling: Reusing workers to reduce overhead
 * - Memory optimization: Minimizing allocations
 * - Task classification: Different strategies for different workloads
 *
 * @see pool.cpp for thread pool optimization strategies
 */
void performance_optimization_examples() {
    print_section("SECTION 4: Performance Optimization Techniques");

    // Example 4.1: Worker reuse pattern
    print_safe("Example 4.1: Worker reuse for performance");
    {
        PerformanceTimer timer;
        timer.start("Worker reuse");

        // Reuse the same worker for multiple tasks
        atom::async::AsyncWorker<int> reusableWorker;

        for (int i = 0; i < 5; ++i) {
            reusableWorker.startAsync([i]() -> int {
                print_safe("🔧 Reused worker executing task ", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return i * i;
            });

            reusableWorker.waitForCompletion();
            int result = reusableWorker.getResult();
            print_safe("📊 Task ", i, " result: ", result);
        }

        timer.stop();
    }
}

// Main functionint main() {
try {
    std::cout << "====== Advanced AsyncWorker Examples ======" << std::endl;

    worker_container_examples();
    advanced_error_handling_examples();
    task_dependency_examples();
    performance_optimization_examples();

    std::cout << "\n====== All Advanced Examples Completed ======" << std::endl;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
    return 1;
} catch (...) {
    std::cerr << "Unknown unhandled exception in main" << std::endl;
    return 1;
}

return 0;
}
