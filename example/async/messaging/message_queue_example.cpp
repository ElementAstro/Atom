/**
 * @file message_queue.cpp
 * @brief Comprehensive demonstration of atom::async::MessageQueue functionality
 *
 * @details This example demonstrates:
 * - Priority-based message queuing and processing
 * - Message filtering and selective processing
 * - Subscriber management with different priorities
 * - Queue configuration and optimization
 * - Timeout handling and message expiration
 * - Batch message processing patterns
 * - Queue monitoring and statistics
 * - Advanced queue management techniques
 *
 * @level Intermediate to Advanced
 * @prerequisites Basic understanding of queues, async programming
 * @related_examples message_bus.cpp, queue.cpp, async_executor.cpp
 *
 * @note Demonstrates enterprise-level message queue patterns
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/message_queue.hpp"

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

#ifdef ATOM_USE_ASIO
#include <asio/io_context.hpp>
#endif

using namespace atom::async;

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

// Enhanced section separatorvoid print_section(const std::string& title) {
std::lock_guard<std::mutex> lock(print_mutex);
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n" << std::endl;
}

// Helper function to get thread ID as stringstd::string get_thread_id() {
std::stringstream ss;
ss << std::this_thread::get_id();
return ss.str();
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

// ============================================================================
// MESSAGE TYPES FOR EXAMPLES
// ============================================================================

// Priority message typestruct PriorityMessage {
enum class Priority { LOW = 1, NORMAL = 2, HIGH = 3, CRITICAL = 4 };

std::string content;
Priority priority;
int id;
std::chrono::system_clock::time_point timestamp;

PriorityMessage(std::string content, Priority priority, int id)
    : content(std::move(content)),
      priority(priority),
      id(id),
      timestamp(std::chrono::system_clock::now()) {}
}
;

// Task message for processingstruct TaskMessage {
std::string task_type;
std::vector<int> data;
int batch_id;
std::chrono::milliseconds processing_time;

TaskMessage(std::string task_type, std::vector<int> data, int batch_id,
            std::chrono::milliseconds processing_time)
    : task_type(std::move(task_type)),
      data(std::move(data)),
      batch_id(batch_id),
      processing_time(processing_time) {}
}
;

// Notification messagestruct NotificationMessage {
enum class Type { INFO, WARNING, ERROR_TYPE, ALERT };

Type type;
std::string message;
std::string source;
bool urgent;

NotificationMessage(Type type, std::string message, std::string source,
                    bool urgent = false)
    : type(type),
      message(std::move(message)),
      source(std::move(source)),
      urgent(urgent) {}
}
;

// ============================================================================
// SECTION 1: BASIC MESSAGE QUEUE OPERATIONS
// ============================================================================
/**
 * @section basic_queue Basic Message Queue Operations
 *
 * This section demonstrates fundamental MessageQueue operations:
 * - Creating message queues with different configurations
 * - Basic message publishing and subscription
 * - Message filtering and selective processing
 * - Queue monitoring and statistics
 * - Subscriber management
 *
 * Key concepts:
 * - MessageQueue<T>: Type-safe message queue for specific message types
 * - subscribe(): Register handlers with optional filters and priorities
 * - publish(): Add messages to the queue for processing
 * - Message filtering: Process only messages that meet specific criteria
 *
 * @see message_bus.cpp for publish-subscribe patterns
 */
void basic_queue_operations() {
    print_section("SECTION 1: Basic Message Queue Operations");

#ifdef ATOM_USE_ASIO
    asio::io_context io_context;
    MessageQueue<NotificationMessage> messageQueue(io_context);
    std::thread processingThread([&io_context]() { io_context.run(); });
#else
    MessageQueue<NotificationMessage> messageQueue;
#endif

    // Example 1.1: Basic subscription and publishing
    print_safe("Example 1.1: Basic subscription and publishing");
    {
        PerformanceTimer timer;
        timer.start("Basic queue operations");

        std::atomic<int> processedCount{0};

        // Subscribe to all notifications
        messageQueue.subscribe(
            [&processedCount](const NotificationMessage& notification) {
                processedCount++;
                std::string typeStr;
                switch (notification.type) {
                    case NotificationMessage::Type::INFO:
                        typeStr = "INFO";
                        break;
                    case NotificationMessage::Type::WARNING:
                        typeStr = "WARNING";
                        break;
                    case NotificationMessage::Type::ERROR_TYPE:
                        typeStr = "ERROR";
                        break;
                    case NotificationMessage::Type::ALERT:
                        typeStr = "ALERT";
                        break;
                }
                print_safe("🔔 [", typeStr, "] from ", notification.source,
                           ": ", notification.message);
                print_safe("🔔 Handler thread: ", get_thread_id());
            },
            "basicSubscriber",
            1  // Priority
        );

        // Publish various notifications
        std::vector<NotificationMessage> notifications = {
            NotificationMessage(NotificationMessage::Type::INFO,
                                "System started", "Core"),
            NotificationMessage(NotificationMessage::Type::WARNING,
                                "High CPU usage", "Monitor"),
            NotificationMessage(NotificationMessage::Type::ERROR_TYPE,
                                "Database error", "DB"),
            NotificationMessage(NotificationMessage::Type::ALERT,
                                "Security breach", "Security", true)};

        for (const auto& notification : notifications) {
            messageQueue.publish(notification);
            print_safe("📤 Published notification from ", notification.source);
        }

        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        print_safe("📊 Queue message count: ", messageQueue.getMessageCount());
        print_safe("📊 Subscriber count: ", messageQueue.getSubscriberCount());
        print_safe("📊 Processed messages: ", processedCount.load());

        timer.stop();
    }

#ifdef ATOM_USE_ASIO
    io_context.stop();
    processingThread.join();
#endif
}

