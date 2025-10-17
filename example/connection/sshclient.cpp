/*
 * sshclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-10-01

Description: Comprehensive example usage of the SSHClient class.
Demonstrates SSH connection, authentication, command execution,
file transfer, and directory operations.

Note: This example requires libssh to be available and SSH server
to be running on the target host.

**************************************************/

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if __has_include(<libssh/libssh.h>)
#include "atom/connection/sshclient.hpp"

// Utility class for formatted logging
class ExampleLogger {
public:
    enum Level { INFO, SUCCESS, WARNING, LOG_ERROR };

    static void write(Level level, const std::string& component,
                      const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                  << "] ";

        switch (level) {
            case Level::Info:
                std::cout << "[INFO] ";
                break;
            case Level::Success:
                std::cout << "[SUCCESS] ";
                break;
            case Level::Warning:
                std::cout << "[WARN] ";
                break;
            case LOG_ERROR:
                std::cout << "[ERROR] ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }
};

// Example 1: Basic SSH connection and authentication
void basicConnectionExample(const std::string& host,
                            const std::string& username,
                            const std::string& password) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Starting basic SSH connection example");

    try {
        // Create SSH client
        atom::connection::SSHClient sshClient(host, 22);

        // Connect with authentication
        ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                             "Connecting to " + host + " as " + username);
        sshClient.connect(username, password, 10);

        if (sshClient.isConnected()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example1",
                                 "Successfully connected to SSH server");

            // Disconnect
            sshClient.disconnect();
            ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                                 "Disconnected from SSH server");
        } else {
            ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example1",
                                 "Failed to connect to SSH server");
        }

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example1",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example1",
                         "Basic connection example completed");
}

// Example 2: Command execution
void commandExecutionExample(const std::string& host,
                             const std::string& username,
                             const std::string& password) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                         "Starting command execution example");

    try {
        atom::connection::SSHClient sshClient(host, 22);
        sshClient.connect(username, password);

        if (sshClient.isConnected()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example2",
                                 "Connected for command execution");

            // Execute single command
            std::vector<std::string> output;
            sshClient.executeCommand("ls -la", output);

            ExampleLogger::write(ExampleLogger::Level::Success, "Example2",
                                 "Command 'ls -la' executed successfully");
            ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                                 "Output:");
            for (const auto& line : output) {
                std::cout << "  " << line << std::endl;
            }

            // Execute multiple commands
            std::vector<std::string> commands = {"pwd", "whoami", "date"};
            std::vector<std::vector<std::string>> multiOutput;
            sshClient.executeCommands(commands, multiOutput);

            ExampleLogger::write(ExampleLogger::Level::Success, "Example2",
                                 "Multiple commands executed successfully");
            for (size_t i = 0; i < commands.size() && i < multiOutput.size();
                 ++i) {
                ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                                     "Command '" + commands[i] + "' output:");
                for (const auto& line : multiOutput[i]) {
                    std::cout << "  " << line << std::endl;
                }
            }

            sshClient.disconnect();
        }

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example2",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example2",
                         "Command execution example completed");
}

