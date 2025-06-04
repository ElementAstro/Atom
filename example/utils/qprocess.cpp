/**
 * @file qprocess.cpp
 * @brief QProcess usage example demonstrating external process management
 *
 * This example demonstrates comprehensive usage of the QProcess class for
 * managing external processes including process creation, I/O handling,
 * environment management, and cross-platform compatibility.
 *
 * @author Example User
 * @date 2024
 * @copyright Copyright (C) 2024 Example User
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include "atom/utils/qprocess.hpp"

namespace {
constexpr auto TIMEOUT_SECONDS = std::chrono::seconds(5);
constexpr auto SHORT_TIMEOUT = std::chrono::seconds(1);

#ifdef _WIN32
constexpr const char* ECHO_COMMAND = "cmd.exe";
const std::vector<std::string> ECHO_ARGS = {"/c", "echo"};
constexpr const char* LIST_DIR_COMMAND = "cmd.exe";
const std::vector<std::string> LIST_DIR_ARGS = {"/c", "dir"};
constexpr const char* SLEEP_COMMAND = "timeout.exe";
constexpr const char* CAT_COMMAND = "type";
#else
constexpr const char* ECHO_COMMAND = "/bin/echo";
const std::vector<std::string> ECHO_ARGS = {};
constexpr const char* LIST_DIR_COMMAND = "ls";
const std::vector<std::string> LIST_DIR_ARGS = {"-la"};
constexpr const char* SLEEP_COMMAND = "sleep";
constexpr const char* CAT_COMMAND = "cat";
#endif

/**
 * @brief Print formatted section header
 * @param title Section title to display
 */
void printSection(const std::string& title) {
    spdlog::info("========== {} ==========", title);
}

/**
 * @brief Print process output in formatted manner
 * @param stdout_output Standard output content
 * @param stderr_output Standard error content
 */
void printOutput(const std::string& stdout_output,
                 const std::string& stderr_output) {
    spdlog::info("Standard Output: {}",
                 stdout_output.empty() ? "(empty)" : stdout_output);
    spdlog::info("Standard Error: {}",
                 stderr_output.empty() ? "(empty)" : stderr_output);
}

/**
 * @brief Create temporary file with specified content
 * @param content Content to write to file
 * @return Generated filename
 */
std::string createTempFile(const std::string& content) {
    const std::string filename =
        "qprocess_temp_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()) +
        ".txt";

    std::ofstream file(filename);
    file << content;
    return filename;
}

/**
 * @brief Format environment variable string
 * @param name Variable name
 * @param value Variable value
 * @return Formatted environment string
 */
std::string formatEnvVar(const std::string& name, const std::string& value) {
    return name + "=" + value;
}

/**
 * @brief Execute basic echo command demonstration
 */
void demonstrateBasicExecution() {
    printSection("Basic Process Execution");

    atom::utils::QProcess echoProcess;
    spdlog::info("Executing echo command with 'Hello, World!'");

    auto args = ECHO_ARGS;
    args.emplace_back("Hello, World!");

    echoProcess.start(ECHO_COMMAND, args);

    if (!echoProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("Process did not finish within timeout period");
        echoProcess.terminate();
        return;
    }

    printOutput(echoProcess.readAllStandardOutput(),
                echoProcess.readAllStandardError());
}

/**
 * @brief Demonstrate working directory configuration
 */
void demonstrateWorkingDirectory() {
    printSection("Setting Working Directory");

    atom::utils::QProcess dirProcess;
    const auto currentDir = std::filesystem::current_path().string();
    const auto parentDir =
        std::filesystem::current_path().parent_path().string();

    spdlog::info("Current directory: {}", currentDir);
    spdlog::info("Setting working directory to: {}", parentDir);

    try {
        dirProcess.setWorkingDirectory(parentDir);
        dirProcess.start(LIST_DIR_COMMAND, LIST_DIR_ARGS);

        if (!dirProcess.waitForFinished(TIMEOUT_SECONDS)) {
            spdlog::error("Directory listing process timeout");
            dirProcess.terminate();
            return;
        }

        printOutput(dirProcess.readAllStandardOutput(),
                    dirProcess.readAllStandardError());
    } catch (const std::exception& e) {
        spdlog::error("Error setting working directory: {}", e.what());
    }
}

/**
 * @brief Demonstrate environment variable management
 */
void demonstrateEnvironmentVariables() {
    printSection("Environment Variables");

    atom::utils::QProcess envProcess;
    const std::vector<std::string> environment = {
        formatEnvVar("QPROCESS_TEST_VAR1", "Hello"),
        formatEnvVar("QPROCESS_TEST_VAR2", "World"),
        formatEnvVar("QPROCESS_TEST_VAR3", "From QProcess")};

    spdlog::info("Setting environment variables:");
    for (const auto& env : environment) {
        spdlog::info("  {}", env);
    }

    envProcess.setEnvironment(environment);

#ifdef _WIN32
    envProcess.start("cmd.exe", {"/c", "set", "QPROCESS_TEST"});
#else
    envProcess.start("/bin/sh", {"-c", "env | grep QPROCESS_TEST"});
#endif

    if (!envProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("Environment process timeout");
        envProcess.terminate();
        return;
    }

    printOutput(envProcess.readAllStandardOutput(),
                envProcess.readAllStandardError());
}

