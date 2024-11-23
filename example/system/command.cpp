#include "atom/system/command.hpp"

#include <iostream>
#include <unordered_map>
#include <vector>

using namespace atom::system;

int main() {
    // Execute a command and get the output
    try {
        std::string output = executeCommand("echo Hello, World!");
        std::cout << "Command output: " << output << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error executing command: " << e.what() << std::endl;
    }

    // Execute a command with input and get the output
    try {
        std::string output = executeCommandWithInput("cat", "Hello, World!");
        std::cout << "Command output with input: " << output << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error executing command with input: " << e.what()
                  << std::endl;
    }

    // Execute a command and get the output with status
    try {
        int status;
        std::string output = executeCommandStream(
            "echo Hello, Stream!", false,
            [](const std::string& line) {
                std::cout << "Processing line: " << line << std::endl;
            },
            status);
        std::cout << "Command output stream: " << output
                  << ", status: " << status << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error executing command stream: " << e.what()
                  << std::endl;
    }

    // Execute a list of commands
    try {
        std::vector<std::string> commands = {"echo Command 1", "echo Command 2",
                                             "echo Command 3"};
        executeCommands(commands);
    } catch (const std::runtime_error& e) {
        std::cerr << "Error executing commands: " << e.what() << std::endl;
    }

    // Kill a process by its name
    try {
        killProcessByName("some_process", SIGTERM);
    } catch (const std::runtime_error& e) {
        std::cerr << "Error killing process by name: " << e.what() << std::endl;
    }

    // Kill a process by its PID
    try {
        killProcessByPID(12345, SIGTERM);
    } catch (const std::runtime_error& e) {
        std::cerr << "Error killing process by PID: " << e.what() << std::endl;
    }

    // Execute a command with environment variables and get the output
    try {
        std::unordered_map<std::string, std::string> envVars = {
            {"VAR1", "value1"}, {"VAR2", "value2"}};
        std::string output = executeCommandWithEnv("printenv VAR1", envVars);
        std::cout << "Command output with env: " << output << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error executing command with env: " << e.what()
                  << std::endl;
    }

    // Execute a command and get the output along with the exit status
    try {
        auto [output, status] = executeCommandWithStatus("echo Status Check");
        std::cout << "Command output with status: " << output
                  << ", status: " << status << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error executing command with status: " << e.what()
                  << std::endl;
    }

    // Execute a command and get a boolean indicating success
    try {
        bool success = executeCommandSimple("echo Simple Command");
        std::cout << "Command success: " << std::boolalpha << success
                  << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error executing simple command: " << e.what()
                  << std::endl;
    }

    // Start a process and get the process ID and handle
    try {
        auto [pid, handle] = startProcess("sleep 10");
        std::cout << "Started process with PID: " << pid << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error starting process: " << e.what() << std::endl;
    }

    // Check if a command is available in the system
    bool isAvailable = isCommandAvailable("echo");
    std::cout << "Is 'echo' command available: " << std::boolalpha
              << isAvailable << std::endl;

    return 0;
}