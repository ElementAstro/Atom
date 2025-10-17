#include "atom/extra/uv/coro.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace uv_coro;
using namespace std::chrono_literals;

// Example coroutine functions
Task<std::string> fetch_data_async(const std::string& url) {
    std::cout << "Starting to fetch data from: " << url << std::endl;

    // Simulate network delay
    co_await timeout(500ms);

    std::cout << "Data fetched from: " << url << std::endl;
    co_return "Data from " + url;
}

Task<int> calculate_async(int a, int b) {
    std::cout << "Starting calculation: " << a << " + " << b << std::endl;

    // Simulate computation delay
    co_await timeout(200ms);

    int result = a + b;
    std::cout << "Calculation completed: " << result << std::endl;
    co_return result;
}

Task<void> process_files_async(const std::vector<std::string>& filenames) {
    std::cout << "Processing " << filenames.size() << " files..." << std::endl;

    for (const auto& filename : filenames) {
        std::cout << "Processing file: " << filename << std::endl;

        // Simulate file processing
        co_await timeout(100ms);

        std::cout << "Completed processing: " << filename << std::endl;
    }

    std::cout << "All files processed" << std::endl;
}

Task<std::string> tcp_echo_client(const std::string& host, int port,
                                  const std::string& message) {
    std::cout << "Connecting to " << host << ":" << port << std::endl;

    try {
        // Connect to server
        auto socket = co_await tcp_connect(host, port);
        std::cout << "Connected successfully" << std::endl;

        // Send message
        co_await tcp_write(socket, message);
        std::cout << "Sent message: " << message << std::endl;

        // Read response
        auto response = co_await tcp_read(socket);
        std::cout << "Received response: " << response << std::endl;

        co_return response;
    } catch (const UvError& e) {
        std::cerr << "TCP error: " << e.what() << std::endl;
        co_return "Error: " + std::string(e.what());
    }
}

