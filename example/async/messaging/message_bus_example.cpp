/**
 * @file message_bus.cpp
 * @brief Comprehensive demonstration of atom::async::MessageBus functionality
 *
 * @details This example demonstrates:
 * - Basic publish-subscribe messaging patterns
 * - Message filtering and namespaces
 * - Multiple subscriber management
 * - Async vs synchronous message publishing
 * - Message history and replay capabilities
 * - Global messaging and broadcasting
 * - Advanced subscriber patterns and lifecycle management
 * - Performance optimization techniques
 *
 * @level Intermediate to Advanced
 * @prerequisites Basic understanding of pub-sub patterns, async programming
 * @related_examples message_queue.cpp, eventstack.cpp, async_executor.cpp
 *
 * @note Demonstrates enterprise-level messaging patterns
 *
 * @author Atom Async Examples
 * @date 2024
 */

#include "atom/async/messaging/message_bus.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
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

// Basic message typestruct BasicMessage {
std::string content;
int id;

BasicMessage(std::string content, int id)
    : content(std::move(content)), id(id) {}
}
;

// User event messagestruct UserEvent {
std::string username;
std::string action;
std::chrono::system_clock::time_point timestamp;

UserEvent(std::string username, std::string action)
    : username(std::move(username)),
      action(std::move(action)),
      timestamp(std::chrono::system_clock::now()) {}
}
;

// System notification messagestruct SystemNotification {
enum class Level { INFO, WARNING, ERROR_LEVEL, CRITICAL };

Level level;
std::string message;
std::string component;

SystemNotification(Level level, std::string message, std::string component)
    : level(level),
      message(std::move(message)),
      component(std::move(component)) {}
}
;

// Data processing messagestruct DataProcessingMessage {
std::vector<int> data;
std::string operation;
int batch_id;

DataProcessingMessage(std::vector<int> data, std::string operation,
                      int batch_id)
    : data(std::move(data)),
      operation(std::move(operation)),
      batch_id(batch_id) {}
}
;

// ============================================================================
// SECTION 1: BASIC PUBLISH-SUBSCRIBE PATTERNS
// ============================================================================
/**
 * @section basic_pubsub Basic Publish-Subscribe Patterns
 *
 * This section demonstrates fundamental MessageBus operations:
 * - Creating MessageBus instances
 * - Basic message subscription and publishing
 * - Subscriber management and unsubscription
 * - Message handler patterns
 * - Thread-safe message handling
 *
 * Key concepts:
 * - MessageBus: Central hub for publish-subscribe messaging
 * - subscribe(): Register handlers for specific message types
 * - publish(): Send messages to all subscribers
 * - unsubscribe(): Remove specific subscribers
 *
 * @see eventstack.cpp for event-based messaging patterns
 */
void basic_pubsub_examples() {
    print_section("SECTION 1: Basic Publish-Subscribe Patterns");

#ifdef ATOM_USE_ASIO
    asio::io_context io_context;
    auto messageBus = MessageBus::createShared(io_context);
    std::thread ioThread([&io_context]() { io_context.run(); });
#else
    auto messageBus = MessageBus::createShared();
#endif

    // Example 1.1: Basic message subscription and publishing
    print_safe("Example 1.1: Basic message subscription and publishing");
    {
        PerformanceTimer timer;
        timer.start("Basic pub-sub");

        std::atomic<int> messageCount{0};

        // Subscribe to basic messages
        auto token = messageBus->subscribe<BasicMessage>(
            "basic.message", [&messageCount](const BasicMessage& message) {
                messageCount++;
                print_safe("🔔 Handler received: '", message.content,
                           "' (ID: ", message.id, ")");
                print_safe("🔔 Handler thread: ", get_thread_id());
            });

        // Publish several messages
        for (int i = 1; i <= 3; ++i) {
            BasicMessage message("Message " + std::to_string(i), i);
            messageBus->publish("basic.message", message);
            print_safe("📤 Published message ", i);
        }

        // Wait for message processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        print_safe("📊 Total messages received: ", messageCount.load());

        // Unsubscribe
        messageBus->unsubscribe<BasicMessage>(token);
        print_safe("🚫 Unsubscribed from basic.message");

        timer.stop();
    }

#ifdef ATOM_USE_ASIO
    io_context.stop();
    ioThread.join();
#endif
}

