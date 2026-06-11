/**
 * @file command_execution_suite.cpp
 * @brief Comprehensive example demonstrating all command execution capabilities
 *
 * This example showcases the complete command execution functionality available
 * in the Atom System module, including:
 * - Synchronous and asynchronous command execution
 * - Command piping and chaining
 * - Input/output handling and streaming
 * - Timeout management and cancellation
 * - Environment variable management
 * - Command history and batch processing
 * - Cross-platform command execution
 * - Advanced error handling and recovery
 *
 * @warning Some commands may require elevated privileges
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @author Atom Framework
 * @date 2024
 */

#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <unordered_map>
#include <sstream>
#include <thread>
#include <vector>
#include "atom/system/command.hpp"

using namespace atom::system;

/**
 * @brief Utility function to print formatted output
 */
void printOutput(const std::string& title, const std::string& output) {
    std::cout << "\n--- " << title << " ---" << std::endl;
    if (output.empty()) {
        std::cout << "(No output)" << std::endl;
    } else {
        // Limit output length for readability
        std::string displayOutput = output;
        if (displayOutput.length() > 500) {
            displayOutput = displayOutput.substr(0, 500) + "\n... (truncated)";
        }
        std::cout << displayOutput << std::endl;
    }
    std::cout << std::string(50, '-') << std::endl;
}

/**
 * @brief Utility function to print command result with status
 */
// executeCommandWithStatus / executeCommandsWithCommonEnv return
// std::pair<output, exitCode>.
void printCommandResult(const std::string& title,
                        const std::pair<std::string, int>& result) {
    std::cout << "\n--- " << title << " ---" << std::endl;
    std::cout << "Exit Status: " << result.second << std::endl;
    std::cout << "Output:" << std::endl;
    if (result.first.empty()) {
        std::cout << "(No output)" << std::endl;
    } else {
        std::string displayOutput = result.first;
        if (displayOutput.length() > 300) {
            displayOutput = displayOutput.substr(0, 300) + "\n... (truncated)";
        }
        std::cout << displayOutput << std::endl;
    }
    std::cout << std::string(50, '-') << std::endl;
}

/**
 * @brief Line processor callback for streaming output
 */
void lineProcessor(const std::string& line) {
    std::cout << "[STREAM] " << line << std::endl;
}

/**
 * @brief Advanced line processor with statistics
 */
class StatisticsLineProcessor {
private:
    std::atomic<int> lineCount_{0};
    std::atomic<int> totalChars_{0};

public:
    void operator()(const std::string& line) {
        lineCount_++;
        totalChars_ += line.length();
        std::cout << "[LINE " << lineCount_ << "] " << line << std::endl;
    }

    void printStatistics() const {
        std::cout << "Statistics: " << lineCount_ << " lines, " << totalChars_
                  << " characters processed" << std::endl;
    }
};

/**
 * @brief Get platform-appropriate test commands
 */
std::vector<std::string> getTestCommands() {
#ifdef _WIN32
    return {"echo Hello World", "dir /b", "ver", "echo %PATH%",
            "ping -n 2 127.0.0.1"};
#else
    return {"echo Hello World", "ls -la", "uname -a", "echo $PATH",
            "ping -c 2 127.0.0.1"};
#endif
}

