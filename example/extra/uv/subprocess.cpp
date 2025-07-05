/**
 * @file main.cpp
 * @brief Complete example demonstrating the UvProcess class usage
 */
#include "atom/extra/uv/subprocess.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

// Helper function to print a header for each test
void printTestHeader(const std::string& title) {
    std::cout << "\n\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Helper to get a timestamp string
std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S") << '.'
       << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Example 1: Basic command execution
bool runBasicCommand() {
    printTestHeader("Example 1: Basic Command Execution");

    UvProcess process;
    std::string output;

    std::cout << "[" << getTimestamp() << "] Starting basic command..."
              << std::endl;

// Use appropriate command for the operating system
#ifdef _WIN32
    bool success = process.spawn(
        "cmd.exe", {"/c", "echo Hello, World! & echo Current directory: & cd"},
        "",  // Current directory
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp()
                      << "] Process exited with status: " << exit_status
                      << std::endl;
        },
        // Stdout callback
        [&output](const char* data, ssize_t size) {
            std::cout << "[" << getTimestamp() << "] Received " << size
                      << " bytes of data" << std::endl;
            output.append(data, size);
        },
        // Stderr callback
        [](const char* data, ssize_t size) {
            std::cerr << "[" << getTimestamp()
                      << "] Error: " << std::string(data, size) << std::endl;
        });
#else
    bool success = process.spawn(
        "/bin/sh", {"-c", "echo Hello, World!; echo Current directory:; pwd"},
        "",  // Current directory
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp()
                      << "] Process exited with status: " << exit_status
                      << std::endl;
        },
        // Stdout callback
        [&output](const char* data, ssize_t size) {
            std::cout << "[" << getTimestamp() << "] Received " << size
                      << " bytes of data" << std::endl;
            output.append(data, size);
        },
        // Stderr callback
        [](const char* data, ssize_t size) {
            std::cerr << "[" << getTimestamp()
                      << "] Error: " << std::string(data, size) << std::endl;
        });
#endif

    if (!success) {
        std::cerr << "Failed to start process" << std::endl;
        return false;
    }

    std::cout << "[" << getTimestamp()
              << "] Process started with PID: " << process.getPid()
              << std::endl;

    // Process events while we wait
    while (process.isRunning()) {
        std::cout << "[" << getTimestamp()
                  << "] Waiting for process to complete..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "[" << getTimestamp() << "] Command output:" << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << output;
    std::cout << "------------------------" << std::endl;

    return true;
}

// Example 2: Process with timeout
bool runProcessWithTimeout() {
    printTestHeader("Example 2: Process with Timeout");

    UvProcess process;
    std::string output;

    // Configure process options with a timeout
    UvProcess::ProcessOptions options;

#ifdef _WIN32
    options.file = "cmd.exe";
    options.args = {"/c",
                    "echo Starting long process... & timeout /t 5 & echo This "
                    "should not be seen"};
#else
    options.file = "/bin/sh";
    options.args = {
        "-c",
        "echo Starting long process...; sleep 5; echo This should not be seen"};
#endif

    // Set a 2-second timeout
    options.timeout = std::chrono::milliseconds(2000);

    std::cout << "[" << getTimestamp()
              << "] Starting process with 2-second timeout..." << std::endl;

    bool success = process.spawnWithOptions(
        options,
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp()
                      << "] Process exited with status: " << exit_status
                      << ", signal: " << term_signal << std::endl;
        },
        // Stdout callback
        [&output](const char* data, ssize_t size) {
            output.append(data, size);
            std::cout << "[" << getTimestamp()
                      << "] Output: " << std::string(data, size);
        },
        // No stderr callback
        nullptr,
        // Timeout callback
        []() {
            std::cout << "[" << getTimestamp()
                      << "] Process timed out after 2 seconds!" << std::endl;
        });

    if (!success) {
        std::cerr << "Failed to start process with timeout" << std::endl;
        return false;
    }

    // Wait for process to complete or timeout
    process.waitForExit();

    // Check process status
    std::cout << "[" << getTimestamp() << "] Process status: ";
    switch (process.getStatus()) {
        case UvProcess::ProcessStatus::EXITED:
            std::cout << "EXITED with code " << process.getExitCode()
                      << std::endl;
            break;
        case UvProcess::ProcessStatus::TERMINATED:
            std::cout << "TERMINATED by signal" << std::endl;
            break;
        case UvProcess::ProcessStatus::TIMED_OUT:
            std::cout << "TIMED_OUT" << std::endl;
            break;
        default:
            std::cout << "OTHER (" << static_cast<int>(process.getStatus())
                      << ")" << std::endl;
    }

    return true;
}

// Example 3: Interactive process
bool runInteractiveProcess() {
    printTestHeader("Example 3: Interactive Process");

    UvProcess process;

    std::cout << "[" << getTimestamp() << "] Starting interactive process..."
              << std::endl;

#ifdef _WIN32
    bool success = process.spawn(
        "cmd.exe", {"/k", "echo Type commands for CMD. Type 'exit' to quit."},
        "",
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp() << "] Interactive process exited"
                      << std::endl;
        },
        // Stdout callback
        [](const char* data, ssize_t size) {
            std::cout << std::string(data, size);
        },
        // Stderr callback
        [](const char* data, ssize_t size) {
            std::cerr << std::string(data, size);
        });
#else
    bool success = process.spawn(
        "/bin/sh", {}, "",
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp() << "] Interactive process exited"
                      << std::endl;
        },
        // Stdout callback
        [](const char* data, ssize_t size) {
            std::cout << std::string(data, size);
        },
        // Stderr callback
        [](const char* data, ssize_t size) {
            std::cerr << std::string(data, size);
        });