// Example 3: File operations
void fileOperationsExample(const std::string& host, const std::string& username,
                           const std::string& password) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                         "Starting file operations example");

    try {
        atom::connection::SSHClient sshClient(host, 22);
        sshClient.connect(username, password);

        if (sshClient.isConnected()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                                 "Connected for file operations");

            // Create a test file locally
            std::string localTestFile = "test_upload.txt";
            std::ofstream testFile(localTestFile);
            testFile
                << "This is a test file for SSH upload/download example.\n";
            testFile
                << "Created at: "
                << std::chrono::system_clock::now().time_since_epoch().count()
                << "\n";
            testFile.close();

            std::string remoteTestFile = "/tmp/test_upload.txt";
            std::string downloadedFile = "test_download.txt";

            // Upload file
            ExampleLogger::write(
                ExampleLogger::Level::Info, "Example3",
                "Uploading file: " + localTestFile + " -> " + remoteTestFile);
            sshClient.uploadFile(localTestFile, remoteTestFile);
            ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                                 "File uploaded successfully");

            // Check if file exists
            if (sshClient.fileExists(remoteTestFile)) {
                ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                                     "Remote file exists: " + remoteTestFile);

                // Get file info
                sftp_attributes attrs;
                sshClient.getFileInfo(remoteTestFile, attrs);
                ExampleLogger::write(
                    ExampleLogger::Level::Info, "Example3",
                    "File size: " + std::to_string(attrs->size) + " bytes");
                sftp_attributes_free(attrs);

                // Download file
                ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                                     "Downloading file: " + remoteTestFile +
                                         " -> " + downloadedFile);
                sshClient.downloadFile(remoteTestFile, downloadedFile);
                ExampleLogger::write(ExampleLogger::Level::Success, "Example3",
                                     "File downloaded successfully");

                // Clean up remote file
                sshClient.removeFile(remoteTestFile);
                ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                                     "Remote file cleaned up");
            }

            // Clean up local files
            std::filesystem::remove(localTestFile);
            std::filesystem::remove(downloadedFile);
            ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                                 "Local files cleaned up");

            sshClient.disconnect();
        }

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example3",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example3",
                         "File operations example completed");
}

// Example 4: Directory operations
void directoryOperationsExample(const std::string& host,
                                const std::string& username,
                                const std::string& password) {
    ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                         "Starting directory operations example");

    try {
        atom::connection::SSHClient sshClient(host, 22);
        sshClient.connect(username, password);

        if (sshClient.isConnected()) {
            ExampleLogger::write(ExampleLogger::Level::Success, "Example4",
                                 "Connected for directory operations");

            std::string testDir = "/tmp/ssh_test_dir";

            // Create directory
            ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                                 "Creating directory: " + testDir);
            sshClient.createDirectory(testDir);
            ExampleLogger::write(ExampleLogger::Level::Success, "Example4",
                                 "Directory created successfully");

            // List directory contents
            ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                                 "Listing /tmp directory contents:");
            auto contents = sshClient.listDirectory("/tmp");
            for (const auto& item : contents) {
                if (item.find("ssh_test") != std::string::npos) {
                    std::cout << "  " << item << std::endl;
                }
            }

            // Remove directory
            sshClient.removeDirectory(testDir);
            ExampleLogger::write(ExampleLogger::Level::Success, "Example4",
                                 "Directory removed successfully");

            sshClient.disconnect();
        }

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Example4",
                             "Exception: " + std::string(e.what()));
    }

    ExampleLogger::write(ExampleLogger::Level::Info, "Example4",
                         "Directory operations example completed");
}

int main() {
    // Configuration - modify these values for your SSH server
    std::string host = "localhost";     // Change to your SSH server
    std::string username = "testuser";  // Change to your username
    std::string password = "testpass";  // Change to your password

    ExampleLogger::write(ExampleLogger::Level::Warning, "Main",
                         "This example requires a running SSH server");
    ExampleLogger::write(
        ExampleLogger::Level::Warning, "Main",
        "Please modify host, username, and password in the source code");
    ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                         "Target: " + username + "@" + host);

    try {
        ExampleLogger::write(ExampleLogger::Level::Info, "Main",
                             "Starting comprehensive SSH client examples");

        // Run all examples
        basicConnectionExample(host, username, password);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        commandExecutionExample(host, username, password);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        fileOperationsExample(host, username, password);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        directoryOperationsExample(host, username, password);

        ExampleLogger::write(ExampleLogger::Level::Success, "Main",
                             "All SSH client examples completed successfully");
        return 0;

    } catch (const std::exception& e) {
        ExampleLogger::write(ExampleLogger::LOG_ERROR, "Main",
                             "Exception: " + std::string(e.what()));
        return 1;
    }
}

#else
int main() {
    std::cout << "SSH client example requires libssh library to be available."
              << std::endl;
    std::cout << "Please install libssh and rebuild with SSH support enabled."
              << std::endl;
    return 0;
}
#endif
