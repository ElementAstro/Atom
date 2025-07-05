#include "atom/connection/async_fifoserver.hpp"
#include "atom/connection/async_fifoclient.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// Helper function to print messages with timestamps
void logMessage(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::ctime(&now_c) << "] " << message;
}

// Class to demonstrate FifoServer usage
class FifoServerDemo {
public:
    FifoServerDemo() : running_(false) {}

    // Start the demo
    void run() {
// Define fifo path based on platform
#ifdef _WIN32
        const std::string fifoPath = "\\\\.\\pipe\\demo_fifo_pipe";
#else
        const std::string fifoPath = "/tmp/demo_fifo";
#endif

        logMessage("Starting FifoServer demo with path: " + fifoPath + "\n");

        // Example 1: Create and start the server
        logMessage("Example 1: Creating and starting FifoServer\n");
        atom::async::connection::FifoServer server(fifoPath);

        // Start the server
        server.start();
        if (server.isRunning()) {
            logMessage("Server started successfully\n");
        } else {
            logMessage("Failed to start server\n");
            return;
        }

        // Example 2: Check server status
        logMessage("Example 2: Checking server status\n");
        logMessage("Server running status: " +
                   std::string(server.isRunning() ? "Running" : "Not Running") +
                   "\n");

        // Example 3: Create a client and send messages to the server
        logMessage("Example 3: Creating client and sending messages\n");
        running_ = true;

        // Start client in a separate thread
        std::thread clientThread(
            [this, fifoPath]() { clientFunction(fifoPath); });

        // Wait for a few seconds
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Example 4: Stop the server
        logMessage("Example 4: Stopping the server\n");
        running_ = false;

        if (clientThread.joinable()) {
            clientThread.join();
        }

        server.stop();
        logMessage("Server stopped\n");

        // Example 5: Check server status after stopping
        logMessage("Example 5: Checking server status after stopping\n");
        logMessage(
            "Server running status: " +
            std::string(server.isRunning() ? "Still Running" : "Stopped") +
            "\n");

        // Example 6: Restart the server
        logMessage("Example 6: Restarting the server\n");
        server.start();
        if (server.isRunning()) {
            logMessage("Server restarted successfully\n");

            // Send one more message after restart
            try {
                atom::async::connection::FifoClient restartClient(fifoPath);
                restartClient.write("Message after server restart\n");
                logMessage("Sent message after server restart\n");
            } catch (const std::exception& e) {
                logMessage(
                    std::string("Error sending message after restart: ") +
                    e.what() + "\n");
            }

            // Give the server time to process the message
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Stop the server again
            server.stop();
            logMessage("Server stopped again\n");
        } else {
            logMessage("Failed to restart server\n");
        }
    }

private:
    // Client function that runs in a separate thread
    void clientFunction(const std::string& fifoPath) {
        try {
            // Create a client that connects to the server
            atom::async::connection::FifoClient client(fifoPath);

            // Send periodic messages until signaled to stop
            int messageCount = 1;
            while (running_ && messageCount <= 5) {
                std::string message =
                    "Test message " + std::to_string(messageCount) + "\n";

                // Try to send with timeout
                if (client.write(message, std::chrono::milliseconds(500))) {
                    logMessage("Client sent: " + message);
                } else {
                    logMessage("Client failed to send message\n");
                }

                messageCount++;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Example with a very long message
            std::string longMessage =
                "This is a very long message that tests the FIFO buffer "
                "handling. ";
            for (int i = 0; i < 5; i++) {
                longMessage += "Repeated text to make the message longer. ";
            }
            longMessage += "\n";

            if (client.write(longMessage)) {
                logMessage("Client sent a long message\n");
            }

            logMessage("Client thread finished\n");
        } catch (const std::exception& e) {
            logMessage(std::string("Client error: ") + e.what() + "\n");
        }
    }

    std::atomic<bool> running_;
};

// Error handling wrapper for main
int main() {
    try {
        FifoServerDemo demo;
        demo.run();
        logMessage("FifoServer demo completed successfully\n");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
