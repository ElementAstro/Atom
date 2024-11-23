#include "atom/utils/qprocess.hpp"

#include <iostream>

using namespace atom::utils;

int main() {
    // Create a QProcess instance
    QProcess process;

    // Set the working directory for the process
    process.setWorkingDirectory("/path/to/working/directory");

    // Set the environment variables for the process
    std::vector<std::string> env = {"VAR1=value1", "VAR2=value2"};
    process.setEnvironment(env);

    // Start the external process with the given program and arguments
    std::string program = "/path/to/executable";
    std::vector<std::string> args = {"arg1", "arg2"};
    process.start(program, args);

    // Wait for the process to start
    bool started = process.waitForStarted(5000);  // Wait for 5 seconds
    std::cout << "Process started: " << std::boolalpha << started << std::endl;

    // Check if the process is currently running
    bool running = process.isRunning();
    std::cout << "Process is running: " << std::boolalpha << running << std::endl;

    // Write data to the process's standard input
    process.write("input data\n");

    // Read all available data from the process's standard output
    std::string output = process.readAllStandardOutput();
    std::cout << "Standard Output: " << output << std::endl;

    // Read all available data from the process's standard error
    std::string errorOutput = process.readAllStandardError();
    std::cout << "Standard Error: " << errorOutput << std::endl;

    // Wait for the process to finish
    bool finished = process.waitForFinished(10000);  // Wait for 10 seconds
    std::cout << "Process finished: " << std::boolalpha << finished << std::endl;

    // Terminate the process if it is still running
    if (process.isRunning()) {
        process.terminate();
        std::cout << "Process terminated." << std::endl;
    }

    return 0;
}