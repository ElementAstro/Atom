#include "atom/connection/async_tcpclient.hpp"
#include "atom/connection/async_sockethub.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>

// Utility class for formatted logging with timestamps

#undef ERROR
class Logger {
public:
    enum Level { INFO, WARNING, ERROR, SUCCESS };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Get current time with milliseconds
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count() %
                      1000;

        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &now_time_t);
#else
        localtime_r(&now_time_t, &tm_buf);
#endif

        std::stringstream ss;
        ss << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "."
           << std::setfill('0') << std::setw(3) << now_ms << "] ";

        // Add color codes based on log level
        switch (level) {
            case INFO:
                std::cout << ss.str() << "[INFO] ";
                break;
            case WARNING:
                std::cout << ss.str() << "[WARN] ";
                break;
            case ERROR:
                std::cout << ss.str() << "[ERROR] ";
                break;
            case SUCCESS:
                std::cout << ss.str() << "[SUCCESS] ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }

private:
    static std::mutex mutex_;
};

std::mutex Logger::mutex_;

// Enhanced statistics tracking for async operations
class AsyncStats {
public:
    std::atomic<size_t> total_operations{0};
    std::atomic<size_t> successful_operations{0};
    std::atomic<size_t> failed_operations{0};
    std::atomic<size_t> bytes_transferred{0};
    std::atomic<size_t> connection_attempts{0};
    std::atomic<size_t> reconnection_attempts{0};
    std::chrono::steady_clock::time_point start_time;

    AsyncStats() : start_time(std::chrono::steady_clock::now()) {}

    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        Logger::log(Logger::INFO, "AsyncStats",
                    "=== Async Operation Statistics ===");
        Logger::log(Logger::INFO, "AsyncStats",
                    "Runtime: " + std::to_string(seconds) + " seconds");
        Logger::log(
            Logger::INFO, "AsyncStats",
            "Total operations: " + std::to_string(total_operations.load()));
        Logger::log(
            Logger::INFO, "AsyncStats",
            "Successful: " + std::to_string(successful_operations.load()));
        Logger::log(Logger::INFO, "AsyncStats",
                    "Failed: " + std::to_string(failed_operations.load()));
        Logger::log(
            Logger::INFO, "AsyncStats",
            "Bytes transferred: " + std::to_string(bytes_transferred.load()));
        Logger::log(Logger::INFO, "AsyncStats",
                    "Connection attempts: " +
                        std::to_string(connection_attempts.load()));
        Logger::log(Logger::INFO, "AsyncStats",
                    "Reconnection attempts: " +
                        std::to_string(reconnection_attempts.load()));

        if (seconds > 0) {
            Logger::log(Logger::INFO, "AsyncStats",
                        "Operations/sec: " +
                            std::to_string(total_operations.load() / seconds));
            Logger::log(Logger::INFO, "AsyncStats",
                        "Bytes/sec: " +
                            std::to_string(bytes_transferred.load() / seconds));
        }

        double success_rate = total_operations.load() > 0
                                  ? (double(successful_operations.load()) /
                                     total_operations.load()) *
                                        100.0
                                  : 0.0;
        Logger::log(Logger::INFO, "AsyncStats",
                    "Success rate: " + std::to_string(success_rate) + "%");
    }
};

// Message queue for async message processing
class AsyncMessageQueue {
private:
    std::queue<std::string> messages_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_processing_{false};

public:
    void push(const std::string& message) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        messages_.push(message);
        cv_.notify_one();
    }

    bool pop(std::string& message, std::chrono::milliseconds timeout =
                                       std::chrono::milliseconds(1000)) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        if (cv_.wait_for(lock, timeout, [this] {
                return !messages_.empty() || stop_processing_;
            })) {
            if (!messages_.empty()) {
                message = messages_.front();
                messages_.pop();
                return true;
            }
        }
        return false;
    }

    void stop() {
        stop_processing_ = true;
        cv_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex_));
        return messages_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex_));
        return messages_.empty();
    }
};

