#include "atom/connection/async_sockethub.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Helper class for timestamped logging
class Logger {
public:
    static void log(const std::string& source, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        std::cout << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "] "
                  << "[" << source << "] " << message << std::endl;
    }

private:
    static std::mutex mutex_;
};

std::mutex Logger::mutex_;

// Simulate a client for testing
class TestClient {
public:
    TestClient(const std::string& name, int port)
        : name_(name), socket_(io_context_), is_connected_(false) {
        Logger::log(name_, "Initializing client");

        try {
            asio::ip::tcp::endpoint endpoint(
                asio::ip::address::from_string("127.0.0.1"), port);
            socket_.connect(endpoint);
            is_connected_ = true;
            Logger::log(name_,
                        "Connected to server on port " + std::to_string(port));

            // Start reading
            startRead();

            // Start thread for IO operations
            io_thread_ = std::thread([this]() {
                try {
                    io_context_.run();
                } catch (const std::exception& e) {
                    Logger::log(name_, std::string("IO error: ") + e.what());
                }
            });
        } catch (const std::exception& e) {
            Logger::log(name_, std::string("Connection failed: ") + e.what());
        }
    }

    ~TestClient() { disconnect(); }

    void sendMessage(const std::string& message) {
        if (!is_connected_) {
            Logger::log(name_, "Cannot send message: not connected");
            return;
        }

        Logger::log(name_, "Sending message: " + message);
        asio::async_write(
            socket_, asio::buffer(message),
            [this, message](std::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    Logger::log(name_, "Send error: " + ec.message());
                    disconnect();
                } else {
                    Logger::log(name_, "Message sent successfully");
                }
            });
    }

    void disconnect() {
        if (is_connected_) {
            is_connected_ = false;
            asio::post(io_context_, [this]() { socket_.close(); });

            if (io_thread_.joinable()) {
                io_thread_.join();
            }

            Logger::log(name_, "Disconnected from server");
        }
    }

    bool isConnected() const { return is_connected_; }

private:
    void startRead() {
        auto buffer = std::make_shared<std::vector<char>>(1024);
        socket_.async_read_some(
            asio::buffer(*buffer),
            [this, buffer](std::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string message(buffer->data(), length);
                    Logger::log(name_, "Received: " + message);
                    startRead();  // Continue reading
                } else {
                    Logger::log(name_, "Read error: " + ec.message());
                    disconnect();
                }
            });
    }

    std::string name_;
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
    std::thread io_thread_;
    std::atomic<bool> is_connected_;
};

// Main example class to demonstrate SocketHub usage
class SocketHubExample {
public:
    SocketHubExample() : running_(false) {}

    void run() {
        // Example 1: Create and start a SocketHub
        Logger::log("Main", "Example 1: Creating and starting SocketHub");
        atom::async::connection::SocketHub hub(false);  // non-SSL mode

        // Example 2: Register message handler
        Logger::log("Main", "Example 2: Registering message handler");
        hub.addHandler([this](const std::string& message, size_t client_id) {
            Logger::log(
                "MessageHandler",
                "Client " + std::to_string(client_id) + " sent: " + message);

            // Echo back the message with a prefix
            std::string response = "Echo from server: " + message;
            handleServerCommands(message, client_id);
        });

        // Example 3: Register connection handler
        Logger::log("Main", "Example 3: Registering connect handler");
        hub.addConnectHandler([this](size_t client_id) {
            Logger::log("ConnectHandler",
                        "Client " + std::to_string(client_id) + " connected");
            connected_clients_.push_back(client_id);
        });

        // Example 4: Register disconnection handler
        Logger::log("Main", "Example 4: Registering disconnect handler");
        hub.addDisconnectHandler([this](size_t client_id) {
            Logger::log(
                "DisconnectHandler",
                "Client " + std::to_string(client_id) + " disconnected");

            // Remove from connected clients list
            connected_clients_.erase(
                std::remove(connected_clients_.begin(),
                            connected_clients_.end(), client_id),
                connected_clients_.end());
        });

        // Example 5: Start the server
        const int PORT = 8080;
        Logger::log("Main", "Example 5: Starting server on port " +
                                std::to_string(PORT));
        hub.start(PORT);

        if (hub.isRunning()) {
            Logger::log("Main", "Server started successfully");
        } else {
            Logger::log("Main", "Failed to start server");
            return;
        }

        // Wait for server to initialize fully
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Example 6: Connect clients
        Logger::log("Main", "Example 6: Connecting test clients");
        std::vector<std::unique_ptr<TestClient>> clients;

        // Create 3 test clients
        for (int i = 1; i <= 3; i++) {
            std::string clientName = "Client" + std::to_string(i);
            clients.push_back(std::make_unique<TestClient>(clientName, PORT));
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // Example 7: Send messages from clients
        Logger::log("Main", "Example 7: Sending messages from clients");
        for (size_t i = 0; i < clients.size(); i++) {
            if (clients[i]->isConnected()) {
                clients[i]->sendMessage("Hello from client " +
                                        std::to_string(i + 1));
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Example 8: Broadcast message to all clients
        Logger::log("Main", "Example 8: Broadcasting message to all clients");
        hub.broadcastMessage("Server broadcast: Hello to all clients!");

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Example 9: Send targeted messages
        Logger::log("Main", "Example 9: Sending targeted messages");
        if (!connected_clients_.empty()) {
            for (size_t client_id : connected_clients_) {
                hub.sendMessageToClient(
                    client_id,
                    "Private message for client " + std::to_string(client_id));
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Example 10: Disconnect one client
        Logger::log("Main", "Example 10: Disconnecting one client");
        if (!clients.empty() && clients[0]->isConnected()) {
            clients[0]->disconnect();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Example 11: Send messages after client disconnect
        Logger::log("Main",
                    "Example 11: Sending messages after client disconnect");
        hub.broadcastMessage("Broadcast after disconnect");

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Example 12: Stop the server
        Logger::log("Main", "Example 12: Stopping the server");
        hub.stop();
        Logger::log("Main", "Server stopped");

        // Disconnect remaining clients
        for (auto& client : clients) {
            if (client->isConnected()) {
                client->disconnect();
            }
        }

        // Example 13: Check server status after stopping
        Logger::log("Main",
                    "Example 13: Checking server status after stopping");
        if (hub.isRunning()) {
            Logger::log("Main", "Server is still running (unexpected)");
        } else {
            Logger::log("Main", "Server is stopped (expected)");
        }

        Logger::log("Main", "SocketHub example completed");
    }

private:
    void handleServerCommands(const std::string& message, size_t client_id) {
        // Simple command processor
        if (message == "ping") {
            // Handle ping command
            server_hub_->sendMessageToClient(client_id, "pong");
        } else if (message.find("echo ") == 0) {
            // Echo command
            std::string echo_message =
                message.substr(5);  // Remove "echo " prefix
            server_hub_->sendMessageToClient(client_id, echo_message);
        }
    }

    // Store the hub for command handling
    atom::async::connection::SocketHub* server_hub_ = nullptr;
    std::atomic<bool> running_;
    std::vector<size_t> connected_clients_;
};

int main() {
    try {
        Logger::log("Main", "Starting SocketHub example application");
        SocketHubExample example;
        example.run();
        return 0;
    } catch (const std::exception& e) {
        Logger::log("Main", std::string("Fatal error: ") + e.what());
        return 1;
    }
}