// ============================================================================
// SECTION 2: PRIORITY-BASED MESSAGE PROCESSING
// ============================================================================
/**
 * @section priority_processing Priority-based Message Processing
 *
 * This section demonstrates priority-based message handling:
 * - Multiple subscribers with different priorities
 * - Priority-based message ordering
 * - High-priority message fast-tracking
 * - Priority-aware filtering
 * - Load balancing across priority levels
 *
 * Key concepts:
 * - Subscriber priority: Higher priority subscribers process messages first
 * - Message priority: Messages can have inherent priority levels
 * - Priority queuing: Messages processed in priority order
 * - Priority filtering: Different filters for different priority levels
 *
 * @see async_executor.cpp for priority-based task execution
 */
void priority_processing_examples() {
    print_section("SECTION 2: Priority-based Message Processing");

#ifdef ATOM_USE_ASIO
    asio::io_context io_context;
    MessageQueue<PriorityMessage> messageQueue(io_context);
    std::thread processingThread([&io_context]() { io_context.run(); });
#else
    MessageQueue<PriorityMessage> messageQueue;
#endif

    // Example 2.1: Multiple subscribers with different priorities
    print_safe("Example 2.1: Multiple subscribers with different priorities");
    {
        PerformanceTimer timer;
        timer.start("Priority processing");

        std::atomic<int> highPriorityCount{0}, normalPriorityCount{0},
            lowPriorityCount{0};

        // High priority subscriber (priority 3)
        messageQueue.subscribe(
            [&highPriorityCount](const PriorityMessage& message) {
                highPriorityCount++;
                std::string priorityStr;
                switch (message.priority) {
                    case PriorityMessage::Priority::LOW:
                        priorityStr = "LOW";
                        break;
                    case PriorityMessage::Priority::NORMAL:
                        priorityStr = "NORMAL";
                        break;
                    case PriorityMessage::Priority::HIGH:
                        priorityStr = "HIGH";
                        break;
                    case PriorityMessage::Priority::CRITICAL:
                        priorityStr = "CRITICAL";
                        break;
                }
                print_safe("🔴 HIGH_PRIORITY_HANDLER [", priorityStr,
                           "] ID:", message.id, " - ", message.content);
            },
            "highPrioritySubscriber",
            3,  // High subscriber priority
            [](const PriorityMessage& message) {
                // Only process HIGH and CRITICAL messages
                return message.priority >= PriorityMessage::Priority::HIGH;
            });

        // Normal priority subscriber (priority 2)
        messageQueue.subscribe(
            [&normalPriorityCount](const PriorityMessage& message) {
                normalPriorityCount++;
                print_safe("🟡 NORMAL_PRIORITY_HANDLER ID:", message.id, " - ",
                           message.content);
            },
            "normalPrioritySubscriber",
            2,  // Normal subscriber priority
            [](const PriorityMessage& message) {
                // Process NORMAL and HIGH messages
                return message.priority >= PriorityMessage::Priority::NORMAL;
            });

        // Low priority subscriber (priority 1)
        messageQueue.subscribe(
            [&lowPriorityCount](const PriorityMessage& message) {
                lowPriorityCount++;
                print_safe("🟢 LOW_PRIORITY_HANDLER ID:", message.id, " - ",
                           message.content);
            },
            "lowPrioritySubscriber",
            1,  // Low subscriber priority
            [](const PriorityMessage& message) {
                // Process all messages
                return true;
            });

        // Publish messages with different priorities
        std::vector<PriorityMessage> messages = {
            PriorityMessage("Low priority task", PriorityMessage::Priority::LOW,
                            1),
            PriorityMessage("Critical system alert",
                            PriorityMessage::Priority::CRITICAL, 2),
            PriorityMessage("Normal operation",
                            PriorityMessage::Priority::NORMAL, 3),
            PriorityMessage("High priority request",
                            PriorityMessage::Priority::HIGH, 4),
            PriorityMessage("Another low priority task",
                            PriorityMessage::Priority::LOW, 5)};

        for (const auto& message : messages) {
            messageQueue.publish(message);
            print_safe("📤 Published message ID:", message.id,
                       " with priority level");
        }

        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        print_safe("📊 High priority handler processed: ",
                   highPriorityCount.load(), " messages");
        print_safe("📊 Normal priority handler processed: ",
                   normalPriorityCount.load(), " messages");
        print_safe("📊 Low priority handler processed: ",
                   lowPriorityCount.load(), " messages");

        timer.stop();
    }

#ifdef ATOM_USE_ASIO
    io_context.stop();
    processingThread.join();
#endif
}