Task<void> file_operations_async() {
    std::cout << "Starting file operations..." << std::endl;

    try {
        // Open file for writing
        auto write_file = co_await file_open(
            "test_async.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
        std::cout << "File opened for writing" << std::endl;

        // Write data
        std::string data =
            "Hello from async file operations!\nThis is line 2.\nThis is line "
            "3.\n";
        co_await file_write(write_file, data);
        std::cout << "Data written to file" << std::endl;

        // Close write file
        co_await file_close(write_file);
        std::cout << "Write file closed" << std::endl;

        // Open file for reading
        auto read_file = co_await file_open("test_async.txt", O_RDONLY, 0);
        std::cout << "File opened for reading" << std::endl;

        // Read data
        auto read_data = co_await file_read(read_file, 1024);
        std::cout << "Read data from file: " << read_data << std::endl;

        // Close read file
        co_await file_close(read_file);
        std::cout << "Read file closed" << std::endl;

    } catch (const UvError& e) {
        std::cerr << "File operation error: " << e.what() << std::endl;
    }
}

Task<void> concurrent_tasks_example() {
    std::cout << "Starting concurrent tasks example..." << std::endl;

    // Start multiple tasks concurrently
    auto task1 = fetch_data_async("https://api1.example.com");
    auto task2 = fetch_data_async("https://api2.example.com");
    auto task3 = calculate_async(10, 20);

    // Wait for all tasks to complete
    auto result1 = co_await task1;
    auto result2 = co_await task2;
    auto result3 = co_await task3;

    std::cout << "All concurrent tasks completed:" << std::endl;
    std::cout << "  Result 1: " << result1 << std::endl;
    std::cout << "  Result 2: " << result2 << std::endl;
    std::cout << "  Result 3: " << result3 << std::endl;
}

Task<void> error_handling_example() {
    std::cout << "Starting error handling example..." << std::endl;

    try {
        // This will likely fail (invalid host)
        auto result = co_await tcp_echo_client("invalid.host.example", 12345,
                                               "test message");
        std::cout << "Unexpected success: " << result << std::endl;
    } catch (const UvError& e) {
        std::cout << "Caught expected error: " << e.what() << std::endl;
    }

    try {
        // This will also likely fail (invalid file)
        auto file = co_await file_open("/invalid/path/file.txt", O_RDONLY, 0);
        std::cout << "Unexpected file open success" << std::endl;
    } catch (const UvError& e) {
        std::cout << "Caught expected file error: " << e.what() << std::endl;
    }

    std::cout << "Error handling example completed" << std::endl;
}

Task<void> timeout_example() {
    std::cout << "Starting timeout example..." << std::endl;

    // Short timeout
    auto start = std::chrono::steady_clock::now();
    co_await timeout(100ms);
    auto end = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Short timeout completed in " << duration.count() << "ms"
              << std::endl;

    // Longer timeout
    start = std::chrono::steady_clock::now();
    co_await timeout(500ms);
    end = std::chrono::steady_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Long timeout completed in " << duration.count() << "ms"
              << std::endl;
}

Task<void> udp_example() {
    std::cout << "Starting UDP example..." << std::endl;

    try {
        // Send UDP message
        std::string message = "Hello UDP!";
        co_await udp_send("127.0.0.1", 8080, message);
        std::cout << "UDP message sent: " << message << std::endl;

        // Try to receive (this might timeout if no server is listening)
        try {
            auto received = co_await udp_receive(8080, 1000ms);
            std::cout << "UDP message received: " << received.data.size()
                      << " bytes" << std::endl;
        } catch (const UvError& e) {
            std::cout << "UDP receive timeout or error (expected): " << e.what()
                      << std::endl;
        }

    } catch (const UvError& e) {
        std::cout << "UDP error: " << e.what() << std::endl;
    }
}

Task<void> process_example() {
    std::cout << "Starting process example..." << std::endl;

    try {
        // Run a simple command
        ProcessOptions options;
        options.file = "echo";
        options.args = {"echo", "Hello from subprocess!"};

        auto result = co_await spawn_process(options);
        std::cout << "Process completed with exit code: " << result.exit_code
                  << std::endl;
        std::cout << "Process output: " << result.stdout_data << std::endl;

    } catch (const UvError& e) {
        std::cout << "Process error: " << e.what() << std::endl;
    }
}

Task<void> main_coroutine() {
    std::cout << "=== UV Coroutines Example ===" << std::endl;

    // 1. Basic async operations
    std::cout << "\n1. Basic Async Operations:" << std::endl;
    co_await fetch_data_async("https://example.com/api/data");
    auto calc_result = co_await calculate_async(15, 25);
    std::cout << "Calculation result: " << calc_result << std::endl;

    // 2. File operations
    std::cout << "\n2. File Operations:" << std::endl;
    co_await file_operations_async();

    // 3. Process files
    std::cout << "\n3. Process Files:" << std::endl;
    std::vector<std::string> files = {"file1.txt", "file2.txt", "file3.txt"};
    co_await process_files_async(files);

    // 4. Concurrent tasks
    std::cout << "\n4. Concurrent Tasks:" << std::endl;
    co_await concurrent_tasks_example();

    // 5. Error handling
    std::cout << "\n5. Error Handling:" << std::endl;
    co_await error_handling_example();

    // 6. Timeout operations
    std::cout << "\n6. Timeout Operations:" << std::endl;
    co_await timeout_example();

    // 7. UDP operations
    std::cout << "\n7. UDP Operations:" << std::endl;
    co_await udp_example();

    // 8. Process operations
    std::cout << "\n8. Process Operations:" << std::endl;
    co_await process_example();

    std::cout << "\n=== UV Coroutines Example Completed ===" << std::endl;
}

int main() {
    try {
        // Create scheduler and run the main coroutine
        Scheduler scheduler;

        // Schedule the main coroutine
        scheduler.schedule(main_coroutine());

        // Run the event loop
        scheduler.run();

        std::cout << "Event loop completed" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