/**
 * @brief Demonstrate process input/output handling
 */
void demonstrateInputOutput() {
    printSection("Process Input/Output");

    atom::utils::QProcess ioProcess;

#ifdef _WIN32
    ioProcess.start("more.com", {});
#else
    ioProcess.start("cat", {});
#endif

    if (!ioProcess.waitForStarted(SHORT_TIMEOUT)) {
        spdlog::error("Process failed to start within timeout");
        return;
    }

    const std::string inputData =
        "This is a test input.\nIt has multiple lines.\nEnd of input.";
    spdlog::info("Writing data to process stdin: {}", inputData);

    ioProcess.write(inputData);

#ifdef _WIN32
    ioProcess.write("\x1A");
#endif

    if (!ioProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("IO process timeout");
        ioProcess.terminate();
        return;
    }

    printOutput(ioProcess.readAllStandardOutput(),
                ioProcess.readAllStandardError());
}

/**
 * @brief Demonstrate long-running process management
 */
void demonstrateLongRunningProcess() {
    printSection("Long-Running Processes and Timeouts");

    atom::utils::QProcess sleepProcess;
    spdlog::info("Starting process that sleeps for 10 seconds");

#ifdef _WIN32
    sleepProcess.start(SLEEP_COMMAND, {"10"});
#else
    sleepProcess.start(SLEEP_COMMAND, {"10"});
#endif

    spdlog::info("Process started. Waiting for 2 seconds");

    if (sleepProcess.waitForFinished(std::chrono::seconds(2))) {
        spdlog::info("Process unexpectedly finished within 2 seconds");
    } else {
        spdlog::info("Process still running after 2 seconds as expected");

        if (sleepProcess.isRunning()) {
            spdlog::info("Process confirmed running. Terminating...");
            sleepProcess.terminate();
            spdlog::info("Process terminated");
        } else {
            spdlog::warn("Unexpected: isRunning() returned false");
        }
    }
}

/**
 * @brief Demonstrate error handling scenarios
 */
void demonstrateErrorHandling() {
    printSection("Error Handling");

    spdlog::info("Testing non-existent executable");
    atom::utils::QProcess invalidProcess;

    try {
        invalidProcess.start("this_executable_does_not_exist", {});
        if (!invalidProcess.waitForStarted(SHORT_TIMEOUT)) {
            spdlog::info("Process failed to start as expected");
        } else {
            spdlog::warn("Unexpected: Process started successfully");
            invalidProcess.terminate();
        }
    } catch (const std::exception& e) {
        spdlog::info("Caught expected exception: {}", e.what());
    }

    spdlog::info("Testing invalid working directory");
    atom::utils::QProcess invalidDirProcess;

    try {
        invalidDirProcess.setWorkingDirectory(
            "/path/that/definitely/does/not/exist");
        spdlog::warn("Unexpected: setWorkingDirectory did not throw exception");
    } catch (const std::exception& e) {
        spdlog::info("Caught expected exception: {}", e.what());
    }

    spdlog::info("Testing invalid environment variable format");
    atom::utils::QProcess invalidEnvProcess;

    try {
        invalidEnvProcess.setEnvironment(
            std::vector<std::string>({"invalid_format_without_equals_sign"}));
        spdlog::warn("Unexpected: setEnvironment did not throw exception");
    } catch (const std::exception& e) {
        spdlog::info("Caught expected exception: {}", e.what());
    }
}

/**
 * @brief Demonstrate file reading with external processes
 */
void demonstrateFileReading() {
    printSection("Reading from Files with External Processes");

    const std::string fileContent =
        "This is line 1\nThis is line 2\nThis is line 3\n";
    const std::string tempFilename = createTempFile(fileContent);

    spdlog::info("Created temporary file: {}", tempFilename);
    spdlog::info("File content: {}", fileContent);

    atom::utils::QProcess catProcess;
    spdlog::info("Reading file with '{}'", CAT_COMMAND);
    catProcess.start(CAT_COMMAND, {tempFilename});

    if (!catProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("File reading process timeout");
        catProcess.terminate();
    } else {
        printOutput(catProcess.readAllStandardOutput(),
                    catProcess.readAllStandardError());
    }

    try {
        std::filesystem::remove(tempFilename);
        spdlog::info("Temporary file removed");
    } catch (const std::exception& e) {
        spdlog::error("Failed to remove temporary file: {}", e.what());
    }
}

/**
 * @brief Demonstrate asynchronous process management
 */