// Global statistics instance
AsyncStats globalAsyncStats;

// Echo server for testing the TCP client
class EchoServer {
public:
    EchoServer(int port) : port_(port), running_(false) {
        Logger::log(Logger::INFO, "EchoServer",
                    "Initializing on port " + std::to_string(port));
    }

    ~EchoServer() { stop(); }

    void start() {
        if (running_)
            return;

        atom::async::connection::SocketHubConfig config;
        config.use_ssl = false;
        server_ = std::make_unique<atom::async::connection::SocketHub>(config);

        // Add message handler
        server_->addMessageHandler(
            [this](const atom::async::connection::Message& message,
                   size_t client_id) {
                Logger::log(Logger::INFO, "EchoServer",
                            "Received from client " +
                                std::to_string(client_id) + ": " +
                                message.asString());

                // Echo the message back
                auto response = atom::async::connection::Message::createText(
                    "Echo: " + message.asString());
                server_->sendMessageToClient(client_id, response);
            });

        // Add connect handler
        server_->addConnectHandler(
            [](size_t client_id, const std::string& address) {
                Logger::log(Logger::SUCCESS, "EchoServer",
                            "Client " + std::to_string(client_id) +
                                " connected from " + address);
            });

        // Add disconnect handler
        server_->addDisconnectHandler(
            [](size_t client_id, const std::string& address) {
                Logger::log(Logger::INFO, "EchoServer",
                            "Client " + std::to_string(client_id) +
                                " disconnected from " + address);
            });

        // Start the server
        server_->start(port_);
        running_ = server_->isRunning();

        if (running_) {
            Logger::log(Logger::SUCCESS, "EchoServer",
                        "Started on port " + std::to_string(port_));
        } else {
            Logger::log(Logger::ERROR, "EchoServer",
                        "Failed to start on port " + std::to_string(port_));
        }
    }

    void stop() {
        if (running_ && server_) {
            server_->stop();
            running_ = false;
            Logger::log(Logger::INFO, "EchoServer", "Server stopped");
        }
    }

    bool isRunning() const { return running_; }

private:
    int port_;
    bool running_;
    std::unique_ptr<atom::async::connection::SocketHub> server_;
};

// Function to convert string to bytes
std::vector<char> stringToBytes(const std::string& str) {
    return std::vector<char>(str.begin(), str.end());
}

// Function to convert bytes to string
std::string bytesToString(const std::vector<char>& data) {
    return std::string(data.begin(), data.end());
}

