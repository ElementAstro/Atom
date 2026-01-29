/**
 * @file qprocess_example.cpp
 * @brief Examples for atom::utils QProcess
 */

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include "atom/utils/process/qprocess.hpp"

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateBasicProcess() {
    printSection("1. Basic Process Execution");

    QProcess process;

    std::cout << "Creating a simple process..." << std::endl;

#ifdef _WIN32
    std::string command = "cmd";
    std::vector<std::string> args = {"/c", "echo", "Hello from QProcess!"};
#else
    std::string command = "echo";
    std::vector<std::string> args = {"Hello from QProcess!"};
#endif

    std::cout << "Command: " << command << std::endl;

    process.start(command, args);

    if (process.waitForFinished(std::chrono::seconds(5))) {
        std::cout << "Process finished successfully" << std::endl;
        std::cout << "Exit code: " << process.exitCode() << std::endl;
        std::cout << "Output: " << process.readAllStandardOutput() << std::endl;
    } else {
        std::cout << "Process timed out or failed" << std::endl;
    }
}

void demonstrateWorkingDirectory() {
    printSection("2. Working Directory");

    QProcess process;

    std::cout << "Setting working directory..." << std::endl;

#ifdef _WIN32
    process.setWorkingDirectory("C:\\");
    std::cout << "Working directory: C:\\" << std::endl;
#else
    process.setWorkingDirectory("/tmp");
    std::cout << "Working directory: /tmp" << std::endl;
#endif

    auto workDir = process.workingDirectory();
    if (workDir) {
        std::cout << "Confirmed: " << *workDir << std::endl;
    }
}

void demonstrateEnvironment() {
    printSection("3. Environment Variables");

    QProcess process;

    std::cout << "Setting environment variables..." << std::endl;

    std::vector<std::string> env = {"MY_VAR=hello", "MY_NUMBER=42",
                                    "MY_PATH=/custom/path"};

    process.setEnvironment(env);

    std::cout << "Environment set:" << std::endl;
    for (const auto& var : env) {
        std::cout << "  " << var << std::endl;
    }
}

void demonstrateCallbacks() {
    printSection("4. Process Callbacks");

    QProcess process;

    std::cout << "Setting up callbacks..." << std::endl;

    process.setStartedCallback(
        []() { std::cout << "  [Callback] Process started!" << std::endl; });

    process.setFinishedCallback([](int exitCode, QProcess::ExitStatus status) {
        std::cout << "  [Callback] Process finished with code: " << exitCode
                  << std::endl;
        std::cout << "  [Callback] Status: "
                  << (status == QProcess::ExitStatus::NormalExit ? "Normal"
                                                                 : "Crash")
                  << std::endl;
    });

    process.setErrorCallback([](QProcess::ProcessError error) {
        std::cout << "  [Callback] Error occurred: " << static_cast<int>(error)
                  << std::endl;
    });

    process.setReadyReadStandardOutputCallback([](std::string_view data) {
        std::cout << "  [Callback] stdout: " << data << std::endl;
    });

    process.setReadyReadStandardErrorCallback([](std::string_view data) {
        std::cout << "  [Callback] stderr: " << data << std::endl;
    });

    std::cout << "Callbacks configured" << std::endl;
}

void demonstrateProcessState() {
    printSection("5. Process States");

    std::cout << "QProcess states:" << std::endl;
    std::cout << "  NotRunning - Process is not running" << std::endl;
    std::cout << "  Starting   - Process is starting" << std::endl;
    std::cout << "  Running    - Process is running" << std::endl;

    QProcess process;
    std::cout << "\nInitial state: NotRunning" << std::endl;

    auto state = process.state();
    std::cout << "Current state: "
              << (state == QProcess::ProcessState::NotRunning ? "NotRunning"
                  : state == QProcess::ProcessState::Starting ? "Starting"
                                                              : "Running")
              << std::endl;
}

void demonstrateProcessErrors() {
    printSection("6. Process Errors");

    std::cout << "QProcess error types:" << std::endl;
    std::cout << "  NoError       - No error occurred" << std::endl;
    std::cout << "  FailedToStart - Process failed to start" << std::endl;
    std::cout << "  Crashed       - Process crashed" << std::endl;
    std::cout << "  Timedout      - Operation timed out" << std::endl;
    std::cout << "  ReadError     - Error reading from process" << std::endl;
    std::cout << "  WriteError    - Error writing to process" << std::endl;
    std::cout << "  UnknownError  - Unknown error" << std::endl;
}

void demonstrateInputOutput() {
    printSection("7. Process I/O");

    std::cout << "Reading and writing to process:" << std::endl;
    std::cout << R"(
    QProcess process;
    process.start("interactive_program");

    // Write to stdin
    process.write("input data\n");

    // Read from stdout
    std::string output = process.readAllStandardOutput();

    // Read from stderr
    std::string errors = process.readAllStandardError();

    // Read line by line
    while (process.canReadLine()) {
        std::string line = process.readLine();
        // Process line...
    }
    )" << std::endl;
}

void demonstrateAsyncExecution() {
    printSection("8. Async Process Execution");

    std::cout << "Running process asynchronously:" << std::endl;
    std::cout << R"(
    QProcess process;

    // Start without waiting
    process.start("long_running_task");

    // Do other work while process runs
    while (process.state() == QProcess::ProcessState::Running) {
        // Check for output
        if (process.bytesAvailable() > 0) {
            std::cout << process.readAllStandardOutput();
        }

        // Do other work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Process finished
    std::cout << "Exit code: " << process.exitCode() << std::endl;
    )" << std::endl;
}

void demonstrateProcessChaining() {
    printSection("9. Process Chaining Example");

    std::cout << "Chaining processes (pipe-like):" << std::endl;
    std::cout << R"(
    // First process
    QProcess process1;
    process1.start("generate_data");
    process1.waitForFinished();
    std::string data = process1.readAllStandardOutput();

    // Second process uses first's output
    QProcess process2;
    process2.start("process_data");
    process2.write(data);
    process2.closeWriteChannel();
    process2.waitForFinished();

    // Get final result
    std::string result = process2.readAllStandardOutput();
    )" << std::endl;
}

void demonstrateRealWorldExample() {
    printSection("10. Real-World Example: Git Status");

    std::cout << "Getting git repository status:" << std::endl;
    std::cout << R"(
    QProcess git;
    git.setWorkingDirectory("/path/to/repo");
    git.start("git", {"status", "--porcelain"});

    if (git.waitForFinished(std::chrono::seconds(10))) {
        std::string output = git.readAllStandardOutput();

        if (output.empty()) {
            std::cout << "Working directory clean" << std::endl;
        } else {
            std::cout << "Modified files:" << std::endl;
            // Parse output...
        }
    }
    )" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  QProcess Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicProcess();
        demonstrateWorkingDirectory();
        demonstrateEnvironment();
        demonstrateCallbacks();
        demonstrateProcessState();
        demonstrateProcessErrors();
        demonstrateInputOutput();
        demonstrateAsyncExecution();
        demonstrateProcessChaining();
        demonstrateRealWorldExample();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All QProcess examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
