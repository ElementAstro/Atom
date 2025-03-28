/*
 * qprocess_example.cpp
 *
 * This example demonstrates the usage of the QProcess class for managing
 * external processes. It covers process creation, input/output handling,
 * environment management, and more across different platforms.
 *
 * Copyright (C) 2024 Example User
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"
#include "atom/utils/qprocess.hpp"

// Platform-specific commands and utilities
#ifdef _WIN32
const std::string ECHO_COMMAND = "cmd.exe";
const std::vector<std::string> ECHO_ARGS = {"/c", "echo"};
const std::string LIST_DIR_COMMAND = "cmd.exe";
const std::vector<std::string> LIST_DIR_ARGS = {"/c", "dir"};
const std::string SLEEP_COMMAND = "timeout.exe";
const std::string CAT_COMMAND = "type";
const std::string ENV_VAR_FORMAT = "{}={}";
#else
const std::string ECHO_COMMAND = "/bin/echo";
const std::vector<std::string> ECHO_ARGS = {};
const std::string LIST_DIR_COMMAND = "ls";
const std::vector<std::string> LIST_DIR_ARGS = {"-la"};
const std::string SLEEP_COMMAND = "sleep";
const std::string CAT_COMMAND = "cat";
const std::string ENV_VAR_FORMAT = "{}={}";
#endif

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

// Helper function to print command output
void printOutput(const std::string& stdout_output,
                 const std::string& stderr_output) {
    std::cout << "Standard Output:" << std::endl;
    if (stdout_output.empty()) {
        std::cout << "(empty)" << std::endl;
    } else {
        std::cout << stdout_output << std::endl;
    }

    std::cout << "Standard Error:" << std::endl;
    if (stderr_output.empty()) {
        std::cout << "(empty)" << std::endl;
    } else {
        std::cout << stderr_output << std::endl;
    }
    std::cout << std::endl;
}

// Helper to create a temporary file with content
std::string createTempFile(const std::string& content) {
    std::string filename =
        "qprocess_temp_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()) +
        ".txt";
    std::ofstream file(filename);
    file << content;
    file.close();
    return filename;
}

// Helper to format environment variables
std::string formatEnvVar(const std::string& name, const std::string& value) {
    return std::string(ENV_VAR_FORMAT)
        .replace(ENV_VAR_FORMAT.find("{}"), 2, name)
        .replace(ENV_VAR_FORMAT.find("{}"), 2, value);
}

int main(int argc, char* argv[]) {
    // Initialize loguru
    loguru::g_stderr_verbosity = 1;
    loguru::init(argc, argv);

    std::cout << "======================================================="
              << std::endl;
    std::cout << "QProcess Comprehensive Usage Example" << std::endl;
    std::cout << "======================================================="
              << std::endl;

    // ==========================================
    // 1. Basic Process Execution
    // ==========================================
    printSection("Basic Process Execution");

    // Create a QProcess instance
    atom::utils::QProcess echoProcess;

    std::cout << "Executing echo command with 'Hello, World!'..." << std::endl;

    // Start the process with arguments
    std::vector<std::string> args = ECHO_ARGS;
    args.push_back("Hello, World!");

    echoProcess.start(ECHO_COMMAND, args);

    // Wait for process to finish (with timeout)
    if (!echoProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr << "Process did not finish within timeout period."
                  << std::endl;
        echoProcess.terminate();
    }

    // Read output
    std::string stdout_output = echoProcess.readAllStandardOutput();
    std::string stderr_output = echoProcess.readAllStandardError();

    printOutput(stdout_output, stderr_output);

    // ==========================================
    // 2. Setting Working Directory
    // ==========================================
    printSection("Setting Working Directory");

    atom::utils::QProcess dirProcess;

    // Get the current directory for display
    auto currentDir = std::filesystem::current_path().string();
    std::cout << "Current directory: " << currentDir << std::endl;

    // Set working directory to the parent directory
    auto parentDir = std::filesystem::current_path().parent_path().string();
    std::cout << "Setting working directory to: " << parentDir << std::endl;

    try {
        dirProcess.setWorkingDirectory(parentDir);
        dirProcess.start(LIST_DIR_COMMAND, LIST_DIR_ARGS);

        if (!dirProcess.waitForFinished(std::chrono::seconds(5))) {
            std::cerr << "Directory listing process did not finish within "
                         "timeout period."
                      << std::endl;
            dirProcess.terminate();
        }

        stdout_output = dirProcess.readAllStandardOutput();
        stderr_output = dirProcess.readAllStandardError();

        printOutput(stdout_output, stderr_output);
    } catch (const std::exception& e) {
        std::cerr << "Error setting working directory: " << e.what()
                  << std::endl;
    }

    // ==========================================
    // 3. Environment Variables
    // ==========================================
    printSection("Environment Variables");

    atom::utils::QProcess envProcess;

    // Set custom environment variables
    std::vector<std::string> environment = {
        formatEnvVar("QPROCESS_TEST_VAR1", "Hello"),
        formatEnvVar("QPROCESS_TEST_VAR2", "World"),
        formatEnvVar("QPROCESS_TEST_VAR3", "From QProcess")};

    std::cout << "Setting environment variables:" << std::endl;
    for (const auto& env : environment) {
        std::cout << "  " << env << std::endl;
    }

    envProcess.setEnvironment(environment);

    // Execute a command that displays environment variables
#ifdef _WIN32
    // On Windows, use 'set' to display environment variables
    envProcess.start("cmd.exe", {"/c", "set", "QPROCESS_TEST"});
#else
    // On Unix-like systems, use 'env | grep' to display environment variables
    envProcess.start("/bin/sh", {"-c", "env | grep QPROCESS_TEST"});
#endif

    if (!envProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr << "Environment process did not finish within timeout period."
                  << std::endl;
        envProcess.terminate();
    }

    stdout_output = envProcess.readAllStandardOutput();
    stderr_output = envProcess.readAllStandardError();

    printOutput(stdout_output, stderr_output);

    // ==========================================
    // 4. Process Input/Output
    // ==========================================
    printSection("Process Input/Output");

    atom::utils::QProcess ioProcess;

    // Create a process that reads from stdin
#ifdef _WIN32
    ioProcess.start("more.com", {});
#else
    ioProcess.start("cat", {});
#endif

    std::cout << "Writing data to process stdin..." << std::endl;
    std::string inputData =
        "This is a test input.\nIt has multiple lines.\nEnd of input.";
    std::cout << "Input data:\n" << inputData << std::endl << std::endl;

    // Wait for the process to start
    if (!ioProcess.waitForStarted(std::chrono::seconds(1))) {
        std::cerr << "Process did not start within timeout period."
                  << std::endl;
        return 1;
    }

    // Write to process stdin
    ioProcess.write(inputData);

    // On Windows, we need to close stdin to signal EOF
#ifdef _WIN32
    ioProcess.write("\x1A");  // Ctrl+Z (EOF in Windows)
#endif

    if (!ioProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr << "IO process did not finish within timeout period."
                  << std::endl;
        ioProcess.terminate();
    }

    stdout_output = ioProcess.readAllStandardOutput();
    stderr_output = ioProcess.readAllStandardError();

    printOutput(stdout_output, stderr_output);

    // ==========================================
    // 5. Long-Running Processes and Timeouts
    // ==========================================
    printSection("Long-Running Processes and Timeouts");

    atom::utils::QProcess sleepProcess;

    std::cout << "Starting a process that sleeps for 10 seconds..."
              << std::endl;

    // Start a sleep process
#ifdef _WIN32
    sleepProcess.start(SLEEP_COMMAND, {"10"});
#else
    sleepProcess.start(SLEEP_COMMAND, {"10"});
#endif

    std::cout << "Process started. Waiting for 2 seconds..." << std::endl;

    // Wait for a shorter time than the process will take
    if (sleepProcess.waitForFinished(std::chrono::seconds(2))) {
        std::cout << "Process unexpectedly finished within 2 seconds."
                  << std::endl;
    } else {
        std::cout << "Process is still running after 2 seconds as expected."
                  << std::endl;

        // Check if the process is running
        if (sleepProcess.isRunning()) {
            std::cout << "Process is confirmed to be running via isRunning()."
                      << std::endl;
            std::cout << "Terminating process..." << std::endl;
            sleepProcess.terminate();
            std::cout << "Process terminated." << std::endl;
        } else {
            std::cout << "Unexpected: isRunning() returned false." << std::endl;
        }
    }

    // ==========================================
    // 6. Error Handling
    // ==========================================
    printSection("Error Handling");

    // Case 1: Invalid executable
    std::cout << "Attempting to run a non-existent executable..." << std::endl;
    atom::utils::QProcess invalidProcess;

    try {
        invalidProcess.start("this_executable_does_not_exist", {});
        if (!invalidProcess.waitForStarted(std::chrono::seconds(1))) {
            std::cout << "Process failed to start as expected." << std::endl;
        } else {
            std::cout << "Unexpected: Process started successfully."
                      << std::endl;
            invalidProcess.terminate();
        }
    } catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
    }

    // Case 2: Invalid working directory
    std::cout << "\nAttempting to set an invalid working directory..."
              << std::endl;
    atom::utils::QProcess invalidDirProcess;

    try {
        invalidDirProcess.setWorkingDirectory(
            "/path/that/definitely/does/not/exist");
        std::cout
            << "Unexpected: setWorkingDirectory did not throw an exception."
            << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
    }

    // Case 3: Invalid environment variable format
    std::cout << "\nAttempting to set an invalid environment variable..."
              << std::endl;
    atom::utils::QProcess invalidEnvProcess;

    try {
        invalidEnvProcess.setEnvironment(
            std::vector<std::string>{"invalid_format_without_equals_sign"});
        std::cout << "Unexpected: setEnvironment did not throw an exception."
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception as expected: " << e.what() << std::endl;
    }

    // ==========================================
    // 7. Reading from Files with External Processes
    // ==========================================
    printSection("Reading from Files with External Processes");

    // Create a temporary file
    std::string fileContent =
        "This is line 1\nThis is line 2\nThis is line 3\n";
    std::string tempFilename = createTempFile(fileContent);

    std::cout << "Created temporary file: " << tempFilename << std::endl;
    std::cout << "File content:\n" << fileContent << std::endl;

    // Use an external process to read the file
    atom::utils::QProcess catProcess;

    std::vector<std::string> catArgs = {tempFilename};

    std::cout << "Reading file with '" << CAT_COMMAND << "'..." << std::endl;
    catProcess.start(CAT_COMMAND, catArgs);

    if (!catProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr
            << "File reading process did not finish within timeout period."
            << std::endl;
        catProcess.terminate();
    }

    stdout_output = catProcess.readAllStandardOutput();
    stderr_output = catProcess.readAllStandardError();

    printOutput(stdout_output, stderr_output);

    // Clean up temporary file
    try {
        std::filesystem::remove(tempFilename);
        std::cout << "Temporary file removed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to remove temporary file: " << e.what()
                  << std::endl;
    }

    // ==========================================
    // 8. Asynchronous Process Management
    // ==========================================
    printSection("Asynchronous Process Management");

    atom::utils::QProcess asyncProcess;

    std::cout << "Starting a background process..." << std::endl;

    // Start a process that will produce output over time
#ifdef _WIN32
    asyncProcess.start(
        "cmd.exe",
        {"/c", "for /l %i in (1,1,5) do (echo Line %i & timeout /t 1 > nul)"});
#else
    asyncProcess.start(
        "bash", {"-c", "for i in {1..5}; do echo Line $i; sleep 1; done"});
#endif

    if (!asyncProcess.waitForStarted(std::chrono::seconds(1))) {
        std::cerr << "Async process failed to start." << std::endl;
        return 1;
    }

    std::cout << "Process started successfully. Reading output in real-time..."
              << std::endl;

    // Poll for output while the process is running
    for (int i = 0; i < 10; ++i) {
        if (!asyncProcess.isRunning()) {
            std::cout << "Process has finished." << std::endl;
            break;
        }

        std::string currentOutput = asyncProcess.readAllStandardOutput();
        if (!currentOutput.empty()) {
            std::cout << "Output received: " << currentOutput;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }

    // Wait for process to finish and get any remaining output
    if (asyncProcess.isRunning()) {
        std::cout << "Waiting for process to finish..." << std::endl;
        if (!asyncProcess.waitForFinished(std::chrono::seconds(5))) {
            std::cerr << "Process did not finish within timeout, terminating."
                      << std::endl;
            asyncProcess.terminate();
        }
    }

    stdout_output = asyncProcess.readAllStandardOutput();
    stderr_output = asyncProcess.readAllStandardError();

    if (!stdout_output.empty()) {
        std::cout << "Remaining output: " << stdout_output;
    }

    std::cout << std::endl;

    // ==========================================
    // 9. Process Move Operations
    // ==========================================
    printSection("Process Move Operations");

    std::cout << "Testing move constructor and assignment..." << std::endl;

    // Create a process
    atom::utils::QProcess originalProcess;
    originalProcess.start(ECHO_COMMAND, {"Original Process"});

    // Use move constructor
    std::cout << "Moving process using move constructor..." << std::endl;
    atom::utils::QProcess movedProcess(std::move(originalProcess));

    if (!movedProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr << "Moved process did not finish within timeout period."
                  << std::endl;
        movedProcess.terminate();
    }

    stdout_output = movedProcess.readAllStandardOutput();
    stderr_output = movedProcess.readAllStandardError();

    printOutput(stdout_output, stderr_output);

    // Use move assignment
    std::cout << "Moving process using move assignment..." << std::endl;
    atom::utils::QProcess firstProcess;
    firstProcess.start(ECHO_COMMAND, {"First Process"});

    atom::utils::QProcess secondProcess;
    secondProcess.start(ECHO_COMMAND, {"Second Process"});

    std::cout << "Moving second process to first process..." << std::endl;
    firstProcess = std::move(secondProcess);

    if (!firstProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr << "Process did not finish within timeout period."
                  << std::endl;
        firstProcess.terminate();
    }

    stdout_output = firstProcess.readAllStandardOutput();
    stderr_output = firstProcess.readAllStandardError();

    printOutput(stdout_output, stderr_output);

    // ==========================================
    // 10. Advanced Usage: Process Chaining
    // ==========================================
    printSection("Advanced Usage: Process Chaining");

    std::cout << "Demonstrating process output piping (manually)..."
              << std::endl;

    // First process: Generate some content
    atom::utils::QProcess generateProcess;
#ifdef _WIN32
    generateProcess.start("cmd.exe",
                          {"/c", "echo Line 1 & echo Line 2 & echo Line 3"});
#else
    generateProcess.start(
        "bash", {"-c", "echo 'Line 1' && echo 'Line 2' && echo 'Line 3'"});
#endif

    if (!generateProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr << "Generate process did not finish within timeout period."
                  << std::endl;
        generateProcess.terminate();
        return 1;
    }

    // Get output from first process
    std::string generatedOutput = generateProcess.readAllStandardOutput();
    std::cout << "Output from first process:\n" << generatedOutput << std::endl;

    // Use output as input to second process
    atom::utils::QProcess transformProcess;
#ifdef _WIN32
    transformProcess.start("cmd.exe", {"/c", "findstr /R /C:\"Line\""});
#else
    transformProcess.start("grep", {"Line"});
#endif

    if (!transformProcess.waitForStarted(std::chrono::seconds(1))) {
        std::cerr << "Transform process failed to start." << std::endl;
        return 1;
    }

    // Write the output from the first process to the second process
    transformProcess.write(generatedOutput);

    // Close the input
#ifdef _WIN32
    transformProcess.write("\x1A");  // Ctrl+Z (EOF in Windows)
#endif

    if (!transformProcess.waitForFinished(std::chrono::seconds(5))) {
        std::cerr << "Transform process did not finish within timeout period."
                  << std::endl;
        transformProcess.terminate();
        return 1;
    }

    stdout_output = transformProcess.readAllStandardOutput();
    stderr_output = transformProcess.readAllStandardError();

    std::cout << "Output from second process:" << std::endl;
    printOutput(stdout_output, stderr_output);

    std::cout << "======================================================="
              << std::endl;
    std::cout << "QProcess Example Complete" << std::endl;
    std::cout << "======================================================="
              << std::endl;

    return 0;
}