int main() {
    try {
        std::cout << "=== Atom System Command Execution Suite ===" << std::endl;
        std::cout
            << "Demonstrating comprehensive command execution capabilities\n"
            << std::endl;

        // 1. Basic command execution
        std::cout << "[1. Basic Command Execution]" << std::endl;
#ifdef _WIN32
        std::string output = executeCommand("echo Hello from Windows!");
#else
        std::string output = executeCommand("echo Hello from Unix!");
#endif
        printOutput("Basic Command Execution", output);

        // 2. Command execution with line processing callback
        std::cout << "\n[2. Command Execution with Line Callback]" << std::endl;
        StatisticsLineProcessor processor;
#ifdef _WIN32
        output = executeCommand(
            "dir /b", false,
            [&processor](const std::string& line) { processor(line); });
#else
        output = executeCommand(
            "ls -1", false,
            [&processor](const std::string& line) { processor(line); });
#endif
        processor.printStatistics();

        // 3. Command execution with input
        std::cout << "\n[3. Command Execution with Input]" << std::endl;
#ifdef _WIN32
        output = executeCommandWithInput("findstr apple",
                                         "apple\nbanana\ncherry\napple pie");
#else
        output = executeCommandWithInput("grep apple",
                                         "apple\nbanana\ncherry\napple pie");
#endif
        printOutput("Command with Input", output);

        // 4. Command execution with timeout
        std::cout << "\n[4. Command Execution with Timeout]" << std::endl;
        auto timeout = std::chrono::milliseconds(2000);
#ifdef _WIN32
        auto result = executeCommandWithTimeout("ping -n 1 127.0.0.1", timeout);
#else
        auto result = executeCommandWithTimeout("ping -c 1 127.0.0.1", timeout);
#endif
        if (result.has_value()) {
            printOutput("Command with Timeout", result.value());
        } else {
            std::cout << "Command timed out after " << timeout.count() << "ms"
                      << std::endl;
        }

        // 5. Command execution with exit status
        std::cout << "\n[5. Command with Exit Status]" << std::endl;
#ifdef _WIN32
        auto resultWithStatus =
            executeCommandWithStatus("echo Success && exit 0");
#else
        auto resultWithStatus =
            executeCommandWithStatus("echo Success && exit 0");
#endif
        printCommandResult("Command with Exit Status", resultWithStatus);

        // 6. Simple command execution checking success
        std::cout << "\n[6. Simple Command Execution]" << std::endl;
#ifdef _WIN32
        bool success = executeCommandSimple("echo Test");
#else
        bool success = executeCommandSimple("echo Test");
#endif
        std::cout << "Command succeeded: " << (success ? "Yes" : "No")
                  << std::endl;

        // 7. Check if commands are available
        std::cout << "\n[7. Check Command Availability]" << std::endl;
        std::vector<std::string> commandsToCheck = {
#ifdef _WIN32
            "cmd", "powershell", "notepad", "nonexistentcommand"
#else
            "bash", "ls", "grep", "nonexistentcommand"
#endif
        };

        for (const auto& cmd : commandsToCheck) {
            bool available = isCommandAvailable(cmd);
            std::cout << "Command '" << cmd
                      << "' available: " << (available ? "Yes" : "No")
                      << std::endl;
        }

        // 8. Asynchronous command execution
        std::cout << "\n[8. Asynchronous Command Execution]" << std::endl;
#ifdef _WIN32
        auto futureResult = executeCommandAsync("ping -n 3 127.0.0.1");
#else
        auto futureResult =
            executeCommandAsync("sleep 2 && echo 'Async command completed'");
#endif
        std::cout << "Async command started. Doing other work while waiting..."
                  << std::endl;

        // Do some other work while the command is running
        for (int i = 0; i < 5; i++) {
            std::cout << "Main thread working... " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Get the result
        try {
            std::string asyncResult = futureResult.get();
            printOutput("Async Command Result", asyncResult);
        } catch (const std::exception& e) {
            std::cout << "Async command failed: " << e.what() << std::endl;
        }

        // 9. Pipe commands
        std::cout << "\n[9. Pipe Commands]" << std::endl;
#ifdef _WIN32
        output = pipeCommands("dir /b", "findstr .cpp");
#else
        output = pipeCommands("ls -la", "grep .cpp");
#endif
        printOutput("Piped Commands", output);

        // 10. Multiple commands with common environment
        std::cout << "\n[10. Multiple Commands with Common Environment]"
                  << std::endl;
        std::vector<std::string> commands = {
#ifdef _WIN32
            "echo %CUSTOM_VAR%", "echo Current directory: %CD%"
#else
            "echo $CUSTOM_VAR", "echo Current directory: $PWD"
#endif
        };

        std::unordered_map<std::string, std::string> envVars = {
            {"CUSTOM_VAR", "Hello from environment!"}};

        auto results = executeCommandsWithCommonEnv(commands, envVars, true);
        for (size_t i = 0; i < results.size(); i++) {
            std::ostringstream title;
            title << "Command " << (i + 1) << " with Environment";
            printCommandResult(title.str(), results[i]);
        }

        std::cout << "\n=== Command Execution Suite Complete ===" << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Basic and advanced command execution" << std::endl;
        std::cout << "- Synchronous and asynchronous operations" << std::endl;
        std::cout << "- Input/output handling and streaming" << std::endl;
        std::cout << "- Timeout management" << std::endl;
        std::cout << "- Command availability checking" << std::endl;
        std::cout << "- Command piping and chaining" << std::endl;
        std::cout << "- Environment variable management" << std::endl;
        std::cout << "- Cross-platform compatibility" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
