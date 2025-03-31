#include "atom/connection/async_tcpclient.hpp"
#include "atom/connection/async_sockethub.hpp"

#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
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

        server_ = std::make_unique<atom::async::connection::SocketHub>(false);

        // Add message handler
        server_->addHandler(
            [this](const std::string& message, size_t client_id) {
                Logger::log(Logger::INFO, "EchoServer",
                            "Received from client " +
                                std::to_string(client_id) + ": " + message);

                // Echo the message back
                std::string response = "Echo: " + message;
                server_->sendMessageToClient(client_id, response);
            });

        // Add connect handler
        server_->addConnectHandler([](size_t client_id) {
            Logger::log(Logger::SUCCESS, "EchoServer",
                        "Client " + std::to_string(client_id) + " connected");
        });

        // Add disconnect handler
        server_->addDisconnectHandler([](size_t client_id) {
            Logger::log(
                Logger::INFO, "EchoServer",
                "Client " + std::to_string(client_id) + " disconnected");
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

// Main example class demonstrating TcpClient features
class TcpClientExample {
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
        atom::async::connection::TcpClient client(false);

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
        client.enableReconnection(3);
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
        atom::async::connection::TcpClient ssl_client(true);
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

        // Stop the echo server
        Logger::log(Logger::INFO, "Example", "Stopping Echo Server...");
        server.stop();

        // Summary
        Logger::log(Logger::SUCCESS, "Example",
                    "TcpClient example completed successfully");
        printEventSummary();
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
                    "Starting TcpClient example application");
        TcpClientExample example;
        example.run();
        return 0;
    } catch (const std::exception& e) {
        Logger::log(Logger::ERROR, "Main",
                    std::string("Fatal error: ") + e.what());
        return 1;
    }
}