// Enhanced example class demonstrating advanced async TcpClient features
class TcpClientExample {
private:
    AsyncMessageQueue message_queue_;
    std::atomic<bool> running_{true};

public:
    void run() {
        // Start the echo server for testing
        Logger::log(Logger::INFO, "Example", "Starting Echo Server...");
        EchoServer server(8888);
        server.start();

        if (!server.isRunning()) {
            Logger::log(Logger::ERROR, "Example",
                        "Failed to start echo server. Example aborted.");
            return;
        }

        // Give the server time to initialize
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Example 1: Basic TcpClient creation
        Logger::log(Logger::INFO, "Example",
                    "Example 1: Creating TcpClient (non-SSL)");
        atom::async::connection::ConnectionConfig config;
        config.use_ssl = false;
        atom::async::connection::TcpClient client(config);

        // Example 2: Set up callbacks before connecting
        Logger::log(Logger::INFO, "Example", "Example 2: Setting up callbacks");

        // Connected callback
        client.setOnConnectedCallback([this]() {
            Logger::log(Logger::SUCCESS, "Client", "Connected to server");
            connection_events_.push_back("connected");
        });

        // Disconnected callback
        client.setOnDisconnectedCallback([this]() {
            Logger::log(Logger::INFO, "Client", "Disconnected from server");
            connection_events_.push_back("disconnected");
        });

        // Data received callback
        client.setOnDataReceivedCallback([this](const std::vector<char>& data) {
            std::string message = bytesToString(data);
            Logger::log(Logger::INFO, "Client", "Received data: " + message);
            received_data_.push_back(message);
        });

        // Error callback
        client.setOnErrorCallback([this](const std::string& error) {
            Logger::log(Logger::ERROR, "Client", "Error: " + error);
            error_messages_.push_back(error);
        });

        // Example 3: Connect to server
        Logger::log(Logger::INFO, "Example",
                    "Example 3: Connecting to server with timeout");
        bool connected =
            client.connect("localhost", 8888, std::chrono::milliseconds(5000));

        if (connected) {
            Logger::log(Logger::SUCCESS, "Example",
                        "Connected to server successfully");
        } else {
            Logger::log(Logger::ERROR, "Example",
                        "Failed to connect: " + client.getErrorMessage());
        }

        // Example 4: Check connection status
        Logger::log(Logger::INFO, "Example",
                    "Example 4: Checking connection status");
        if (client.isConnected()) {
            Logger::log(Logger::SUCCESS, "Example", "Client is connected");
        } else {
            Logger::log(Logger::ERROR, "Example", "Client is not connected");
        }

        // Example 5: Send data to server
        Logger::log(Logger::INFO, "Example",
                    "Example 5: Sending data to server");
        std::string message = "Hello, TCP Server!";
        if (client.send(stringToBytes(message))) {
            Logger::log(Logger::SUCCESS, "Example",
                        "Message sent successfully");
        } else {
            Logger::log(Logger::ERROR, "Example",
                        "Failed to send message: " + client.getErrorMessage());
        }

        // Wait for response
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Example 6: Configure heartbeat interval
        Logger::log(Logger::INFO, "Example",
                    "Example 6: Setting heartbeat interval");
        client.setHeartbeatInterval(std::chrono::milliseconds(2000));
        Logger::log(Logger::INFO, "Example",
                    "Heartbeat interval set to 2 seconds");

        // Example 7: Enable reconnection attempts
        Logger::log(Logger::INFO, "Example",
                    "Example 7: Enabling reconnection");
        client.configureReconnection(3);
        Logger::log(Logger::INFO, "Example",
                    "Reconnection enabled with 3 attempts");

        // Example 8: Send multiple messages
        Logger::log(Logger::INFO, "Example",
                    "Example 8: Sending multiple messages");
        for (int i = 1; i <= 3; i++) {
            std::string msg = "Message " + std::to_string(i);
            if (client.send(stringToBytes(msg))) {
                Logger::log(Logger::SUCCESS, "Example", "Sent: " + msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Wait for responses
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Example 9: Explicit receive operation with future
        Logger::log(Logger::INFO, "Example",
                    "Example 9: Explicit receive with future");

        // Send a specific message to receive
        std::string specificMessage = "RequestForExplicitReceive";
        client.send(stringToBytes(specificMessage));

        // Wait a moment for the server to process
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Now try to receive with a timeout
        try {
            Logger::log(Logger::INFO, "Example", "Waiting for response...");
            auto future = client.receive(1024, std::chrono::milliseconds(2000));

            // Wait for the future to complete
            auto status = future.wait_for(std::chrono::seconds(3));

            if (status == std::future_status::ready) {
                auto data = future.get();
                Logger::log(Logger::SUCCESS, "Example",
                            "Received response: " + bytesToString(data));
            } else {
                Logger::log(Logger::WARNING, "Example",
                            "Receive operation timed out");
            }
        } catch (const std::exception& e) {
            Logger::log(Logger::ERROR, "Example",
                        "Exception during receive: " + std::string(e.what()));
        }

        // Example 10: Disconnect from server
        Logger::log(Logger::INFO, "Example",
                    "Example 10: Disconnecting from server");
        client.disconnect();

        // Check if disconnected
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!client.isConnected()) {
            Logger::log(Logger::SUCCESS, "Example",
                        "Client disconnected successfully");
        } else {
            Logger::log(Logger::ERROR, "Example",
                        "Client failed to disconnect");
        }

        // Example 11: Create SSL client
        Logger::log(Logger::INFO, "Example",
                    "Example 11: Creating SSL TcpClient");
        atom::async::connection::ConnectionConfig ssl_config;
        ssl_config.use_ssl = true;
        atom::async::connection::TcpClient ssl_client(ssl_config);
        Logger::log(Logger::INFO, "Example",
                    "SSL client created (not connecting in this example)");

        // Example 12: Error handling
        Logger::log(Logger::INFO, "Example",
                    "Example 12: Error handling demonstration");
        // Try to connect to a non-existent server
        if (!ssl_client.connect("nonexistenthost.local", 12345,
                                std::chrono::milliseconds(2000))) {
            Logger::log(Logger::INFO, "Example",
                        "Expected failure connecting to non-existent host: " +
                            ssl_client.getErrorMessage());
        }

        // Example 13: Reconnect to server
        Logger::log(Logger::INFO, "Example",
                    "Example 13: Reconnecting to server");
        if (client.connect("localhost", 8888,
                           std::chrono::milliseconds(5000))) {
            Logger::log(Logger::SUCCESS, "Example", "Reconnected successfully");

            // Send one more message
            client.send(stringToBytes("Final message after reconnection"));

            // Wait for response
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Disconnect again
            client.disconnect();
        } else {
            Logger::log(Logger::ERROR, "Example",
                        "Failed to reconnect: " + client.getErrorMessage());
        }

        // Example 14: Advanced async patterns
        Logger::log(Logger::INFO, "Example",
                    "Example 14: Advanced async patterns");
        advancedAsyncPatterns(client);

        // Example 15: Concurrent connections
        Logger::log(Logger::INFO, "Example",
                    "Example 15: Concurrent connections");
        concurrentConnectionsExample();

        // Example 16: Message queue processing
        Logger::log(Logger::INFO, "Example",
                    "Example 16: Message queue processing");
        messageQueueExample(client);

        // Stop the echo server
        Logger::log(Logger::INFO, "Example", "Stopping Echo Server...");
        server.stop();

        // Print comprehensive statistics
        globalAsyncStats.print_summary();

        // Summary
        Logger::log(Logger::SUCCESS, "Example",
                    "Enhanced TcpClient example completed successfully");
        printEventSummary();
    }

    // Example 14: Advanced async patterns with futures and promises
    void advancedAsyncPatterns(atom::async::connection::TcpClient& client) {
        Logger::log(Logger::INFO, "AdvancedAsync",
                    "Testing advanced async patterns");

        try {
            // Pattern 1: Future-based async operations
            Logger::log(Logger::INFO, "AdvancedAsync",
                        "Pattern 1: Future-based operations");

            std::vector<std::future<bool>> futures;
            std::vector<std::string> messages = {
                "Future_Message_1", "Future_Message_2", "Future_Message_3"};

            for (const auto& msg : messages) {
                auto future = std::async(std::launch::async, [&client, msg]() {
                    globalAsyncStats.total_operations++;
                    bool success = client.send(stringToBytes(msg));
                    if (success) {
                        globalAsyncStats.successful_operations++;
                        globalAsyncStats.bytes_transferred += msg.length();
                    } else {
                        globalAsyncStats.failed_operations++;
                    }
                    return success;
                });
                futures.push_back(std::move(future));
            }

            // Wait for all futures to complete
            for (auto& future : futures) {
                try {
                    bool result = future.get();
                    std::string status = result ? "succeeded" : "failed";
                    Logger::log(result ? Logger::SUCCESS : Logger::ERROR,
                                "AdvancedAsync", "Future operation " + status);
                } catch (const std::exception& e) {
                    Logger::log(Logger::ERROR, "AdvancedAsync",
                                "Future exception: " + std::string(e.what()));
                }
            }

            // Pattern 2: Promise-based operations
            Logger::log(Logger::INFO, "AdvancedAsync",
                        "Pattern 2: Promise-based operations");

            std::promise<std::string> response_promise;
            auto response_future = response_promise.get_future();

            // Set up a temporary callback to capture response
            auto original_callback = [this](const std::vector<char>& data) {
                std::string message = bytesToString(data);
                Logger::log(Logger::INFO, "Client",
                            "Received data: " + message);
                received_data_.push_back(message);
            };

            client.setOnDataReceivedCallback(
                [&response_promise,
                 original_callback](const std::vector<char>& data) {
                    std::string message = bytesToString(data);
                    original_callback(data);

                    // Fulfill promise with first response
                    static std::once_flag flag;
                    std::call_once(flag, [&response_promise, message]() {
                        response_promise.set_value(message);
                    });
                });

            // Send message and wait for response
            client.send(stringToBytes("Promise_Test_Message"));

            auto status = response_future.wait_for(std::chrono::seconds(3));
            if (status == std::future_status::ready) {
                std::string response = response_future.get();
                Logger::log(Logger::SUCCESS, "AdvancedAsync",
                            "Promise fulfilled with response: " + response);
            } else {
                Logger::log(Logger::WARNING, "AdvancedAsync",
                            "Promise timeout - no response received");
            }

        } catch (const std::exception& e) {
            Logger::log(Logger::ERROR, "AdvancedAsync",
                        "Exception in advanced async patterns: " +
                            std::string(e.what()));
        }
    }

    // Example 15: Concurrent connections demonstration
    void concurrentConnectionsExample() {
        Logger::log(Logger::INFO, "Concurrent",
                    "Testing concurrent connections");

        const int num_connections = 3;
        std::vector<std::thread> connection_threads;
        std::atomic<int> successful_connections{0};
        std::atomic<int> failed_connections{0};

        for (int i = 0; i < num_connections; ++i) {
            connection_threads.emplace_back([i, &successful_connections,
                                             &failed_connections]() {
                try {
                    Logger::log(
                        Logger::INFO, "Concurrent",
                        "Starting connection thread " + std::to_string(i + 1));

                    atom::async::connection::ConnectionConfig config;
                    config.use_ssl = false;
                    config.connect_timeout = std::chrono::milliseconds(3000);

                    atom::async::connection::TcpClient client(config);
                    globalAsyncStats.connection_attempts++;

                    if (client.connect("localhost", 8888,
                                       std::chrono::milliseconds(3000))) {
                        successful_connections++;
                        Logger::log(Logger::SUCCESS, "Concurrent",
                                    "Thread " + std::to_string(i + 1) +
                                        " connected successfully");

                        // Send some messages
                        for (int j = 0; j < 3; ++j) {
                            std::string msg = "Thread_" +
                                              std::to_string(i + 1) + "_Msg_" +
                                              std::to_string(j + 1);
                            if (client.send(stringToBytes(msg))) {
                                globalAsyncStats.successful_operations++;
                                globalAsyncStats.bytes_transferred +=
                                    msg.length();
                            }
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                        }

                        client.disconnect();
                    } else {
                        failed_connections++;
                        globalAsyncStats.failed_operations++;
                        Logger::log(Logger::ERROR, "Concurrent",
                                    "Thread " + std::to_string(i + 1) +
                                        " failed to connect");
                    }
                } catch (const std::exception& e) {
                    failed_connections++;
                    Logger::log(Logger::ERROR, "Concurrent",
                                "Thread " + std::to_string(i + 1) +
                                    " exception: " + std::string(e.what()));
                }
            });
        }

        // Wait for all threads to complete
        for (auto& thread : connection_threads) {
            thread.join();
        }

        Logger::log(
            Logger::INFO, "Concurrent",
            "Concurrent test completed. Successful: " +
                std::to_string(successful_connections.load()) +
                ", Failed: " + std::to_string(failed_connections.load()));
    }

    // Example 16: Message queue processing
    void messageQueueExample(atom::async::connection::TcpClient& client) {
        Logger::log(Logger::INFO, "MessageQueue",
                    "Testing message queue processing");

        try {
            // Start message processor thread
            std::thread processor_thread([this, &client]() {
                Logger::log(Logger::INFO, "MessageQueue",
                            "Message processor started");

                while (running_) {
                    std::string message;
                    if (message_queue_.pop(message,
                                           std::chrono::milliseconds(500))) {
                        Logger::log(Logger::INFO, "MessageQueue",
                                    "Processing queued message: " + message);

                        if (client.send(stringToBytes(message))) {
                            globalAsyncStats.successful_operations++;
                            globalAsyncStats.bytes_transferred +=
                                message.length();
                        } else {
                            globalAsyncStats.failed_operations++;
                        }

                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(100));
                    }
                }

                Logger::log(Logger::INFO, "MessageQueue",
                            "Message processor stopped");
            });

            // Queue some messages
            std::vector<std::string> queue_messages = {
                "Queued_Message_1", "Queued_Message_2", "Queued_Message_3",
                "Queued_Message_4", "Queued_Message_5"};

            for (const auto& msg : queue_messages) {
                message_queue_.push(msg);
                Logger::log(Logger::INFO, "MessageQueue",
                            "Queued message: " + msg);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            // Let the processor work for a while
            std::this_thread::sleep_for(std::chrono::seconds(3));

            // Stop processing
            running_ = false;
            message_queue_.stop();
            processor_thread.join();

            Logger::log(
                Logger::SUCCESS, "MessageQueue",
                "Message queue processing completed. Remaining messages: " +
                    std::to_string(message_queue_.size()));

        } catch (const std::exception& e) {
            Logger::log(Logger::ERROR, "MessageQueue",
                        "Exception in message queue processing: " +
                            std::string(e.what()));
        }
    }

private:
    void printEventSummary() {
        Logger::log(
            Logger::INFO, "Summary",
            "Connection events: " + std::to_string(connection_events_.size()));
        for (const auto& event : connection_events_) {
            Logger::log(Logger::INFO, "Summary", "Event: " + event);
        }

        Logger::log(
            Logger::INFO, "Summary",
            "Received data messages: " + std::to_string(received_data_.size()));
        for (const auto& data : received_data_) {
            Logger::log(Logger::INFO, "Summary", "Data: " + data);
        }

        Logger::log(
            Logger::INFO, "Summary",
            "Error messages: " + std::to_string(error_messages_.size()));
        for (const auto& error : error_messages_) {
            Logger::log(Logger::INFO, "Summary", "Error: " + error);
        }
    }

    std::vector<std::string> connection_events_;
    std::vector<std::string> received_data_;
    std::vector<std::string> error_messages_;
};

int main() {
    try {
        Logger::log(Logger::INFO, "Main",
                    "Starting Enhanced Async TcpClient Example Application");
        Logger::log(Logger::INFO, "Main", "");
        Logger::log(Logger::INFO, "Main", "Features demonstrated:");
        Logger::log(Logger::INFO, "Main",
                    "- Basic async TCP client operations");
        Logger::log(Logger::INFO, "Main",
                    "- SSL/TLS configuration (non-SSL in this example)");
        Logger::log(Logger::INFO, "Main",
                    "- Heartbeat and reconnection mechanisms");
        Logger::log(Logger::INFO, "Main", "- Callback-based event handling");
        Logger::log(Logger::INFO, "Main", "- Future-based async operations");
        Logger::log(Logger::INFO, "Main", "- Promise-based response handling");
        Logger::log(Logger::INFO, "Main", "- Concurrent connection management");
        Logger::log(Logger::INFO, "Main", "- Message queue processing");
        Logger::log(Logger::INFO, "Main", "- Comprehensive error handling");
        Logger::log(Logger::INFO, "Main", "- Performance statistics tracking");
        Logger::log(Logger::INFO, "Main", "");

        TcpClientExample example;
        example.run();

        Logger::log(Logger::SUCCESS, "Main",
                    "All async examples completed successfully");
        return 0;
    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Main",
                    std::string("Fatal error: ") + e.what());
        globalAsyncStats.print_summary();
        return 1;
    }
}