void demonstrateAsynchronousProcess() {
    printSection("Asynchronous Process Management");

    atom::utils::QProcess asyncProcess;
    spdlog::info("Starting background process");

#ifdef _WIN32
    asyncProcess.start(
        "cmd.exe",
        {"/c", "for /l %i in (1,1,5) do (echo Line %i & timeout /t 1 > nul)"});
#else
    asyncProcess.start(
        "bash", {"-c", "for i in {1..5}; do echo Line $i; sleep 1; done"});
#endif

    if (!asyncProcess.waitForStarted(SHORT_TIMEOUT)) {
        spdlog::error("Async process failed to start");
        return;
    }

    spdlog::info("Process started successfully. Reading output in real-time");

    for (int i = 0; i < 10; ++i) {
        if (!asyncProcess.isRunning()) {
            spdlog::info("Process has finished");
            break;
        }

        const std::string currentOutput = asyncProcess.readAllStandardOutput();
        if (!currentOutput.empty()) {
            spdlog::info("Output received: {}", currentOutput);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }

    if (asyncProcess.isRunning()) {
        spdlog::info("Waiting for process to finish");
        if (!asyncProcess.waitForFinished(TIMEOUT_SECONDS)) {
            spdlog::error("Process timeout, terminating");
            asyncProcess.terminate();
        }
    }

    const std::string remainingOutput = asyncProcess.readAllStandardOutput();
    if (!remainingOutput.empty()) {
        spdlog::info("Remaining output: {}", remainingOutput);
    }
}

/**
 * @brief Demonstrate move operations
 */
void demonstrateMoveOperations() {
    printSection("Process Move Operations");

    spdlog::info("Testing move constructor and assignment");

    atom::utils::QProcess originalProcess;
    originalProcess.start(ECHO_COMMAND, {"Original Process"});

    spdlog::info("Moving process using move constructor");
    atom::utils::QProcess movedProcess(std::move(originalProcess));

    if (!movedProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("Moved process timeout");
        movedProcess.terminate();
    } else {
        printOutput(movedProcess.readAllStandardOutput(),
                    movedProcess.readAllStandardError());
    }

    spdlog::info("Testing move assignment");
    atom::utils::QProcess firstProcess;
    firstProcess.start(ECHO_COMMAND, {"First Process"});

    atom::utils::QProcess secondProcess;
    secondProcess.start(ECHO_COMMAND, {"Second Process"});

    spdlog::info("Moving second process to first process");
    firstProcess = std::move(secondProcess);

    if (!firstProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("Process timeout");
        firstProcess.terminate();
    } else {
        printOutput(firstProcess.readAllStandardOutput(),
                    firstProcess.readAllStandardError());
    }
}

/**
 * @brief Demonstrate process chaining
 */
void demonstrateProcessChaining() {
    printSection("Advanced Usage: Process Chaining");

    spdlog::info("Demonstrating process output piping");

    atom::utils::QProcess generateProcess;
#ifdef _WIN32
    generateProcess.start("cmd.exe",
                          {"/c", "echo Line 1 & echo Line 2 & echo Line 3"});
#else
    generateProcess.start(
        "bash", {"-c", "echo 'Line 1' && echo 'Line 2' && echo 'Line 3'"});
#endif

    if (!generateProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("Generate process timeout");
        generateProcess.terminate();
        return;
    }

    const std::string generatedOutput = generateProcess.readAllStandardOutput();
    spdlog::info("Output from first process: {}", generatedOutput);

    atom::utils::QProcess transformProcess;
#ifdef _WIN32
    transformProcess.start("cmd.exe", {"/c", "findstr /R /C:\"Line\""});
#else
    transformProcess.start("grep", {"Line"});
#endif

    if (!transformProcess.waitForStarted(SHORT_TIMEOUT)) {
        spdlog::error("Transform process failed to start");
        return;
    }

    transformProcess.write(generatedOutput);

#ifdef _WIN32
    transformProcess.write("\x1A");
#endif

    if (!transformProcess.waitForFinished(TIMEOUT_SECONDS)) {
        spdlog::error("Transform process timeout");
        transformProcess.terminate();
        return;
    }

    spdlog::info("Output from second process:");
    printOutput(transformProcess.readAllStandardOutput(),
                transformProcess.readAllStandardError());
}
}  // namespace

/**
 * @brief Main function demonstrating QProcess comprehensive usage
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit status
 */
int main(int argc, char* argv[]) {
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);

    spdlog::info("=======================================================");
    spdlog::info("QProcess Comprehensive Usage Example");
    spdlog::info("=======================================================");

    demonstrateBasicExecution();
    demonstrateWorkingDirectory();
    demonstrateEnvironmentVariables();
    demonstrateInputOutput();
    demonstrateLongRunningProcess();
    demonstrateErrorHandling();
    demonstrateFileReading();
    demonstrateAsynchronousProcess();
    demonstrateMoveOperations();
    demonstrateProcessChaining();

    spdlog::info("=======================================================");
    spdlog::info("QProcess Example Complete");
    spdlog::info("=======================================================");

    return 0;
}