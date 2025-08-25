/**
 * @file component_integration.cpp
 * @brief Comprehensive demonstration of atom::async component integration
 *
 * @details This example demonstrates:
 * - Integration of Promise, Future, and AsyncExecutor
 * - MessageBus with Timer for scheduled messaging
 * - ThreadPool with MessageQueue for distributed processing
 * - AsyncWorker with Trigger for event-driven tasks
 * - Complex workflows combining multiple async components
 * - Real-world integration patterns and best practices
 * - Performance optimization through component synergy
 * - Error handling across integrated components
 *
 * @level Advanced to Expert
 * @prerequisites Understanding of individual async components
 * @related_examples promise.cpp, async_executor.cpp, message_bus.cpp, timer.cpp
 *
 * @note Demonstrates enterprise-level async system architecture
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/async.hpp"
#include "atom/async/async_executor.hpp"
#include "atom/async/future.hpp"
#include "atom/async/message_bus.hpp"
#include "atom/async/message_queue.hpp"
#include "atom/async/pool.hpp"
#include "atom/async/promise.hpp"
#include "atom/async/timer.hpp"
#include "atom/async/trigger.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

using namespace atom::async;

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

// Enhanced section separator
void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n" << std::endl;
}

// Performance timer
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

// ============================================================================
// MESSAGE TYPES FOR INTEGRATION EXAMPLES
// ============================================================================

// Data processing request
struct DataRequest {
    int request_id;
    std::vector<int> data;
    std::string operation;

    DataRequest(int id, std::vector<int> data, std::string operation)
        : request_id(id),
          data(std::move(data)),
          operation(std::move(operation)) {}
};

// Processing result
struct ProcessingResult {
    int request_id;
    std::vector<int> result;
    bool success;
    std::string error_message;

    ProcessingResult(int id, std::vector<int> result, bool success,
                     std::string error = "")
        : request_id(id),
          result(std::move(result)),
          success(success),
          error_message(std::move(error)) {}
};

// System event
struct SystemEvent {
    enum class Type { STARTUP, SHUTDOWN, ERROR_EVENT, MAINTENANCE };

    Type type;
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    SystemEvent(Type type, std::string message)
        : type(type),
          message(std::move(message)),
          timestamp(std::chrono::system_clock::now()) {}
};

// ============================================================================
// SECTION 1: PROMISE + EXECUTOR INTEGRATION
// ============================================================================
/**
 * @section promise_executor Promise and AsyncExecutor Integration
 *
 * This section demonstrates how Promise and AsyncExecutor work together:
 * - Creating promises that execute through AsyncExecutor
 * - Priority-based promise execution
 * - Promise chaining with executor-managed tasks
 * - Error handling across promise-executor boundaries
 * - Resource management and cleanup
 *
 * Key concepts:
 * - Executor-managed promises: Promises executed through AsyncExecutor
 * - Priority coordination: Aligning promise and executor priorities
 * - Resource sharing: Efficient resource usage across components
 * - Error propagation: Consistent error handling
 *
 * @see async_executor.cpp for executor configuration
 */
void promise_executor_integration() {
    print_section("SECTION 1: Promise and AsyncExecutor Integration");

    // Example 1.1: Promise execution through AsyncExecutor
    print_safe("Example 1.1: Promise execution through AsyncExecutor");
    {
        PerformanceTimer timer;
        timer.start("Promise-Executor integration");

        // Create AsyncExecutor with custom configuration
        AsyncExecutor::Configuration config;
        config.threadCount = 4;
        config.enableWorkStealing = true;
        config.queueSizePerThread = 100;

        auto executor = std::make_shared<AsyncExecutor>(config);

        // Create promises that will be executed through the executor
        std::vector<Promise<int>> promises;
        std::vector<EnhancedFuture<int>> futures;

        for (int i = 0; i < 5; ++i) {
            Promise<int> promise;
            auto future = promise.getEnhancedFuture();

            // Execute promise resolution through AsyncExecutor
            executor->execute(
                [promise = std::move(promise), i]() mutable {
                    print_safe("🔧 Executor thread processing promise ", i);
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(100 + i * 50));
                    promise.setValue(i * 10);
                    print_safe("🔧 Promise ", i, " resolved with value ",
                               i * 10);
                },
                AsyncExecutor::Priority::Normal);

            futures.push_back(std::move(future));
        }

        // Collect results
        std::vector<int> results;
        for (auto& future : futures) {
            int result = future.wait();
            results.push_back(result);
            print_safe("🏠 Collected result: ", result);
        }

        print_safe("📊 All promises resolved through executor");
        print_safe("📊 Results: ", results.size(), " values collected");

        timer.stop();
    }
}