#endif

    if (!success) {
        std::cerr << "Failed to start interactive process" << std::endl;
        return false;
    }

    // Wait a moment for process to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Send some commands automatically
    std::vector<std::string> commands = {
#ifdef _WIN32
        "echo Current time: %TIME%", "echo Current directory: %CD%",
        "echo Environment variables:", "set | findstr PATH", "exit"
#else
        "echo Current time: $(date)", "echo Current directory: $PWD",
        "echo Environment variables:", "env | grep PATH", "exit"
#endif
    };

    for (const auto& cmd : commands) {
        std::cout << "\n[" << getTimestamp() << "] Sending command: " << cmd
                  << std::endl;

#ifdef _WIN32
        process.writeToStdin(cmd + "\r\n");
#else
        process.writeToStdin(cmd + "\n");
#endif

        // Wait a moment between commands
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Wait for process to exit
    process.waitForExit();

    return true;
}

// Example 4: Error handling
bool runErrorHandlingTest() {
    printTestHeader("Example 4: Error Handling");

    UvProcess process;

    // Set a global error handler
    process.setErrorCallback([](const std::string& error) {
        std::cerr << "[" << getTimestamp() << "] ERROR: " << error << std::endl;
    });

    std::cout << "[" << getTimestamp()
              << "] Attempting to run non-existent command..." << std::endl;

    // Try to run a non-existent command
    bool success = process.spawn(
        "non_existent_command", {"--version"}, "",
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "This should not be called" << std::endl;
        },
        // Stdout callback
        [](const char* data, ssize_t size) {
            std::cout << "This should not be called" << std::endl;
        });

    if (success) {
        std::cerr << "Unexpectedly succeeded in starting non-existent process"
                  << std::endl;
        return false;
    }

    std::cout << "[" << getTimestamp()
              << "] As expected, failed to start non-existent command"
              << std::endl;

    // Now try a valid command but with invalid arguments
    std::cout << "[" << getTimestamp()
              << "] Running command with invalid arguments..." << std::endl;

#ifdef _WIN32
    success = process.spawn(
        "cmd.exe", {"/c", "dir /nonexistentoption"}, "",
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp()
                      << "] Process exited with status: " << exit_status
                      << std::endl;
        },
        // Stdout callback
        [](const char* data, ssize_t size) {
            std::cout << std::string(data, size);
        },
        // Stderr callback
        [](const char* data, ssize_t size) {
            std::cerr << "STDERR: " << std::string(data, size);
        });
#else
    success = process.spawn(
        "/bin/ls", {"--nonexistentoption"}, "",
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp()
                      << "] Process exited with status: " << exit_status
                      << std::endl;
        },
        // Stdout callback
        [](const char* data, ssize_t size) {
            std::cout << std::string(data, size);
        },
        // Stderr callback
        [](const char* data, ssize_t size) {
            std::cerr << "STDERR: " << std::string(data, size);
        });
#endif

    if (!success) {
        std::cerr << "Failed to start process with invalid arguments"
                  << std::endl;
        return false;
    }

    // Wait for process to complete
    process.waitForExit();

    std::cout << "[" << getTimestamp()
              << "] Process completed with exit code: " << process.getExitCode()
              << std::endl;

    return true;
}

// Example 5: Process with custom environment
bool runProcessWithEnvironment() {
    printTestHeader("Example 5: Process with Custom Environment");

    UvProcess process;
    std::string output;

    // Configure process options with custom environment
    UvProcess::ProcessOptions options;

#ifdef _WIN32
    options.file = "cmd.exe";
    options.args = {
        "/c",
        "echo Custom environment variable: %CUSTOM_VAR% & echo PATH: %PATH%"};
#else
    options.file = "/bin/sh";
    options.args = {
        "-c",
        "echo Custom environment variable: $CUSTOM_VAR; echo PATH: $PATH"};
#endif

    // Set custom environment variables
    options.env = {
        {"CUSTOM_VAR", "Hello from UvProcess!"},
        {"PATH", "/custom/path:/another/path"}  // Override PATH
    };

    // Don't inherit parent environment (use only our specified variables)
    options.inherit_parent_env = false;

    std::cout << "[" << getTimestamp()
              << "] Starting process with custom environment..." << std::endl;

    bool success = process.spawnWithOptions(
        options,
        // Exit callback
        [](int64_t exit_status, int term_signal) {
            std::cout << "[" << getTimestamp()
                      << "] Process exited with status: " << exit_status
                      << std::endl;
        },
        // Stdout callback
        [&output](const char* data, ssize_t size) {
            output.append(data, size);
            std::cout << std::string(data, size);
        });

    if (!success) {
        std::cerr << "Failed to start process with custom environment"
                  << std::endl;
        return false;
    }

    // Wait for process to complete
    process.waitForExit();

    return true;
}

int main() {
    bool success = true;

    std::cout << "UV Process Example Application" << std::endl;
    std::cout << "Running on: "
#ifdef _WIN32
              << "Windows"
#else
              << "Unix/Linux"
#endif
              << std::endl;

    // Run all examples
    success &= runBasicCommand();
    success &= runProcessWithTimeout();
    success &= runInteractiveProcess();
    success &= runErrorHandlingTest();
    success &= runProcessWithEnvironment();

    if (success) {
        std::cout << "\n\nAll examples completed successfully!" << std::endl;
        return 0;
    } else {
        std::cerr << "\n\nSome examples failed!" << std::endl;
        return 1;
    }
}
