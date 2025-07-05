#include "atom/connection/async_fifoclient.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>

// Helper function to print optional string results
void printResult(const std::optional<std::string>& result,
                 const std::string& operation) {
    if (result) {
        std::cout << "Success: " << operation << " - Data: " << *result
                  << std::endl;
    } else {
        std::cout << "Failed: " << operation << " - No data received"
                  << std::endl;
    }
}

int main() {
// Define FIFO path based on platform
#ifdef _WIN32
    const std::string fifoPath = "\\\\.\\pipe\\example_pipe";
#else
    const std::string fifoPath = "/tmp/example_fifo";
#endif

    try {
        std::cout << "Creating FifoClient with path: " << fifoPath << std::endl;

        // Create FifoClient instance
        atom::async::connection::FifoClient client(fifoPath);

        // Example 1: Check if FIFO is open
        std::cout << "Example 1: Checking if FIFO is open" << std::endl;
        if (client.isOpen()) {
            std::cout << "Success: FIFO is open" << std::endl;
        } else {
            std::cout << "Failed: FIFO is not open" << std::endl;
            return 1;
        }

        // Example 2: Write without timeout
        std::cout << "\nExample 2: Writing data without timeout" << std::endl;
        std::string message1 = "Hello FIFO World!";
        if (client.write(message1)) {
            std::cout << "Success: Data written without timeout: " << message1
                      << std::endl;
        } else {
            std::cout << "Failed: Could not write data without timeout"
                      << std::endl;
        }

        // Example 3: Write with timeout
        std::cout << "\nExample 3: Writing data with 500ms timeout"
                  << std::endl;
        std::string message2 = "This message has a timeout";
        if (client.write(message2, std::chrono::milliseconds(500))) {
            std::cout << "Success: Data written with timeout: " << message2
                      << std::endl;
        } else {
            std::cout << "Failed: Could not write data within timeout"
                      << std::endl;
        }

        // Example 4: Read without timeout
        std::cout << "\nExample 4: Reading data without timeout" << std::endl;
        auto result1 = client.read();
        printResult(result1, "Reading without timeout");

        // Example 5: Read with timeout
        std::cout << "\nExample 5: Reading data with 1000ms timeout"
                  << std::endl;
        auto result2 = client.read(std::chrono::milliseconds(1000));
        printResult(result2, "Reading with timeout");

        // Example 6: Try write after closing
        std::cout
            << "\nExample 6: Closing FIFO and trying operations after closing"
            << std::endl;
        client.close();
        if (!client.isOpen()) {
            std::cout << "Success: FIFO is now closed" << std::endl;
        }

        if (client.write("This shouldn't work")) {
            std::cout << "Unexpected: Data was written after closing"
                      << std::endl;
        } else {
            std::cout << "Expected: Could not write after closing" << std::endl;
        }

        // Example 7: Reopen after closing
        std::cout
            << "\nExample 7: Creating a new FifoClient instance after closing"
            << std::endl;
        try {
            atom::async::connection::FifoClient newClient(fifoPath);
            if (newClient.isOpen()) {
                std::cout << "Success: New FIFO client opened successfully"
                          << std::endl;

                // Write and read with the new client
                std::string message3 = "Message from reopened client";
                if (newClient.write(message3)) {
                    std::cout << "Success: Data written to reopened client: "
                              << message3 << std::endl;
                }

                // Read with a short timeout
                auto result3 = newClient.read(std::chrono::milliseconds(300));
                printResult(result3, "Reading from reopened client");
            }
        } catch (const std::exception& e) {
            std::cout << "Error reopening FIFO: " << e.what() << std::endl;
        }

        // Example 8: Exception handling when FIFO doesn't exist
        std::cout << "\nExample 8: Testing with non-existent FIFO path"
                  << std::endl;
        try {
            atom::async::connection::FifoClient invalidClient(
                "non_existent_path");
            std::cout << "Unexpected: Created client with invalid path"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected exception: " << e.what() << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nFifoClient example completed successfully" << std::endl;
    return 0;
}