// ============================================================================
// SECTION 2: MULTIPLE SUBSCRIBERS AND MESSAGE FILTERING
// ============================================================================
/**
 * @section multiple_subscribers Multiple Subscribers and Message Filtering
 *
 * This section demonstrates advanced subscriber management:
 * - Multiple subscribers for the same message type
 * - Message filtering and selective handling
 * - Subscriber priority and ordering
 * - Namespace-based message organization
 * - Conditional message processing
 *
 * Key concepts:
 * - Multiple handlers: Multiple subscribers can handle the same message
 * - Message namespaces: Organizing messages with hierarchical names
 * - Filtering: Selective message processing based on content
 * - Handler coordination: Managing multiple concurrent handlers
 *
 * @see parallel.cpp for concurrent message processing
 */
void multiple_subscribers_examples() {
    print_section("SECTION 2: Multiple Subscribers and Message Filtering");

#ifdef ATOM_USE_ASIO
    asio::io_context io_context;
    auto messageBus = MessageBus::createShared(io_context);
    std::thread ioThread([&io_context]() { io_context.run(); });
#else
    auto messageBus = MessageBus::createShared();
#endif

    // Example 2.1: Multiple subscribers for same message type
    print_safe("Example 2.1: Multiple subscribers for same message type");
    {
        PerformanceTimer timer;
        timer.start("Multiple subscribers");

        std::atomic<int> handler1Count{0}, handler2Count{0}, handler3Count{0};

        // Subscribe multiple handlers to user events
        auto token1 = messageBus->subscribe<UserEvent>(
            "user.event", [&handler1Count](const UserEvent& event) {
                handler1Count++;
                print_safe("🔔 Logger: User '", event.username, "' performed '",
                           event.action, "'");
            });

        auto token2 = messageBus->subscribe<UserEvent>(
            "user.event", [&handler2Count](const UserEvent& event) {
                handler2Count++;
                if (event.action == "login") {
                    print_safe("🔔 Security: Login detected for user '",
                               event.username, "'");
                }
            });

        auto token3 = messageBus->subscribe<UserEvent>(
            "user.event", [&handler3Count](const UserEvent& event) {
                handler3Count++;
                print_safe("🔔 Analytics: Recording action '", event.action,
                           "' for user '", event.username, "'");
            });

        // Publish various user events
        std::vector<UserEvent> events = {
            UserEvent("alice", "login"), UserEvent("bob", "logout"),
            UserEvent("charlie", "login"), UserEvent("alice", "view_profile")};

        for (const auto& event : events) {
            messageBus->publish("user.event", event);
            print_safe("📤 Published user event: ", event.username, " -> ",
                       event.action);
        }

        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        print_safe("📊 Handler 1 (Logger) processed: ", handler1Count.load(),
                   " events");
        print_safe("📊 Handler 2 (Security) processed: ", handler2Count.load(),
                   " events");
        print_safe("📊 Handler 3 (Analytics) processed: ", handler3Count.load(),
                   " events");

        // Cleanup
        messageBus->unsubscribe<UserEvent>(token1);
        messageBus->unsubscribe<UserEvent>(token2);
        messageBus->unsubscribe<UserEvent>(token3);

        timer.stop();
    }

#ifdef ATOM_USE_ASIO
    io_context.stop();
    ioThread.join();
#endif
}

// ============================================================================
// SECTION 3: NAMESPACE-BASED MESSAGING AND GLOBAL BROADCASTING
// ============================================================================
/**
 * @section namespaces Namespace-based Messaging and Global Broadcasting
 *
 * This section demonstrates advanced messaging organization:
 * - Hierarchical message namespaces
 * - Global message broadcasting
 * - Namespace-specific filtering
 * - System-wide notifications
 * - Message routing strategies
 *
 * Key concepts:
 * - Namespaces: Hierarchical organization of message types
 * - Global publishing: Broadcasting to all subscribers regardless of namespace
 * - Message routing: Directing messages based on namespace patterns
 * - System notifications: Critical system-wide messages
 *
 * @see eventstack.cpp for hierarchical event handling
 */
