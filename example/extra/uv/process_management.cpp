#include "atom/extra/uv/subprocess.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== UV Process Management Example ===" << std::endl;

        // 1. Basic process execution
        std::cout << "\n1. Basic Process Execution:" << std::endl;
        {
            UvProcess process;

            bool process_completed = false;
            std::string stdout_output;
            std::string stderr_output;
            int exit_code = 0;

            // Set up callbacks
            auto exit_callback = [&](int64_t status, int signal) {
                std::cout << "Process exited with status: " << status
                          << ", signal: " << signal << std::endl;
                exit_code = static_cast<int>(status);
                process_completed = true;
            };

            auto stdout_callback = [&](const char* data, ssize_t size) {
                if (size > 0) {
                    std::string chunk(data, size);
                    stdout_output += chunk;
                    std::cout << "STDOUT: " << chunk;
                }
            };

            auto stderr_callback = [&](const char* data, ssize_t size) {
                if (size > 0) {
                    std::string chunk(data, size);
                    stderr_output += chunk;
                    std::cout << "STDERR: " << chunk;
                }
            };

            // Spawn a simple echo command
            std::vector<std::string> args = {"echo", "Hello from subprocess!"};
            bool success = process.spawn("echo", args, "", exit_callback,
                                         stdout_callback, stderr_callback);

            if (success) {
                std::cout << "Process spawned successfully" << std::endl;

                // Wait for process to complete
                while (!process_completed &&
                       process.getStatus() ==
                           UvProcess::ProcessStatus::RUNNING) {
                    std::this_thread::sleep_for(10ms);
                }

                std::cout << "Process completed with exit code: " << exit_code
                          << std::endl;
                std::cout << "Total stdout: " << stdout_output << std::endl;
            } else {
                std::cerr << "Failed to spawn process" << std::endl;
            }
        }

        // 2. Process with working directory
        std::cout << "\n2. Process with Working Directory:" << std::endl;
        {
            UvProcess process;

            bool process_completed = false;
            std::string output;

            auto exit_callback = [&](int64_t status, int signal) {
                std::cout << "Directory listing process completed" << std::endl;
                process_completed = true;
            };

            auto stdout_callback = [&](const char* data, ssize_t size) {
                if (size > 0) {
                    output += std::string(data, size);
                }
            };

            // List current directory contents
#ifdef _WIN32
            std::vector<std::string> args = {"dir"};
            bool success = process.spawn("cmd", {"/c", "dir"}, ".",
                                         exit_callback, stdout_callback);
#else
            std::vector<std::string> args = {"ls", "-la"};
            bool success =
                process.spawn("ls", args, ".", exit_callback, stdout_callback);
#endif

            if (success) {
                while (!process_completed &&
                       process.getStatus() ==
                           UvProcess::ProcessStatus::RUNNING) {
                    std::this_thread::sleep_for(10ms);
                }

                std::cout << "Directory listing output:" << std::endl;
                std::cout << output << std::endl;
            }
        }

        // 3. Process with environment variables
        std::cout << "\n3. Process with Environment Variables:" << std::endl;
        {
            UvProcess process;

            UvProcess::ProcessOptions options;
#ifdef _WIN32
            options.file = "cmd";
            options.args = {"/c", "echo", "%CUSTOM_VAR%"};
#else
            options.file = "sh";
            options.args = {"-c", "echo $CUSTOM_VAR"};
#endif
            options.env["CUSTOM_VAR"] = "Hello from environment!";
            options.env["ANOTHER_VAR"] = "Another value";
            options.inherit_parent_env = true;

            bool process_completed = false;
            std::string output;

            auto exit_callback = [&](int64_t status, int signal) {
                process_completed = true;
            };

            auto stdout_callback = [&](const char* data, ssize_t size) {
                if (size > 0) {
                    output += std::string(data, size);
                }
            };

            bool success = process.spawnWithOptions(options, exit_callback,
                                                    stdout_callback);

            if (success) {
                while (!process_completed &&
                       process.getStatus() ==
                           UvProcess::ProcessStatus::RUNNING) {
                    std::this_thread::sleep_for(10ms);
                }

                std::cout << "Environment variable output: " << output
                          << std::endl;
            }
        }

        // 4. Interactive process with stdin
        std::cout << "\n4. Interactive Process with Stdin:" << std::endl;
        {
            UvProcess process;

            bool process_completed = false;
            std::string output;

            auto exit_callback = [&](int64_t status, int signal) {
                std::cout << "Interactive process completed" << std::endl;
                process_completed = true;
            };

            auto stdout_callback = [&](const char* data, ssize_t size) {
                if (size > 0) {
                    std::string chunk(data, size);
                    output += chunk;
                    std::cout << "Process output: " << chunk;
                }
            };

#ifdef _WIN32
            // Use findstr on Windows (similar to grep)
            std::vector<std::string> args = {"findstr", "test"};
            bool success = process.spawn("findstr", args, "", exit_callback,
                                         stdout_callback);
#else
            // Use grep to filter input
            std::vector<std::string> args = {"grep", "test"};
            bool success =
                process.spawn("grep", args, "", exit_callback, stdout_callback);
#endif

            if (success) {
                // Send some input to the process
                process.writeToStdin("This is a test line\n");
                process.writeToStdin("This line should be filtered\n");
                process.writeToStdin("Another test line\n");
                process.writeToStdin("Final line without keyword\n");

                // Close stdin to signal end of input
                process.closeStdin();

                // Wait for process to complete
                while (!process_completed &&
                       process.getStatus() ==
                           UvProcess::ProcessStatus::RUNNING) {
                    std::this_thread::sleep_for(10ms);
                }

                std::cout << "Interactive process completed" << std::endl;
            }
        }

        // 5. Process with timeout
        std::cout << "\n5. Process with Timeout:" << std::endl;
        {
            UvProcess process;

            UvProcess::ProcessOptions options;
#ifdef _WIN32
            options.file = "ping";
            options.args = {"ping", "-n", "10", "127.0.0.1"};  // Ping 10 times
#else
            options.file = "sleep";
            options.args = {"sleep", "5"};  // Sleep for 5 seconds
#endif
            options.timeout = 2s;  // 2 second timeout

            bool process_completed = false;
            bool timed_out = false;

            auto exit_callback = [&](int64_t status, int signal) {
                std::cout << "Timeout process exited with status: " << status
                          << std::endl;
                process_completed = true;
            };

            auto timeout_callback = [&]() {
                std::cout << "Process timed out!" << std::endl;
                timed_out = true;
            };

            auto error_callback = [&](const std::string& error) {
                std::cout << "Process error: " << error << std::endl;
            };

            bool success = process.spawnWithOptions(
                options, exit_callback, nullptr, nullptr, timeout_callback,
                error_callback);

            if (success) {
                auto start_time = std::chrono::steady_clock::now();

                while (!process_completed && !timed_out &&
                       process.getStatus() ==
                           UvProcess::ProcessStatus::RUNNING) {
                    std::this_thread::sleep_for(10ms);
                }

                auto end_time = std::chrono::steady_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time);

                std::cout << "Process ran for " << duration.count() << "ms"
                          << std::endl;
                std::cout << "Final status: ";
                switch (process.getStatus()) {
                    case UvProcess::ProcessStatus::EXITED:
                        std::cout << "EXITED" << std::endl;
                        break;
                    case UvProcess::ProcessStatus::TERMINATED:
                        std::cout << "TERMINATED" << std::endl;
                        break;
                    case UvProcess::ProcessStatus::TIMED_OUT:
                        std::cout << "TIMED_OUT" << std::endl;
                        break;
                    default:
                        std::cout << "OTHER" << std::endl;
                        break;
                }
            }
        }

        // 6. Multiple concurrent processes
        std::cout << "\n6. Multiple Concurrent Processes:" << std::endl;
        {
            std::vector<std::unique_ptr<UvProcess>> processes;
            std::vector<bool> completed_flags;
            std::vector<std::string> outputs;

            const int num_processes = 3;
            processes.reserve(num_processes);
            completed_flags.resize(num_processes, false);
            outputs.resize(num_processes);

            for (int i = 0; i < num_processes; ++i) {
                auto process = std::make_unique<UvProcess>();

                auto exit_callback = [&completed_flags, i](int64_t status,
                                                           int signal) {
                    std::cout << "Process " << i
                              << " completed with status: " << status
                              << std::endl;
                    completed_flags[i] = true;
                };

                auto stdout_callback = [&outputs, i](const char* data,
                                                     ssize_t size) {
                    if (size > 0) {
                        outputs[i] += std::string(data, size);
                    }
                };

                // Each process echoes a different message
                std::vector<std::string> args = {
                    "echo", "Message from process " + std::to_string(i)};
                bool success = process->spawn("echo", args, "", exit_callback,
                                              stdout_callback);

                if (success) {
                    processes.push_back(std::move(process));
                } else {
                    std::cerr << "Failed to spawn process " << i << std::endl;
                }
            }

            // Wait for all processes to complete
            bool all_completed = false;
            while (!all_completed) {
                all_completed = true;
                for (size_t i = 0; i < processes.size(); ++i) {
                    if (!completed_flags[i] &&
                        processes[i]->getStatus() ==
                            UvProcess::ProcessStatus::RUNNING) {
                        all_completed = false;
                    }
                }
                std::this_thread::sleep_for(10ms);
            }

            std::cout << "All processes completed. Outputs:" << std::endl;
            for (size_t i = 0; i < outputs.size(); ++i) {
                std::cout << "  Process " << i << ": " << outputs[i];
            }
        }

        std::cout << "\n=== UV Process Management Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
