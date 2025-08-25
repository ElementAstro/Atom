#include "atom/extra/uv/message_bus.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace uv_message_bus;
using namespace std::chrono_literals;

// Example message types
struct UserLoginMessage {
    std::string user_id;
    std::string session_id;
    std::chrono::system_clock::time_point timestamp;

    std::string to_string() const {
        return "UserLogin{user_id=" + user_id + ", session_id=" + session_id +
               "}";
    }
};

struct OrderCreatedMessage {
    std::string order_id;
    std::string customer_id;
    double amount;
    std::string currency;

    std::string to_string() const {
        return "OrderCreated{order_id=" + order_id +
               ", customer_id=" + customer_id +
               ", amount=" + std::to_string(amount) + " " + currency + "}";
    }
};

struct SystemStatusMessage {
    std::string component;
    std::string status;
    std::string details;

    std::string to_string() const {
        return "SystemStatus{component=" + component + ", status=" + status +
               ", details=" + details + "}";
    }
};

// Example subscribers
class UserService {
private:
    std::string name_;
    std::atomic<int> messages_received_{0};

public:
    explicit UserService(const std::string& name) : name_(name) {}

    void on_user_login(const UserLoginMessage& msg) {
        messages_received_++;
        std::cout << "[" << name_ << "] Received: " << msg.to_string()
                  << std::endl;

        // Simulate processing
        std::this_thread::sleep_for(10ms);
        std::cout << "[" << name_
                  << "] Processed user login for: " << msg.user_id << std::endl;
    }

    void on_system_status(const SystemStatusMessage& msg) {
        messages_received_++;
        std::cout << "[" << name_
                  << "] System status update: " << msg.to_string() << std::endl;
    }

    int get_messages_received() const { return messages_received_.load(); }
};

class OrderService {
private:
    std::string name_;
    std::atomic<int> orders_processed_{0};

public:
    explicit OrderService(const std::string& name) : name_(name) {}

    void on_order_created(const OrderCreatedMessage& msg) {
        orders_processed_++;
        std::cout << "[" << name_ << "] Processing order: " << msg.to_string()
                  << std::endl;

        // Simulate order processing
        std::this_thread::sleep_for(50ms);
        std::cout << "[" << name_ << "] Order processed: " << msg.order_id
                  << std::endl;
    }

    void on_user_login(const UserLoginMessage& msg) {
        std::cout << "[" << name_
                  << "] User logged in, checking pending orders for: "
                  << msg.user_id << std::endl;
    }

    int get_orders_processed() const { return orders_processed_.load(); }
};

class AuditService {
private:
    std::string name_;
    std::vector<std::string> audit_log_;
    std::mutex log_mutex_;

public:
    explicit AuditService(const std::string& name) : name_(name) {}

    void on_user_login(const UserLoginMessage& msg) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        audit_log_.push_back("LOGIN: " + msg.to_string());
        std::cout << "[" << name_ << "] Audited user login: " << msg.user_id
                  << std::endl;
    }

    void on_order_created(const OrderCreatedMessage& msg) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        audit_log_.push_back("ORDER: " + msg.to_string());
        std::cout << "[" << name_
                  << "] Audited order creation: " << msg.order_id << std::endl;
    }

    void on_system_status(const SystemStatusMessage& msg) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        audit_log_.push_back("STATUS: " + msg.to_string());
        std::cout << "[" << name_
                  << "] Audited system status: " << msg.component << std::endl;
    }

    size_t get_audit_count() const {
        std::lock_guard<std::mutex> lock(log_mutex_);
        return audit_log_.size();
    }

    void print_audit_log() const {
        std::lock_guard<std::mutex> lock(log_mutex_);
        std::cout << "[" << name_ << "] Audit log (" << audit_log_.size()
                  << " entries):" << std::endl;
        for (const auto& entry : audit_log_) {
            std::cout << "  " << entry << std::endl;
        }
    }
};