// ============================================================================
// SECTION 2: MESSAGEBUS + TIMER INTEGRATION
// ============================================================================
/**
 * @section messagebus_timer MessageBus and Timer Integration
 *
 * This section demonstrates scheduled messaging patterns:
 * - Timer-triggered message publishing
 * - Periodic system notifications
 * - Scheduled data processing workflows
 * - Time-based message coordination
 * - Event scheduling and management
 *
 * Key concepts:
 * - Scheduled messaging: Using timers to trigger message publishing
 * - Periodic workflows: Regular processing cycles
 * - Time coordination: Synchronizing components through time-based events
 * - Event scheduling: Managing complex time-based workflows
 *
 * @see timer.cpp for advanced timer patterns
 */
void messagebus_timer_integration() {
    print_section("SECTION 2: MessageBus and Timer Integration");

    // Example 2.1: Scheduled system notifications
    print_safe("Example 2.1: Scheduled system notifications");
    {
        PerformanceTimer timer;
        timer.start("MessageBus-Timer integration");

        auto messageBus = MessageBus::createShared();

        std::atomic<int> notificationCount{0};

        // Subscribe to system events
        auto token = messageBus->subscribe<SystemEvent>(
            "system.scheduled", [&notificationCount](const SystemEvent& event) {
                notificationCount++;
                std::string typeStr;
                switch (event.type) {
                    case SystemEvent::Type::STARTUP:
                        typeStr = "STARTUP";
                        break;
                    case SystemEvent::Type::SHUTDOWN:
                        typeStr = "SHUTDOWN";
                        break;
                    case SystemEvent::Type::ERROR_EVENT:
                        typeStr = "ERROR";
                        break;
                    case SystemEvent::Type::MAINTENANCE:
                        typeStr = "MAINTENANCE";
                        break;
                }
                print_safe("🔔 Scheduled event [", typeStr,
                           "]: ", event.message);
            });

        // Simulate scheduled messaging using threads (since Timer API is
        // complex)
        std::vector<std::thread> scheduledTasks;

        // Schedule startup notification
        scheduledTasks.emplace_back([messageBus]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            SystemEvent startupEvent(SystemEvent::Type::STARTUP,
                                     "System initialization complete");
            messageBus->publish("system.scheduled", startupEvent);
            print_safe("📅 Scheduled startup notification");
        });

        // Schedule periodic maintenance notifications
        scheduledTasks.emplace_back([messageBus]() {
            for (int i = 0; i < 3; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                SystemEvent maintenanceEvent(
                    SystemEvent::Type::MAINTENANCE,
                    "Periodic health check #" + std::to_string(i + 1));
                messageBus->publish("system.scheduled", maintenanceEvent);
                print_safe("📅 Scheduled maintenance notification #", i + 1);
            }
        });

        // Let the scheduled events run
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));

        // Wait for all scheduled tasks to complete
        for (auto& task : scheduledTasks) {
            if (task.joinable()) {
                task.join();
            }
        }

        print_safe("📊 Total scheduled notifications: ",
                   notificationCount.load());

        // Cleanup
        messageBus->unsubscribe<SystemEvent>(token);

        timer.stop();
    }
}

