/*
 * sshclient_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the SSHClient class.
Demonstrates all features including:
- Basic connection and authentication
- Command execution (single and multiple)
- File operations (upload, download, check existence)
- Directory operations (create, list, remove)
- File info and rename operations
- Error handling

Performance Notes:
- Command output uses 4KB buffer for better throughput
- File transfers use 64KB buffer for optimal performance
- All SFTP resources are RAII-managed for exception safety

Note: Requires libssh library to be available.

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
#include "atom/connection/ssh/sshclient.hpp"

namespace {

class Logger {
public:
    enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERR, LOG_DEBUG };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] ";

        switch (level) {
            case LOG_INFO:
                std::cout << "[INFO]    ";
                break;
            case LOG_SUCCESS:
                std::cout << "[SUCCESS] ";
                break;
            case LOG_WARNING:
                std::cout << "[WARN]    ";
                break;
            case LOG_ERR:
                std::cout << "[ERROR]   ";
                break;
            case LOG_DEBUG:
                std::cout << "[DEBUG]   ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }
};

}  // namespace

// Example 1: Basic connection and authenticationvoid
// basicConnectionExample(const std::string& host,
                            const std::string& username,
                            const std::string& password) {
                                Logger::log(Logger::LOG_INFO, "Example1",
                                            "=== Basic SSH Connection ===");

                                try {
                                    // Create SSH client with host and port
                                    atom::connection::SSHClient client(host,
                                                                       22);

                                    Logger::log(
                                        Logger::LOG_INFO, "Example1",
                                        "Created SSHClient for host: " + host);
                                    Logger::log(
                                        Logger::LOG_INFO, "Example1",
                                        "isConnected: " +
                                            std::string(client.isConnected()
                                                            ? "yes"
                                                            : "no"));

                                    // Connect with credentials
                                    Logger::log(
                                        Logger::LOG_INFO, "Example1",
                                        "Connecting as user: " + username);
                                    client.connect(username, password, 10);

                                    if (client.isConnected()) {
                                        Logger::log(Logger::LOG_SUCCESS,
                                                    "Example1",
                                                    "Successfully connected to "
                                                    "SSH server");

                                        // Disconnect
                                        client.disconnect();
                                        Logger::log(Logger::LOG_INFO,
                                                    "Example1", "Disconnected");
                                    }

                                } catch (const std::exception& e) {
                                    Logger::log(
                                        Logger::LOG_ERR, "Example1",
                                        "Exception: " + std::string(e.what()));
                                }

                                Logger::log(
                                    Logger::LOG_INFO, "Example1",
                                    "Basic connection example completed\n");
                            }

                            // Example 2: Single command executionvoid
                            // singleCommandExample(const std::string& host,
                            // const std::string& username,
                          const std::string& password) {
                              Logger::log(Logger::LOG_INFO, "Example2",
                                          "=== Single Command Execution ===");

                              try {
                                  atom::connection::SSHClient client(host, 22);
                                  client.connect(username, password);

                                  if (client.isConnected()) {
                                      Logger::log(Logger::LOG_SUCCESS,
                                                  "Example2", "Connected");

                                      // Execute various commands
                                      std::vector<std::string> commands = {
                                          "whoami", "pwd", "date", "uname -a",
                                          "ls -la /tmp"};

                                      for (const auto& cmd : commands) {
                                          Logger::log(Logger::LOG_INFO,
                                                      "Example2",
                                                      "Executing: " + cmd);

                                          std::vector<std::string> output;
                                          client.executeCommand(cmd, output);

                                          Logger::log(Logger::LOG_SUCCESS,
                                                      "Example2",
                                                      "Output (" +
                                                          std::to_string(
                                                              output.size()) +
                                                          " lines):");
                                          for (const auto& line : output) {
                                              std::cout << "  " << line
                                                        << std::endl;
                                          }
                                      }

                                      client.disconnect();
                                  }

                              } catch (const std::exception& e) {
                                  Logger::log(
                                      Logger::LOG_ERR, "Example2",
                                      "Exception: " + std::string(e.what()));
                              }

                              Logger::log(Logger::LOG_INFO, "Example2",
                                          "Single command example completed\n");
                          }

                          // Example 3: Multiple commands executionvoid
                          // multipleCommandsExample(const std::string& host,
                             const std::string& username,
                             const std::string& password) {
                                 Logger::log(
                                     Logger::LOG_INFO, "Example3",
                                     "=== Multiple Commands Execution ===");

                                 try {
                                     atom::connection::SSHClient client(host,
                                                                        22);
                                     client.connect(username, password);

                                     if (client.isConnected()) {
                                         Logger::log(Logger::LOG_SUCCESS,
                                                     "Example3", "Connected");

                                         // Prepare multiple commands
                                         std::vector<std::string> commands = {
                                             "echo 'Step 1: Check system'",
                                             "hostname",
                                             "echo 'Step 2: Check disk'",
                                             "df -h",
                                             "echo 'Step 3: Check memory'",
                                             "free -m"};

                                         Logger::log(Logger::LOG_INFO,
                                                     "Example3",
                                                     "Executing " +
                                                         std::to_string(
                                                             commands.size()) +
                                                         " commands");

                                         std::vector<std::vector<std::string>>
                                             outputs;
                                         client.executeCommands(commands,
                                                                outputs);

                                         Logger::log(Logger::LOG_SUCCESS,
                                                     "Example3",
                                                     "All commands executed");

                                         for (size_t i = 0;
                                              i < commands.size() &&
                                              i < outputs.size();
                                              ++i) {
                                             Logger::log(
                                                 Logger::LOG_INFO, "Example3",
                                                 "Command: " + commands[i]);
                                             for (const auto& line :
                                                  outputs[i]) {
                                                 std::cout << "  " << line
                                                           << std::endl;
                                             }
                                         }

                                         client.disconnect();
                                     }

                                 } catch (const std::exception& e) {
                                     Logger::log(
                                         Logger::LOG_ERR, "Example3",
                                         "Exception: " + std::string(e.what()));
                                 }

                                 Logger::log(
                                     Logger::LOG_INFO, "Example3",
                                     "Multiple commands example completed\n");
                             }

                             // Example 4: File existence checkvoid
                             // fileExistsExample(const std::string& host, const
                             // std::string& username,
                       const std::string& password) {
                           Logger::log(Logger::LOG_INFO, "Example4",
                                       "=== File Existence Check ===");

                           try {
                               atom::connection::SSHClient client(host, 22);
                               client.connect(username, password);

                               if (client.isConnected()) {
                                   Logger::log(Logger::LOG_SUCCESS, "Example4",
                                               "Connected");

                                   // Check various paths
                                   std::vector<std::string> paths = {
                                       "/etc/passwd", "/etc/hosts", "/tmp",
                                       "/nonexistent/path/file.txt", "/home"};

                                   for (const auto& path : paths) {
                                       bool exists = client.fileExists(path);
                                       Logger::log(exists ? Logger::LOG_SUCCESS
                                                          : Logger::LOG_WARNING,
                                                   "Example4",
                                                   path + ": " +
                                                       (exists ? "EXISTS"
                                                               : "NOT FOUND"));
                                   }

                                   client.disconnect();
                               }

                           } catch (const std::exception& e) {
                               Logger::log(
                                   Logger::LOG_ERR, "Example4",
                                   "Exception: " + std::string(e.what()));
                           }

                           Logger::log(Logger::LOG_INFO, "Example4",
                                       "File exists example completed\n");
                       }

                       // Example 5: Directory operationsvoid
                       // directoryOperationsExample(const std::string& host,
                                const std::string& username,
                                const std::string& password) {
                                    Logger::log(Logger::LOG_INFO, "Example5",
                                                "=== Directory Operations ===");

                                    try {
                                        atom::connection::SSHClient client(host,
                                                                           22);
                                        client.connect(username, password);

                                        if (client.isConnected()) {
                                            Logger::log(Logger::LOG_SUCCESS,
                                                        "Example5",
                                                        "Connected");

                                            std::string testDir =
                                                "/tmp/ssh_example_dir_" +
                                                std::to_string(
                                                    std::chrono::system_clock::
                                                        now()
                                                            .time_since_epoch()
                                                            .count());

                                            // Create directory
                                            Logger::log(Logger::LOG_INFO,
                                                        "Example5",
                                                        "Creating directory: " +
                                                            testDir);
                                            client.createDirectory(testDir);
                                            Logger::log(Logger::LOG_SUCCESS,
                                                        "Example5",
                                                        "Directory created");

                                            // List parent directory
                                            Logger::log(Logger::LOG_INFO,
                                                        "Example5",
                                                        "Listing /tmp:");
                                            auto contents =
                                                client.listDirectory("/tmp");
                                            int count = 0;
                                            for (const auto& item : contents) {
                                                if (item.find("ssh_example") !=
                                                    std::string::npos) {
                                                    std::cout << "  " << item
                                                              << std::endl;
                                                    count++;
                                                }
                                            }
                                            Logger::log(
                                                Logger::LOG_INFO, "Example5",
                                                "Found " +
                                                    std::to_string(count) +
                                                    " matching items");

                                            // Remove directory
                                            Logger::log(Logger::LOG_INFO,
                                                        "Example5",
                                                        "Removing directory: " +
                                                            testDir);
                                            client.removeDirectory(testDir);
                                            Logger::log(Logger::LOG_SUCCESS,
                                                        "Example5",
                                                        "Directory removed");

                                            client.disconnect();
                                        }

                                    } catch (const std::exception& e) {
                                        Logger::log(Logger::LOG_ERR, "Example5",
                                                    "Exception: " +
                                                        std::string(e.what()));
                                    }

                                    Logger::log(Logger::LOG_INFO, "Example5",
                                                "Directory operations example "
                                                "completed\n");
                                }

                                // Example 6: File upload and downloadvoid
                                // fileTransferExample(const std::string& host,
                                // const std::string& username,
                         const std::string& password) {
                             Logger::log(Logger::LOG_INFO, "Example6",
                                         "=== File Upload/Download ===");

                             try {
                                 atom::connection::SSHClient client(host, 22);
                                 client.connect(username, password);

                                 if (client.isConnected()) {
                                     Logger::log(Logger::LOG_SUCCESS,
                                                 "Example6", "Connected");

                                     // Create local test file
                                     std::string localFile =
                                         "ssh_test_upload.txt";
                                     std::string remoteFile =
                                         "/tmp/ssh_test_upload.txt";
                                     std::string downloadFile =
                                         "ssh_test_download.txt";

                                     {
                                         std::ofstream ofs(localFile);
                                         ofs << "SSH File Transfer Test\n";
                                         ofs << "Timestamp: "
                                             << std::chrono::system_clock::now()
                                                    .time_since_epoch()
                                                    .count()
                                             << "\n";
                                         ofs << "This file was uploaded via "
                                                "SSHClient.\n";
                                     }
                                     Logger::log(
                                         Logger::LOG_INFO, "Example6",
                                         "Created local file: " + localFile);

                                     // Upload file
                                     Logger::log(Logger::LOG_INFO, "Example6",
                                                 "Uploading: " + localFile +
                                                     " -> " + remoteFile);
                                     client.uploadFile(localFile, remoteFile);
                                     Logger::log(Logger::LOG_SUCCESS,
                                                 "Example6", "File uploaded");

                                     // Check if file exists on remote
                                     if (client.fileExists(remoteFile)) {
                                         Logger::log(Logger::LOG_SUCCESS,
                                                     "Example6",
                                                     "Remote file verified: " +
                                                         remoteFile);

                                         // Get file info
                                         sftp_attributes attrs;
                                         client.getFileInfo(remoteFile, attrs);
                                         Logger::log(
                                             Logger::LOG_INFO, "Example6",
                                             "File size: " +
                                                 std::to_string(attrs->size) +
                                                 " bytes");
                                         sftp_attributes_free(attrs);

                                         // Download file
                                         Logger::log(
                                             Logger::LOG_INFO, "Example6",
                                             "Downloading: " + remoteFile +
                                                 " -> " + downloadFile);
                                         client.downloadFile(remoteFile,
                                                             downloadFile);
                                         Logger::log(Logger::LOG_SUCCESS,
                                                     "Example6",
                                                     "File downloaded");

                                         // Verify downloaded file
                                         if (std::filesystem::exists(
                                                 downloadFile)) {
                                             auto size =
                                                 std::filesystem::file_size(
                                                     downloadFile);
                                             Logger::log(
                                                 Logger::LOG_SUCCESS,
                                                 "Example6",
                                                 "Downloaded file size: " +
                                                     std::to_string(size) +
                                                     " bytes");
                                         }

                                         // Clean up remote file
                                         client.removeFile(remoteFile);
                                         Logger::log(Logger::LOG_INFO,
                                                     "Example6",
                                                     "Remote file removed");
                                     }

                                     // Clean up local files
                                     std::filesystem::remove(localFile);
                                     std::filesystem::remove(downloadFile);
                                     Logger::log(Logger::LOG_INFO, "Example6",
                                                 "Local files cleaned up");

                                     client.disconnect();
                                 }

                             } catch (const std::exception& e) {
                                 Logger::log(
                                     Logger::LOG_ERR, "Example6",
                                     "Exception: " + std::string(e.what()));
                             }

                             Logger::log(Logger::LOG_INFO, "Example6",
                                         "File transfer example completed\n");
                         }

                         // Example 7: File renamevoid fileRenameExample(const
                         // std::string& host, const std::string& username,
                       const std::string& password) {
                           Logger::log(Logger::LOG_INFO, "Example7",
                                       "=== File Rename ===");

                           try {
                               atom::connection::SSHClient client(host, 22);
                               client.connect(username, password);

                               if (client.isConnected()) {
                                   Logger::log(Logger::LOG_SUCCESS, "Example7",
                                               "Connected");

                                   std::string originalFile =
                                       "/tmp/ssh_rename_test.txt";
                                   std::string renamedFile =
                                       "/tmp/ssh_renamed_file.txt";

                                   // Create file via command
                                   std::vector<std::string> output;
                                   client.executeCommand(
                                       "echo 'Rename test' > " + originalFile,
                                       output);
                                   Logger::log(Logger::LOG_INFO, "Example7",
                                               "Created file: " + originalFile);

                                   // Rename file
                                   Logger::log(Logger::LOG_INFO, "Example7",
                                               "Renaming: " + originalFile +
                                                   " -> " + renamedFile);
                                   client.rename(originalFile, renamedFile);
                                   Logger::log(Logger::LOG_SUCCESS, "Example7",
                                               "File renamed");

                                   // Verify
                                   if (client.fileExists(renamedFile)) {
                                       Logger::log(Logger::LOG_SUCCESS,
                                                   "Example7",
                                                   "Renamed file exists: " +
                                                       renamedFile);
                                   }
                                   if (!client.fileExists(originalFile)) {
                                       Logger::log(Logger::LOG_SUCCESS,
                                                   "Example7",
                                                   "Original file removed: " +
                                                       originalFile);
                                   }

                                   // Clean up
                                   client.removeFile(renamedFile);
                                   Logger::log(Logger::LOG_INFO, "Example7",
                                               "Cleaned up");

                                   client.disconnect();
                               }

                           } catch (const std::exception& e) {
                               Logger::log(
                                   Logger::LOG_ERR, "Example7",
                                   "Exception: " + std::string(e.what()));
                           }

                           Logger::log(Logger::LOG_INFO, "Example7",
                                       "File rename example completed\n");
                       }

                       // Example 8: Directory uploadvoid
                       // directoryUploadExample(const std::string& host,
                            const std::string& username,
                            const std::string& password) {
                                Logger::log(Logger::LOG_INFO, "Example8",
                                            "=== Directory Upload ===");

                                try {
                                    atom::connection::SSHClient client(host,
                                                                       22);
                                    client.connect(username, password);

                                    if (client.isConnected()) {
                                        Logger::log(Logger::LOG_SUCCESS,
                                                    "Example8", "Connected");

                                        // Create local directory structure
                                        std::string localDir =
                                            "ssh_upload_test_dir";
                                        std::string remoteDir =
                                            "/tmp/ssh_upload_test_dir";

                                        std::filesystem::create_directories(
                                            localDir + "/subdir1");
                                        std::filesystem::create_directories(
                                            localDir + "/subdir2");

                                        {
                                            std::ofstream(localDir +
                                                          "/file1.txt")
                                                << "File 1 content\n";
                                            std::ofstream(localDir +
                                                          "/file2.txt")
                                                << "File 2 content\n";
                                            std::ofstream(localDir +
                                                          "/subdir1/nested.txt")
                                                << "Nested file\n";
                                        }

                                        Logger::log(Logger::LOG_INFO,
                                                    "Example8",
                                                    "Created local directory "
                                                    "structure: " +
                                                        localDir);

                                        // Upload directory
                                        Logger::log(
                                            Logger::LOG_INFO, "Example8",
                                            "Uploading directory: " + localDir +
                                                " -> " + remoteDir);
                                        client.uploadDirectory(localDir,
                                                               remoteDir);
                                        Logger::log(Logger::LOG_SUCCESS,
                                                    "Example8",
                                                    "Directory uploaded");

                                        // List remote directory
                                        if (client.fileExists(remoteDir)) {
                                            auto contents =
                                                client.listDirectory(remoteDir);
                                            Logger::log(
                                                Logger::LOG_INFO, "Example8",
                                                "Remote directory contents (" +
                                                    std::to_string(
                                                        contents.size()) +
                                                    " items):");
                                            for (const auto& item : contents) {
                                                std::cout << "  " << item
                                                          << std::endl;
                                            }
                                        }

                                        // Clean up
                                        std::vector<std::string> output;
                                        client.executeCommand(
                                            "rm -rf " + remoteDir, output);
                                        std::filesystem::remove_all(localDir);
                                        Logger::log(Logger::LOG_INFO,
                                                    "Example8", "Cleaned up");

                                        client.disconnect();
                                    }

                                } catch (const std::exception& e) {
                                    Logger::log(
                                        Logger::LOG_ERR, "Example8",
                                        "Exception: " + std::string(e.what()));
                                }

                                Logger::log(
                                    Logger::LOG_INFO, "Example8",
                                    "Directory upload example completed\n");
                            }

                            // Example 9: Move semanticsvoid
                            // moveSemanticsExample(const std::string& host,
                            // const std::string& username,
                          const std::string& password) {
                              Logger::log(Logger::LOG_INFO, "Example9",
                                          "=== Move Semantics ===");

                              try {
                                  // Create and connect client
                                  atom::connection::SSHClient client1(host, 22);
                                  client1.connect(username, password);

                                  Logger::log(
                                      Logger::LOG_INFO, "Example9",
                                      "client1 connected: " +
                                          std::string(client1.isConnected()
                                                          ? "yes"
                                                          : "no"));

                                  // Move to new client
                                  atom::connection::SSHClient client2 =
                                      std::move(client1);

                                  Logger::log(Logger::LOG_INFO, "Example9",
                                              "After move:");
                                  Logger::log(
                                      Logger::LOG_INFO, "Example9",
                                      "client2 connected: " +
                                          std::string(client2.isConnected()
                                                          ? "yes"
                                                          : "no"));

                                  // Use moved client
                                  if (client2.isConnected()) {
                                      std::vector<std::string> output;
                                      client2.executeCommand(
                                          "echo 'Hello from moved client'",
                                          output);
                                      if (!output.empty()) {
                                          Logger::log(
                                              Logger::LOG_SUCCESS, "Example9",
                                              "Command output: " + output[0]);
                                      }
                                      client2.disconnect();
                                  }

                              } catch (const std::exception& e) {
                                  Logger::log(
                                      Logger::LOG_ERR, "Example9",
                                      "Exception: " + std::string(e.what()));
                              }

                              Logger::log(Logger::LOG_INFO, "Example9",
                                          "Move semantics example completed\n");
                          }

                          // Example 10: Error handlingvoid
                          // errorHandlingExample(const std::string& host, const
                          // std::string& username,
                          const std::string& password) {
                              Logger::log(Logger::LOG_INFO, "Example10",
                                          "=== Error Handling ===");

                              // Test invalid host
                              try {
                                  Logger::log(Logger::LOG_INFO, "Example10",
                                              "Testing invalid host...");
                                  atom::connection::SSHClient client(
                                      "invalid.host.example", 22);
                                  client.connect("user", "pass", 5);
                              } catch (const std::exception& e) {
                                  Logger::log(
                                      Logger::LOG_WARNING, "Example10",
                                      "Expected error (invalid host): " +
                                          std::string(e.what()));
                              }

                              // Test invalid credentials
                              try {
                                  Logger::log(Logger::LOG_INFO, "Example10",
                                              "Testing invalid credentials...");
                                  atom::connection::SSHClient client(host, 22);
                                  client.connect("invalid_user", "invalid_pass",
                                                 5);
                              } catch (const std::exception& e) {
                                  Logger::log(
                                      Logger::LOG_WARNING, "Example10",
                                      "Expected error (invalid creds): " +
                                          std::string(e.what()));
                              }

                              // Test operations on disconnected client
                              try {
                                  Logger::log(Logger::LOG_INFO, "Example10",
                                              "Testing operations when "
                                              "disconnected...");
                                  atom::connection::SSHClient client(host, 22);
                                  // Don't connect, try to execute command
                                  std::vector<std::string> output;
                                  client.executeCommand("ls", output);
                              } catch (const std::exception& e) {
                                  Logger::log(
                                      Logger::LOG_WARNING, "Example10",
                                      "Expected error (not connected): " +
                                          std::string(e.what()));
                              }

                              Logger::log(Logger::LOG_INFO, "Example10",
                                          "Error handling example completed\n");
                          }

                          int main() {
                              // Configuration - modify these values for your
                              // SSH server
                              std::string host = "localhost";
                              std::string username = "testuser";
                              std::string password = "testpass";

                              Logger::log(
                                  Logger::LOG_INFO, "Main",
                                  "========================================");
                              Logger::log(Logger::LOG_INFO, "Main",
                                          "  SSHClient Comprehensive Examples");
                              Logger::log(
                                  Logger::LOG_INFO, "Main",
                                  "========================================");
                              Logger::log(Logger::LOG_WARNING, "Main",
                                          "Note: Requires SSH server and "
                                          "libssh library");
                              Logger::log(Logger::LOG_INFO, "Main",
                                          "Target: " + username + "@" + host);
                              Logger::log(Logger::LOG_INFO, "Main", "");

                              // Run all examples
                              basicConnectionExample(host, username, password);
                              singleCommandExample(host, username, password);
                              multipleCommandsExample(host, username, password);
                              fileExistsExample(host, username, password);
                              directoryOperationsExample(host, username,
                                                         password);
                              fileTransferExample(host, username, password);
                              fileRenameExample(host, username, password);
                              directoryUploadExample(host, username, password);
                              moveSemanticsExample(host, username, password);
                              errorHandlingExample(host, username, password);

                              Logger::log(
                                  Logger::LOG_SUCCESS, "Main",
                                  "========================================");
                              Logger::log(
                                  Logger::LOG_SUCCESS, "Main",
                                  "  All SSHClient examples completed!");
                              Logger::log(
                                  Logger::LOG_SUCCESS, "Main",
                                  "========================================");

                              return 0;
                          }

#elseint main() {
                          std::cout
                              << "========================================"
                              << std::endl;
                          std::cout << "  SSHClient Example" << std::endl;
                          std::cout
                              << "========================================"
                              << std::endl;
                          std::cout << std::endl;
                          std::cout << "This example requires libssh library "
                                       "to be available."
                                    << std::endl;
                          std::cout << "Please install libssh and rebuild with "
                                       "SSH support enabled."
                                    << std::endl;
                          std::cout << std::endl;
                          std::cout << "Installation:" << std::endl;
                          std::cout
                              << "  Ubuntu/Debian: sudo apt install libssh-dev"
                              << std::endl;
                          std::cout << "  Fedora/RHEL:   sudo dnf install "
                                       "libssh-devel"
                                    << std::endl;
                          std::cout << "  macOS:         brew install libssh"
                                    << std::endl;
                          std::cout << "  Windows:       vcpkg install libssh"
                                    << std::endl;
                          return 0;
                          }
#endif