// ============================================================================
// SECTION 3: ADVANCED QUEUE MANAGEMENT
// ============================================================================
/**
 * @section advanced_management Advanced Queue Management
 *
 * This section demonstrates advanced queue management features:
 * - Message cancellation and selective removal
 * - Timeout handling and message expiration
 * - Batch processing patterns
 * - Queue monitoring and statistics
 * - Dynamic subscriber management
 *
 * Key concepts:
 * - cancelMessages(): Remove messages based on criteria
 * - Timeout handling: Messages with expiration times
 * - Batch processing: Processing multiple messages together
 * - Queue statistics: Monitoring queue health and performance
 *
 * @see pool.cpp for batch processing patterns
 */
void advanced_queue_management() {
    print_section("SECTION 3: Advanced Queue Management");

#ifdef ATOM_USE_ASIO
    asio::io_context io_context;
    MessageQueue<TaskMessage> messageQueue(io_context);
    std::thread processingThread([&io_context]() { io_context.run(); });
#else
    MessageQueue<TaskMessage> messageQueue;
#endif

    // Example 3.1: Message cancellation and timeout handling
    print_safe("Example 3.1: Message cancellation and timeout handling");
    {
        PerformanceTimer timer;
        timer.start("Advanced queue management");

        std::atomic<int> processedTasks{0};
        std::atomic<int> cancelledTasks{0};

        // Subscribe to task messages with timeout
        messageQueue.subscribe(
            [&processedTasks](const TaskMessage& task) {
                processedTasks++;
                print_safe("🔧 Processing ", task.task_type, " task (batch ",
                           task.batch_id, ") with ", task.data.size(),
                           " items");

                // Simulate processing time
                std::this_thread::sleep_for(task.processing_time);

                print_safe("✅ Completed ", task.task_type, " task (batch ",
                           task.batch_id, ")");
            },
            "taskProcessor",
            1,  // Priority
            [](const TaskMessage& task) {
                // Only process tasks with reasonable data size
                return task.data.size() <= 1000;
            },
            std::chrono::milliseconds(500)  // Timeout
        );

        // Publish various task messages
        std::vector<TaskMessage> tasks = {
            TaskMessage("data_analysis", {1, 2, 3, 4, 5}, 1,
                        std::chrono::milliseconds(50)),
            TaskMessage("image_processing", {10, 20, 30}, 2,
                        std::chrono::milliseconds(100)),
            TaskMessage(
                "large_computation", std::vector<int>(2000, 1), 3,
                std::chrono::milliseconds(200)),  // Will be filtered out
            TaskMessage("quick_task", {100}, 4, std::chrono::milliseconds(25)),
            TaskMessage("slow_task", {1, 2}, 5,
                        std::chrono::milliseconds(600))  // Will timeout
        };

        for (const auto& task : tasks) {
            messageQueue.publish(task);
            print_safe("📤 Published ", task.task_type, " task (batch ",
                       task.batch_id, ")");
        }

        // Wait for initial processing
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Cancel specific messages
        size_t cancelled =
            messageQueue.cancelMessages([](const TaskMessage& task) {
                return task.task_type == "slow_task";
            });

        print_safe("🚫 Cancelled ", cancelled, " slow tasks");

        // Wait for remaining processing
        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        print_safe("📊 Final queue message count: ",
                   messageQueue.getMessageCount());
        print_safe("📊 Processed tasks: ", processedTasks.load());
        print_safe("📊 Cancelled tasks: ", cancelled);

        // Stop processing
        messageQueue.stopProcessing();

        timer.stop();
    }

#ifdef ATOM_USE_ASIO
    io_context.stop();
    processingThread.join();
#endif
}

// Main functionint main() {
try {
    std::cout << "====== MessageQueue Usage Examples ======" << std::endl;

    basic_queue_operations();
    priority_processing_examples();
    advanced_queue_management();

    std::cout << "\n====== All MessageQueue Examples Completed ======"
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