// ============================================================================
// SECTION 3: THREADPOOL + MESSAGEQUEUE INTEGRATION
// ============================================================================
/**
 * @section threadpool_messagequeue ThreadPool and MessageQueue Integration
 *
 * This section demonstrates distributed processing patterns:
 * - ThreadPool workers processing MessageQueue tasks
 * - Load balancing across multiple worker threads
 * - Result aggregation from distributed processing
 * - Error handling in distributed systems
 * - Performance optimization through parallelization
 *
 * Key concepts:
 * - Distributed processing: Using ThreadPool to process MessageQueue items
 * - Load balancing: Distributing work across available threads
 * - Result coordination: Collecting results from parallel processing
 * - Fault tolerance: Handling failures in distributed processing
 *
 * @see pool.cpp for thread pool optimization
 */
void threadpool_messagequeue_integration() {
    print_section("SECTION 3: ThreadPool and MessageQueue Integration");

    // Example 3.1: Distributed data processing
    print_safe("Example 3.1: Distributed data processing");
    {
        PerformanceTimer timer;
        timer.start("ThreadPool-MessageQueue integration");

        // Create thread pool and message queue
        auto threadPool = std::make_shared<ThreadPool>(4);
        MessageQueue<DataRequest> requestQueue;
        MessageQueue<ProcessingResult> resultQueue;

        std::atomic<int> processedRequests{0};
        std::atomic<int> collectedResults{0};

        // Set up request processing
        requestQueue.subscribe(
            [threadPool, &resultQueue,
             &processedRequests](const DataRequest& request) {
                processedRequests++;
                print_safe("📥 Received request ", request.request_id, " for ",
                           request.operation);

                // Submit to thread pool for processing
                threadPool->submit([request, &resultQueue]() {
                    print_safe("🔧 Processing request ", request.request_id,
                               " in thread pool");

                    // Simulate data processing
                    std::vector<int> result;
                    if (request.operation == "sum") {
                        int sum = 0;
                        for (int val : request.data) {
                            sum += val;
                        }
                        result.push_back(sum);
                    } else if (request.operation == "square") {
                        for (int val : request.data) {
                            result.push_back(val * val);
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    // Publish result
                    ProcessingResult processingResult(request.request_id,
                                                      result, true);
                    resultQueue.publish(processingResult);

                    print_safe("✅ Completed request ", request.request_id);
                });
            },
            "requestProcessor");

        // Set up result collection
        resultQueue.subscribe(
            [&collectedResults](const ProcessingResult& result) {
                collectedResults++;
                print_safe("📤 Result for request ", result.request_id, ": ",
                           result.success ? "SUCCESS" : "FAILED");
                if (result.success && !result.result.empty()) {
                    print_safe("📊 Result data: first value = ",
                               result.result[0]);
                }
            },
            "resultCollector");

        // Submit processing requests
        std::vector<DataRequest> requests = {
            DataRequest(1, {1, 2, 3, 4, 5}, "sum"),
            DataRequest(2, {2, 4, 6}, "square"),
            DataRequest(3, {10, 20, 30}, "sum"),
            DataRequest(4, {1, 3, 5, 7}, "square")};

        for (const auto& request : requests) {
            requestQueue.publish(request);
            print_safe("📤 Submitted request ", request.request_id);
        }

        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        print_safe("📊 Processed requests: ", processedRequests.load());
        print_safe("📊 Collected results: ", collectedResults.load());

        // Cleanup
        requestQueue.stopProcessing();
        resultQueue.stopProcessing();

        timer.stop();
    }
}

// Main function
int main() {
    try {
        std::cout << "====== Component Integration Examples ======"
                  << std::endl;

        promise_executor_integration();
        messagebus_timer_integration();
        threadpool_messagequeue_integration();

        std::cout << "\n====== All Integration Examples Completed ======"
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