void namespace_and_global_examples() {
    print_section(
        "SECTION 3: Namespace-based Messaging and Global Broadcasting");

#ifdef ATOM_USE_ASIO
    asio::io_context io_context;
    auto messageBus = MessageBus::createShared(io_context);
    std::thread ioThread([&io_context]() { io_context.run(); });
#else
    auto messageBus = MessageBus::createShared();
#endif

    // Example 3.1: Namespace-based message organization
    print_safe("Example 3.1: Namespace-based message organization");
    {
        PerformanceTimer timer;
        timer.start("Namespace messaging");

        std::atomic<int> infoCount{0}, warningCount{0}, errorCount{0};

        // Subscribe to different notification levels
        auto infoToken = messageBus->subscribe<SystemNotification>(
            "system.info",
            [&infoCount](const SystemNotification& notification) {
                infoCount++;
                print_safe("ℹ️  INFO [", notification.component,
                           "]: ", notification.message);
            });

        auto warningToken = messageBus->subscribe<SystemNotification>(
            "system.warning",
            [&warningCount](const SystemNotification& notification) {
                warningCount++;
                print_safe("⚠️  WARNING [", notification.component,
                           "]: ", notification.message);
            });

        auto errorToken = messageBus->subscribe<SystemNotification>(
            "system.error",
            [&errorCount](const SystemNotification& notification) {
                errorCount++;
                print_safe("❌ ERROR [", notification.component,
                           "]: ", notification.message);
            });

        // Publish notifications to different namespaces
        messageBus->publish("system.info",
                            SystemNotification(SystemNotification::Level::INFO,
                                               "System started", "Core"));
        messageBus->publish(
            "system.warning",
            SystemNotification(SystemNotification::Level::WARNING,
                               "High memory usage", "Memory"));
        messageBus->publish(
            "system.error",
            SystemNotification(SystemNotification::Level::ERROR_LEVEL,
                               "Database connection failed", "Database"));
        messageBus->publish("system.info",
                            SystemNotification(SystemNotification::Level::INFO,
                                               "User session created", "Auth"));

        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        print_safe("📊 Info messages: ", infoCount.load());
        print_safe("📊 Warning messages: ", warningCount.load());
        print_safe("📊 Error messages: ", errorCount.load());

        // Cleanup
        messageBus->unsubscribe<SystemNotification>(infoToken);
        messageBus->unsubscribe<SystemNotification>(warningToken);
        messageBus->unsubscribe<SystemNotification>(errorToken);

        timer.stop();
    }

    // Example 3.2: Global broadcasting
    print_safe("\nExample 3.2: Global broadcasting");
    {
        PerformanceTimer timer;
        timer.start("Global broadcasting");

        std::atomic<int> globalCount{0};

        // Subscribe to global messages (no specific namespace)
        auto globalToken = messageBus->subscribe<SystemNotification>(
            "", [&globalCount](const SystemNotification& notification) {
                globalCount++;
                print_safe("🌐 GLOBAL [", notification.component,
                           "]: ", notification.message);
            });

        // Publish global critical notification
        SystemNotification criticalNotification(
            SystemNotification::Level::CRITICAL, "System shutdown initiated",
            "System");

        print_safe("📢 Publishing global critical notification...");
        messageBus->publishGlobal(criticalNotification);

        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        print_safe("📊 Global messages received: ", globalCount.load());

        // Cleanup
        messageBus->unsubscribe<SystemNotification>(globalToken);

        timer.stop();
    }

#ifdef ATOM_USE_ASIO
    io_context.stop();
    ioThread.join();
#endif
}

// Main functionint main() {
try {
    std::cout << "====== MessageBus Usage Examples ======" << std::endl;

    basic_pubsub_examples();
    multiple_subscribers_examples();
    namespace_and_global_examples();

    std::cout << "\n====== All MessageBus Examples Completed ======"
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
