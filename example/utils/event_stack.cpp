/**
 * @file error_stack_example.cpp
 * @brief Comprehensive examples demonstrating error stack utilities
 *
 * This example demonstrates all functions available in
 * atom::utils::error_stack.hpp:
 * - Basic error insertion and retrieval
 * - Error filtering by module and severity
 * - Error stack compression and formatting
 * - Real-world error handling scenarios
 * - Performance considerations for error tracking
 * - Integration with logging systems
 */

#include "atom/utils/debug/error_stack.hpp"

#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace atom::error;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Simulate a function that might encounter errors
void simulateFileOperation(std::shared_ptr<ErrorStack> errorStack,
                           const std::string& filename,
                           bool shouldFail = false) {
    if (shouldFail) {
        errorStack->insertError("Failed to open file: " + filename,
                                "FileSystem", "simulateFileOperation", __LINE__,
                                __FILE__);
    } else {
        std::cout << "Successfully processed file: " << filename << std::endl;
    }
}

// Simulate a network operation
void simulateNetworkOperation(std::shared_ptr<ErrorStack> errorStack,
                              const std::string& url, int timeout = 5000) {
    // Simulate random network failures
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 10);

    if (dis(gen) <= 3) {  // 30% chance of failure
        errorStack->insertError(
            "Network timeout connecting to: " + url +
                " (timeout: " + std::to_string(timeout) + "ms)",
            "Network", "simulateNetworkOperation", __LINE__, __FILE__);
    } else {
        std::cout << "Successfully connected to: " << url << std::endl;
    }
}

// Simulate database operations
void simulateDatabaseOperation(std::shared_ptr<ErrorStack> errorStack,
                               const std::string& query, bool isWrite = false) {
    if (isWrite && query.find("DROP") != std::string::npos) {
        errorStack->insertError("Dangerous operation detected: " + query,
                                "Database", "simulateDatabaseOperation",
                                __LINE__, __FILE__);
    } else if (query.empty()) {
        errorStack->insertError("Empty query provided", "Database",
                                "simulateDatabaseOperation", __LINE__,
                                __FILE__);
    } else {
        std::cout << "Successfully executed query: " << query.substr(0, 50)
                  << (query.length() > 50 ? "..." : "") << std::endl;
    }
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  Error Stack Utilities Demo" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ============================
    // Example 1: Basic Error Stack Operations
    // ============================
    printSection("1. Basic Error Stack Operations");

    printSubsection("Creating Error Stacks");

    // Create different types of error stacks
    auto errorStackShared = ErrorStack::createShared();
    auto errorStackUnique = ErrorStack::createUnique();

    std::cout << "Created shared and unique error stacks" << std::endl;

    printSubsection("Basic Error Insertion");

    // Insert various types of errors
    errorStackShared->insertError("Configuration file not found", "Config",
                                  "loadConfig", 42, "config.cpp");
    errorStackShared->insertError("Invalid parameter value", "Validation",
                                  "validateInput", 15, "validator.cpp");
    errorStackShared->insertError("Memory allocation failed", "Memory",
                                  "allocateBuffer", 128, "memory.cpp");
    errorStackShared->insertError("Permission denied", "Security",
                                  "checkPermissions", 67, "security.cpp");

    std::cout << "Inserted 4 different types of errors" << std::endl;

    printSubsection("Error Stack Display");

    std::cout << "Complete error stack:" << std::endl;
    errorStackShared->printFilteredErrorStack();

    // ============================
    // Example 2: Error Filtering
    // ============================
    printSection("2. Error Filtering");

    printSubsection("Module-based Filtering");

    // Filter out specific modules
    std::vector<std::string> modulesToFilter = {"Config", "Memory"};
    errorStackShared->setFilteredModules(modulesToFilter);
    std::cout << "Filtered error stack (excluding Config and Memory modules):"
              << std::endl;
    errorStackShared->printFilteredErrorStack();

    // Clear filters
    errorStackShared->clearFilteredModules();
    std::cout << "\nFilters cleared" << std::endl;

    printSubsection("Module-specific Error Retrieval");

    auto securityErrors =
        errorStackShared->getFilteredErrorsByModule("Security");
    std::cout << "Security module errors:" << std::endl;
    for (const auto& error : securityErrors) {
        std::cout << "  " << error << std::endl;
    }

    auto validationErrors =
        errorStackShared->getFilteredErrorsByModule("Validation");
    std::cout << "Validation module errors:" << std::endl;
    for (const auto& error : validationErrors) {
        std::cout << "  " << error << std::endl;
    }

    // ============================
    // Example 3: Error Compression
    // ============================
    printSection("3. Error Compression");

    printSubsection("Compressed Error Output");

    std::string compressedErrors = errorStackShared->getCompressedErrors();
    std::cout << "Compressed errors:" << std::endl;
    std::cout << compressedErrors << std::endl;

    std::cout << "Original error count: " << 4 << std::endl;
    std::cout << "Compressed output length: " << compressedErrors.length()
              << " characters" << std::endl;

    // ============================
    // Example 4: Real-world Simulation
    // ============================
    printSection("4. Real-world Error Handling Simulation");

    printSubsection("File Processing Simulation");

    auto fileErrorStack = ErrorStack::createShared();

    std::vector<std::string> files = {
        "config.json", "data.csv", "missing_file.txt", "log.txt", "backup.zip"};

    for (const auto& file : files) {
        bool shouldFail = (file == "missing_file.txt");
        simulateFileOperation(fileErrorStack, file, shouldFail);
    }

    std::cout << "\nFile processing errors:" << std::endl;
    fileErrorStack->printFilteredErrorStack();

    printSubsection("Network Operations Simulation");

    auto networkErrorStack = ErrorStack::createShared();

    std::vector<std::string> urls = {
        "https://api.example.com/users", "https://api.example.com/data",
        "https://slow-api.example.com/heavy", "https://api.example.com/status"};

    for (const auto& url : urls) {
        simulateNetworkOperation(networkErrorStack, url);
    }

    std::cout << "\nNetwork operation errors:" << std::endl;
    networkErrorStack->printFilteredErrorStack();

    printSubsection("Database Operations Simulation");

    auto dbErrorStack = ErrorStack::createShared();

    std::vector<std::pair<std::string, bool>> queries = {
        {"SELECT * FROM users WHERE active = 1", false},
        {"INSERT INTO logs (message) VALUES ('test')", true},
        {"", false},                 // Empty query
        {"DROP TABLE users", true},  // Dangerous operation
        {"UPDATE users SET last_login = NOW()", true}};

    for (const auto& [query, isWrite] : queries) {
        simulateDatabaseOperation(dbErrorStack, query, isWrite);
    }

    std::cout << "\nDatabase operation errors:" << std::endl;
    dbErrorStack->printFilteredErrorStack();

    std::cout << "\nAll error stack examples completed successfully!"
              << std::endl;

    return 0;
}