int main() {
    try {
        std::cout << "=== UV Message Bus Example ===" << std::endl;

        // 1. Basic message bus setup and publishing
        std::cout << "\n1. Basic Message Bus Setup:" << std::endl;
        {
            MessageBus bus;

            // Create services
            UserService user_service("UserService");
            OrderService order_service("OrderService");
            AuditService audit_service("AuditService");

            // Subscribe to messages
            bus.subscribe<UserLoginMessage>(
                [&user_service](const UserLoginMessage& msg) {
                    user_service.on_user_login(msg);
                });

            bus.subscribe<OrderCreatedMessage>(
                [&order_service](const OrderCreatedMessage& msg) {
                    order_service.on_order_created(msg);
                });

            bus.subscribe<SystemStatusMessage>(
                [&user_service](const SystemStatusMessage& msg) {
                    user_service.on_system_status(msg);
                });

            // Publish some messages
            UserLoginMessage login_msg{"user123", "session456",
                                       std::chrono::system_clock::now()};
            bus.publish(login_msg);

            OrderCreatedMessage order_msg{"order789", "user123", 99.99, "USD"};
            bus.publish(order_msg);

            SystemStatusMessage status_msg{"database", "healthy",
                                           "All connections active"};
            bus.publish(status_msg);

            // Process messages
            bus.process_messages();

            std::cout << "Basic message bus setup completed" << std::endl;
        }

        // 2. Multiple subscribers for same message type
        std::cout << "\n2. Multiple Subscribers:" << std::endl;
        {
            MessageBus bus;

            UserService user_service("UserService");
            OrderService order_service("OrderService");
            AuditService audit_service("AuditService");

            // Multiple subscribers for UserLoginMessage
            bus.subscribe<UserLoginMessage>(
                [&user_service](const UserLoginMessage& msg) {
                    user_service.on_user_login(msg);
                });

            bus.subscribe<UserLoginMessage>(
                [&order_service](const UserLoginMessage& msg) {
                    order_service.on_user_login(msg);
                });

            bus.subscribe<UserLoginMessage>(
                [&audit_service](const UserLoginMessage& msg) {
                    audit_service.on_user_login(msg);
                });

            // Multiple subscribers for OrderCreatedMessage
            bus.subscribe<OrderCreatedMessage>(
                [&order_service](const OrderCreatedMessage& msg) {
                    order_service.on_order_created(msg);
                });

            bus.subscribe<OrderCreatedMessage>(
                [&audit_service](const OrderCreatedMessage& msg) {
                    audit_service.on_order_created(msg);
                });

            // Publish messages
            UserLoginMessage login_msg{"user456", "session789",
                                       std::chrono::system_clock::now()};
            bus.publish(login_msg);

            OrderCreatedMessage order_msg{"order123", "user456", 149.99, "EUR"};
            bus.publish(order_msg);

            // Process messages
            bus.process_messages();

            std::cout << "Multiple subscribers completed" << std::endl;
            std::cout << "User service received: "
                      << user_service.get_messages_received() << " messages"
                      << std::endl;
            std::cout << "Order service processed: "
                      << order_service.get_orders_processed() << " orders"
                      << std::endl;
            std::cout << "Audit service logged: "
                      << audit_service.get_audit_count() << " events"
                      << std::endl;
        }

        // 3. Asynchronous message processing
        std::cout << "\n3. Asynchronous Message Processing:" << std::endl;
        {
            MessageBus bus;
            bus.set_async_processing(true);

            std::atomic<int> messages_processed{0};

            // Subscribe with async processing
            bus.subscribe<UserLoginMessage>([&messages_processed](
                                                const UserLoginMessage& msg) {
                std::cout << "Async processing: " << msg.to_string()
                          << std::endl;

                // Simulate longer processing time
                std::this_thread::sleep_for(100ms);

                messages_processed++;
                std::cout << "Async processing completed for: " << msg.user_id
                          << std::endl;
            });

            // Publish multiple messages quickly
            for (int i = 0; i < 5; ++i) {
                UserLoginMessage msg{"user" + std::to_string(i),
                                     "session" + std::to_string(i),
                                     std::chrono::system_clock::now()};
                bus.publish(msg);
                std::cout << "Published message " << i << std::endl;
            }

            std::cout
                << "All messages published, waiting for async processing..."
                << std::endl;

            // Wait for all messages to be processed
            while (messages_processed.load() < 5) {
                std::this_thread::sleep_for(50ms);
                bus.process_messages();
            }

            std::cout << "Async processing completed: "
                      << messages_processed.load() << " messages" << std::endl;
        }

        // 4. Message filtering
        std::cout << "\n4. Message Filtering:" << std::endl;
        {
            MessageBus bus;

            std::atomic<int> high_value_orders{0};
            std::atomic<int> low_value_orders{0};

            // Filter for high-value orders only
            bus.subscribe<OrderCreatedMessage>(
                [&high_value_orders](const OrderCreatedMessage& msg) {
                    if (msg.amount >= 100.0) {
                        high_value_orders++;
                        std::cout
                            << "High-value order processed: " << msg.to_string()
                            << std::endl;
                    }
                });

            // Filter for low-value orders
            bus.subscribe<OrderCreatedMessage>(
                [&low_value_orders](const OrderCreatedMessage& msg) {
                    if (msg.amount < 100.0) {
                        low_value_orders++;
                        std::cout
                            << "Low-value order processed: " << msg.to_string()
                            << std::endl;
                    }
                });

            // Publish orders with different values
            std::vector<OrderCreatedMessage> orders = {
                {"order1", "customer1", 50.0, "USD"},
                {"order2", "customer2", 150.0, "USD"},
                {"order3", "customer3", 25.0, "USD"},
                {"order4", "customer4", 200.0, "USD"},
                {"order5", "customer5", 75.0, "USD"}};

            for (const auto& order : orders) {
                bus.publish(order);
            }

            bus.process_messages();

            std::cout << "Message filtering completed:" << std::endl;
            std::cout << "  High-value orders: " << high_value_orders.load()
                      << std::endl;
            std::cout << "  Low-value orders: " << low_value_orders.load()
                      << std::endl;
        }

        // 5. Message priorities
        std::cout << "\n5. Message Priorities:" << std::endl;
        {
            MessageBus bus;
            bus.enable_priority_queue(true);

            std::vector<std::string> processing_order;
            std::mutex order_mutex;

            bus.subscribe<SystemStatusMessage>(
                [&processing_order,
                 &order_mutex](const SystemStatusMessage& msg) {
                    std::lock_guard<std::mutex> lock(order_mutex);
                    processing_order.push_back(msg.component + ":" +
                                               msg.status);
                    std::cout << "Processed: " << msg.to_string() << std::endl;
                });

            // Publish messages with different priorities
            SystemStatusMessage low_priority{"cache", "warning",
                                             "Cache hit ratio low"};
            SystemStatusMessage high_priority{"database", "critical",
                                              "Connection pool exhausted"};
            SystemStatusMessage medium_priority{"api", "error",
                                                "Rate limit exceeded"};

            bus.publish(low_priority, MessagePriority::LOW);
            bus.publish(high_priority, MessagePriority::HIGH);
            bus.publish(medium_priority, MessagePriority::MEDIUM);

            // Add more messages
            bus.publish({"logging", "info", "Log rotation completed"},
                        MessagePriority::LOW);
            bus.publish({"security", "critical", "Unauthorized access attempt"},
                        MessagePriority::HIGH);

            bus.process_messages();

            std::cout << "Message processing order:" << std::endl;
            for (size_t i = 0; i < processing_order.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << processing_order[i]
                          << std::endl;
            }
        }

        // 6. Message bus statistics
        std::cout << "\n6. Message Bus Statistics:" << std::endl;
        {
            MessageBus bus;

            UserService user_service("UserService");
            OrderService order_service("OrderService");

            bus.subscribe<UserLoginMessage>(
                [&user_service](const UserLoginMessage& msg) {
                    user_service.on_user_login(msg);
                });

            bus.subscribe<OrderCreatedMessage>(
                [&order_service](const OrderCreatedMessage& msg) {
                    order_service.on_order_created(msg);
                });

            // Generate some activity
            for (int i = 0; i < 10; ++i) {
                UserLoginMessage login_msg{"user" + std::to_string(i),
                                           "session" + std::to_string(i),
                                           std::chrono::system_clock::now()};
                bus.publish(login_msg);

                if (i % 2 == 0) {
                    OrderCreatedMessage order_msg{"order" + std::to_string(i),
                                                  "user" + std::to_string(i),
                                                  (i + 1) * 25.0, "USD"};
                    bus.publish(order_msg);
                }
            }

            bus.process_messages();

            // Get statistics
            auto stats = bus.get_statistics();
            std::cout << "Message Bus Statistics:" << std::endl;
            std::cout << "  Total messages published: " << stats.total_published
                      << std::endl;
            std::cout << "  Total messages processed: " << stats.total_processed
                      << std::endl;
            std::cout << "  Messages in queue: " << stats.queue_size
                      << std::endl;
            std::cout << "  Active subscribers: " << stats.subscriber_count
                      << std::endl;
            std::cout << "  Average processing time: "
                      << stats.average_processing_time.count() << "ms"
                      << std::endl;
        }

        // 7. Error handling and dead letter queue
        std::cout << "\n7. Error Handling and Dead Letter Queue:" << std::endl;
        {
            MessageBus bus;
            bus.enable_dead_letter_queue(true);

            std::atomic<int> successful_processing{0};
            std::atomic<int> failed_processing{0};

            // Subscribe with error-prone handler
            bus.subscribe<OrderCreatedMessage>(
                [&successful_processing,
                 &failed_processing](const OrderCreatedMessage& msg) {
                    // Simulate random failures
                    if (msg.order_id.find("fail") != std::string::npos) {
                        failed_processing++;
                        throw std::runtime_error(
                            "Simulated processing error for order: " +
                            msg.order_id);
                    } else {
                        successful_processing++;
                        std::cout
                            << "Successfully processed: " << msg.to_string()
                            << std::endl;
                    }
                });

            // Set up dead letter handler
            bus.set_dead_letter_handler(
                [](const auto& failed_message, const std::string& error) {
                    std::cout << "Message sent to dead letter queue: " << error
                              << std::endl;
                });

            // Publish messages (some will fail)
            std::vector<OrderCreatedMessage> orders = {
                {"order_success_1", "customer1", 100.0, "USD"},
                {"order_fail_1", "customer2", 200.0, "USD"},
                {"order_success_2", "customer3", 150.0, "USD"},
                {"order_fail_2", "customer4", 300.0, "USD"},
                {"order_success_3", "customer5", 250.0, "USD"}};

            for (const auto& order : orders) {
                bus.publish(order);
            }

            bus.process_messages();

            std::cout << "Error handling completed:" << std::endl;
            std::cout << "  Successful processing: "
                      << successful_processing.load() << std::endl;
            std::cout << "  Failed processing: " << failed_processing.load()
                      << std::endl;

            auto dead_letter_count = bus.get_dead_letter_count();
            std::cout << "  Messages in dead letter queue: "
                      << dead_letter_count << std::endl;
        }

        std::cout << "\n=== UV Message Bus Example Completed ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
