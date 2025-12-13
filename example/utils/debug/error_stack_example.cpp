/**
 * @file error_stack_example.cpp
 * @brief Examples for atom::utils ErrorStack
 */

#include "atom/utils/debug/error_stack.hpp"
#include <iostream>
#include <string>

using namespace atom::error;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateBasicErrorStack() {
    printSection("1. Basic ErrorStack Usage");

    auto errorStack = ErrorStack::createShared();

    errorStack->insertError("File not found", "FileModule", "openFile", 42, "file.cpp");
    errorStack->insertError("Permission denied", "FileModule", "writeFile", 78, "file.cpp");
    errorStack->insertError("Connection timeout", "NetworkModule", "connect", 123, "network.cpp");

    std::cout << "Errors in stack: " << errorStack->size() << std::endl;
    std::cout << "\n--- Error Stack ---" << std::endl;
    errorStack->printFilteredErrorStack();
}

void demonstrateErrorInfoBuilder() {
    printSection("2. ErrorInfoBuilder");

    auto errorStack = ErrorStack::createShared();

    auto error1 = ErrorInfoBuilder()
        .message("Database connection failed")
        .module("DatabaseModule")
        .function("connect")
        .file("database.cpp", 156)
        .level(ErrorLevel::Error)
        .category(ErrorCategory::Database)
        .code(1001)
        .build();

    auto error2 = ErrorInfoBuilder()
        .message("Cache miss - falling back to database")
        .module("CacheModule")
        .function("get")
        .file("cache.cpp", 89)
        .level(ErrorLevel::Warning)
        .category(ErrorCategory::Memory)
        .build();

    errorStack->insertError(error1);
    errorStack->insertError(error2);

    std::cout << "Built errors using ErrorInfoBuilder" << std::endl;
    errorStack->printFilteredErrorStack();
}

void demonstrateErrorLevels() {
    printSection("3. Error Levels");

    auto errorStack = ErrorStack::createShared();

    errorStack->insertError(ErrorInfoBuilder()
        .message("Debug information")
        .module("App").function("init").file("app.cpp", 10)
        .level(ErrorLevel::Debug).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("Application started")
        .module("App").function("main").file("app.cpp", 20)
        .level(ErrorLevel::Info).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("Config file missing, using defaults")
        .module("Config").function("load").file("config.cpp", 30)
        .level(ErrorLevel::Warning).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("Failed to save user data")
        .module("User").function("save").file("user.cpp", 40)
        .level(ErrorLevel::Error).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("System out of memory")
        .module("System").function("allocate").file("system.cpp", 50)
        .level(ErrorLevel::Critical).build());

    std::cout << "All error levels:" << std::endl;
    errorStack->printFilteredErrorStack();
}

void demonstrateErrorCategories() {
    printSection("4. Error Categories");

    auto errorStack = ErrorStack::createShared();

    std::vector<std::pair<ErrorCategory, std::string>> categories = {
        {ErrorCategory::General, "General error occurred"},
        {ErrorCategory::System, "System call failed"},
        {ErrorCategory::Network, "Network unreachable"},
        {ErrorCategory::Database, "Query execution failed"},
        {ErrorCategory::Security, "Authentication failed"},
        {ErrorCategory::IO, "Disk write error"},
        {ErrorCategory::Memory, "Memory allocation failed"},
        {ErrorCategory::Configuration, "Invalid configuration"},
        {ErrorCategory::Validation, "Input validation failed"}
    };

    for (const auto& [cat, msg] : categories) {
        errorStack->insertError(ErrorInfoBuilder()
            .message(msg)
            .module("TestModule").function("test").file("test.cpp", 1)
            .category(cat).build());
    }

    std::cout << "Errors by category:" << std::endl;
    errorStack->printFilteredErrorStack();
}

void demonstrateErrorFiltering() {
    printSection("5. Error Filtering");

    auto errorStack = ErrorStack::createShared();

    for (int i = 0; i < 5; ++i) {
        errorStack->insertError("Error " + std::to_string(i), "ModuleA", "funcA", i * 10, "a.cpp");
        errorStack->insertError("Error " + std::to_string(i), "ModuleB", "funcB", i * 10, "b.cpp");
    }

    std::cout << "Total errors: " << errorStack->size() << std::endl;

    std::cout << "\n--- Filter by module 'ModuleA' ---" << std::endl;
    auto filteredA = errorStack->filterByModule("ModuleA");
    for (const auto& err : filteredA) {
        std::cout << "  " << err.moduleName << ": " << err.errorMessage << std::endl;
    }

    std::cout << "\n--- Get latest error ---" << std::endl;
    if (auto latest = errorStack->getLatestError()) {
        std::cout << "  " << latest->errorMessage << " in " << latest->moduleName << std::endl;
    }
}

void demonstrateRealWorldScenario() {
    printSection("6. Real-World Scenario");

    auto errorStack = ErrorStack::createShared();

    std::cout << "Simulating application startup..." << std::endl;

    errorStack->insertError(ErrorInfoBuilder()
        .message("Loading configuration...")
        .module("Config").function("load").file("config.cpp", 25)
        .level(ErrorLevel::Info).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("Database connection pool initialized")
        .module("Database").function("initPool").file("db.cpp", 100)
        .level(ErrorLevel::Info).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("Cache server not responding, retrying...")
        .module("Cache").function("connect").file("cache.cpp", 50)
        .level(ErrorLevel::Warning)
        .category(ErrorCategory::Network).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("Cache connection established after retry")
        .module("Cache").function("connect").file("cache.cpp", 55)
        .level(ErrorLevel::Info).build());

    errorStack->insertError(ErrorInfoBuilder()
        .message("Application ready to serve requests")
        .module("App").function("start").file("app.cpp", 200)
        .level(ErrorLevel::Info).build());

    std::cout << "\n--- Startup Log ---" << std::endl;
    errorStack->printFilteredErrorStack();

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Total events: " << errorStack->size() << std::endl;
    std::cout << "Warnings: " << errorStack->filterByLevel(ErrorLevel::Warning).size() << std::endl;
    std::cout << "Errors: " << errorStack->filterByLevel(ErrorLevel::Error).size() << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ErrorStack Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicErrorStack();
        demonstrateErrorInfoBuilder();
        demonstrateErrorLevels();
        demonstrateErrorCategories();
        demonstrateErrorFiltering();
        demonstrateRealWorldScenario();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All ErrorStack examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
