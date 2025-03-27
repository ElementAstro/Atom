#include "atom/system/command.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Helper function to print command output
void printOutput(const std::string& title, const std::string& output) {
    std::cout << "\n=== " << title << " ===\n";
    std::cout << output << std::endl;
    std::cout << "=== End of " << title << " ===\n";
}

// Helper function to print command results with status
void printCommandResult(const std::string& title,
                        const std::pair<std::string, int>& result) {
    std::cout << "\n=== " << title << " ===\n";
    std::cout << "Exit Status: " << result.second << std::endl;
    std::cout << "Output:\n" << result.first << std::endl;
    std::cout << "=== End of " << title << " ===\n";
}

// Line processing callback example
void lineProcessor(const std::string& line) {
    std::cout << "Processing line: " << line << std::endl;
}

int main() {
    using namespace atom::system;

    std::cout << "ATOM SYSTEM COMMAND UTILITIES EXAMPLES\n";
    std::cout << "======================================\n";

    try {
        // Basic command execution
        std::cout << "\n[1. Basic Command Execution]\n";
        std::string output = executeCommand("ls -la");
        printOutput("Basic Command Execution", output);

        // Execute command with line processing callback
        std::cout << "\n[2. Command Execution with Line Callback]\n";
        output = executeCommand("ls -la", false, lineProcessor);

        // Execute command with input
        std::cout << "\n[3. Command Execution with Input]\n";
        output = executeCommandWithInput("grep a", "apple\nbanana\ncherry");
        printOutput("Command with Input", output);

        // Execute command with environment variables
        std::cout << "\n[4. Command with Environment Variables]\n";
        std::unordered_map<std::string, std::string> envVars = {
            {"CUSTOM_VAR", "custom_value"}, {"ANOTHER_VAR", "another_value"}};
        output =
            executeCommandWithEnv("echo $CUSTOM_VAR $ANOTHER_VAR", envVars);
        printOutput("Command with Environment", output);

        // Execute command with exit status
        std::cout << "\n[5. Command with Exit Status]\n";
        auto resultWithStatus = executeCommandWithStatus("ls -la");
        printCommandResult("Command with Exit Status", resultWithStatus);

        // Simple command execution checking success
        std::cout << "\n[6. Simple Command Execution]\n";
        bool success = executeCommandSimple("ls -la");
        std::cout << "Command succeeded: " << (success ? "Yes" : "No")
                  << std::endl;

        // Check if command is available
        std::cout << "\n[7. Check if Commands are Available]\n";
        std::cout << "ls command available: "
                  << (isCommandAvailable("ls") ? "Yes" : "No") << std::endl;
        std::cout << "nonexistentcmd available: "
                  << (isCommandAvailable("nonexistentcmd") ? "Yes" : "No")
                  << std::endl;

        // Execute multiple commands
        std::cout << "\n[8. Execute Multiple Commands]\n";
        std::vector<std::string> commands = {
            "echo 'Command 1'", "echo 'Command 2'", "echo 'Command 3'"};
        executeCommands(commands);

        // Get command output as lines
        std::cout << "\n[9. Get Command Output as Lines]\n";
        std::vector<std::string> lines = executeCommandGetLines("ls -la");
        std::cout << "Command output lines:\n";
        for (const auto& line : lines) {
            std::cout << "Line: " << line << std::endl;
        }

        // Pipe commands
        std::cout << "\n[10. Pipe Commands]\n";
        output = pipeCommands("ls -la", "grep .cpp");
        printOutput("Piped Commands", output);

        // Asynchronous command execution
        std::cout << "\n[11. Asynchronous Command Execution]\n";
        auto futureResult =
            executeCommandAsync("sleep 2 && echo 'Async command completed'");
        std::cout
            << "Async command started. Doing other work while waiting...\n";

        // Do some other work while the command is running
        for (int i = 0; i < 5; i++) {
            std::cout << "Main thread working... " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Wait for the async command to complete and get its result
        output = futureResult.get();
        printOutput("Async Command Result", output);

        // Command with timeout
        std::cout << "\n[12. Command with Timeout]\n";
        std::cout << "Executing quick command with timeout:\n";
        auto timeoutResult = executeCommandWithTimeout(
            "echo 'Quick command'", std::chrono::milliseconds(5000));
        if (timeoutResult) {
            printOutput("Command completed within timeout", *timeoutResult);
        } else {
            std::cout << "Command timed out!\n";
        }

        std::cout << "Executing slow command with timeout:\n";
        timeoutResult = executeCommandWithTimeout(
            "sleep 3 && echo 'Slow command'", std::chrono::milliseconds(1000));
        if (timeoutResult) {
            printOutput("Command completed within timeout", *timeoutResult);
        } else {
            std::cout << "Command timed out as expected!\n";
        }

        // Execute commands with common environment
        std::cout << "\n[13. Execute Commands with Common Environment]\n";
        commands = {"echo $ENV_VAR1", "echo $ENV_VAR2",
                    "echo 'Simple command'"};
        envVars = {{"ENV_VAR1", "Environment Variable 1"},
                   {"ENV_VAR2", "Environment Variable 2"}};
        auto commandResults = executeCommandsWithCommonEnv(commands, envVars);

        for (size_t i = 0; i < commandResults.size(); i++) {
            std::cout << "Command " << i + 1
                      << " exit status: " << commandResults[i].second
                      << std::endl;
            std::cout << "Command " << i + 1
                      << " output: " << commandResults[i].first << std::endl;
        }

        // Find processes by substring
        std::cout << "\n[14. Find Processes by Substring]\n";
        auto processes = getProcessesBySubstring("bash");
        std::cout << "Processes containing 'bash':\n";
        for (const auto& process : processes) {
            std::cout << "PID: " << process.first
                      << ", Name: " << process.second << std::endl;
        }

        // Command stream execution with termination condition
        std::cout << "\n[15. Command Stream Execution]\n";
        int status = 0;
        std::atomic<int> lineCount = 0;

        auto lineCountProcessor = [&lineCount](const std::string& line) {
            std::cout << "Stream line: " << line << std::endl;
            lineCount++;
        };

        auto terminateCondition = [&lineCount]() {
            return lineCount >= 5;  // Terminate after 5 lines
        };

        output = executeCommandStream("ls -la", false, lineCountProcessor,
                                      status, terminateCondition);
        std::cout << "Stream execution status: " << status << std::endl;

        // Command history
        std::cout << "\n[16. Command History]\n";
        auto history =
            createCommandHistory(10);  // History with maximum 10 entries

        // Add commands to history
        history->addCommand("ls -la", 0);
        history->addCommand("grep pattern file.txt", 1);
        history->addCommand("echo 'Hello World'", 0);

        // Get last commands
        std::cout << "Last 2 commands:\n";
        auto lastCommands = history->getLastCommands(2);
        for (const auto& cmd : lastCommands) {
            std::cout << "Command: " << cmd.first
                      << ", Exit Status: " << cmd.second << std::endl;
        }

        // Search commands
        std::cout << "\nCommands containing 'echo':\n";
        auto searchResults = history->searchCommands("echo");
        for (const auto& cmd : searchResults) {
            std::cout << "Command: " << cmd.first
                      << ", Exit Status: " << cmd.second << std::endl;
        }

        std::cout << "\nHistory size: " << history->size() << std::endl;

        // Clear history
        history->clear();
        std::cout << "History after clearing, size: " << history->size()
                  << std::endl;

        std::cout << "\n[17. Process Management]\n";
        // Note: These operations require appropriate permissions and should be
        // used cautiously. We'll just demonstrate how to call them.

        // Start a process
        std::cout << "Starting a background process...\n";
        auto processInfo = startProcess("sleep 10");
        std::cout << "Started process with PID: " << processInfo.first
                  << std::endl;

        // Using process ID to kill
        std::cout
            << "Demonstrating how to kill a process (not actually killing):\n";
        std::cout << "killProcessByPID(" << processInfo.first
                  << ", 15) would send SIGTERM\n";

        // Using process name to kill
        std::cout
            << "killProcessByName(\"processname\", 15) would kill by name\n";

        std::cout << "\nAll examples completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}