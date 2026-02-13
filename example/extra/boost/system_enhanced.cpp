#include "atom/extra/boost/system.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>

using namespace atom::extra::boost;

void demonstrateBasicErrorHandling() {
    std::cout << "=== Basic Error Handling ===" << std::endl;

    // Create errors with different methods
    Error no_error;
    Error file_error(boost::system::errc::no_such_file_or_directory,
                     boost::system::generic_category());
    Error permission_error(boost::system::errc::permission_denied,
                          boost::system::generic_category());

    std::cout << "No error: " << (no_error ? "has error" : "no error") << std::endl;
    std::cout << "File error: " << file_error.message() << std::endl;
    std::cout << "Permission error: " << permission_error.message() << std::endl;

    // Test error comparison
    Error another_file_error(boost::system::errc::no_such_file_or_directory,
                             boost::system::generic_category());
    std::cout << "File errors equal: " << (file_error == another_file_error) << std::endl;
}

void demonstrateErrorContext() {
    std::cout << "\n=== Error Context ===" << std::endl;

    ErrorContext context;
    context.function_name = "demonstrateErrorContext";
    context.file_name = __FILE__;
    context.line_number = __LINE__;
    context.timestamp = std::chrono::steady_clock::now();
    context.metadata["operation"] = "file_read";
    context.metadata["filename"] = "test.txt";
    context.metadata["user_id"] = "12345";

    boost::system::error_code ec(boost::system::errc::no_such_file_or_directory,
                                boost::system::generic_category());
    Error contextual_error(ec, context);

    std::cout << "Basic message: " << contextual_error.message() << std::endl;
    std::cout << "Detailed message: " << contextual_error.detailedMessage() << std::endl;

    // Test macro usage
    auto macro_error = MAKE_ERROR_WITH_CONTEXT(ec);
    std::cout << "Macro error: " << macro_error.detailedMessage() << std::endl;
}

void demonstrateResultType() {
    std::cout << "\n=== Result Type ===" << std::endl;

    // Function that might fail
    auto divide = [](double a, double b) -> Result<double> {
        if (b == 0.0) {
            return Result<double>(Error(boost::system::errc::invalid_argument,
                                       boost::system::generic_category()));
        }
        return Result<double>(a / b);
    };

    auto result1 = divide(10.0, 2.0);
    auto result2 = divide(10.0, 0.0);

    std::cout << "10 / 2 = ";
    if (result1) {
        std::cout << result1.value() << std::endl;
    } else {
        std::cout << "Error: " << result1.error().message() << std::endl;
    }

    std::cout << "10 / 0 = ";
    if (result2) {
        std::cout << result2.value() << std::endl;
    } else {
        std::cout << "Error: " << result2.error().message() << std::endl;
    }

    // Test valueOr
    std::cout << "10 / 0 with default: " << result2.valueOr(-1.0) << std::endl;

    // Test map operation
    auto squared_result = result1.map([](double x) { return x * x; });
    if (squared_result) {
        std::cout << "(10 / 2)^2 = " << squared_result.value() << std::endl;
    }
}

void demonstrateVoidResult() {
    std::cout << "\n=== Void Result ===" << std::endl;

    auto write_file = [](const std::string& filename, const std::string& content) -> Result<void> {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return Result<void>(Error(boost::system::errc::permission_denied,
                                     boost::system::generic_category()));
        }
        file << content;
        return Result<void>();
    };

    auto result1 = write_file("/tmp/test.txt", "Hello, World!");
    auto result2 = write_file("/root/test.txt", "This will fail");

    std::cout << "Write to /tmp/test.txt: " << (result1 ? "Success" : "Failed") << std::endl;
    std::cout << "Write to /root/test.txt: " << (result2 ? "Success" : "Failed") << std::endl;

    if (!result2) {
        std::cout << "Error: " << result2.error().message() << std::endl;
    }
}

void demonstrateMakeResult() {
    std::cout << "\n=== Make Result ===" << std::endl;

    // Test makeResult with successful function
    auto success_result = makeResult([]() -> int {
        return 42;
    });

    std::cout << "Success result: ";
    if (success_result) {
        std::cout << success_result.value() << std::endl;
    } else {
        std::cout << "Error: " << success_result.error().message() << std::endl;
    }

    // Test makeResult with throwing function
    auto error_result = makeResult([]() -> int {
        throw Exception(Error(boost::system::errc::operation_not_permitted,
                             boost::system::generic_category()));
    });

    std::cout << "Error result: ";
    if (error_result) {
        std::cout << error_result.value() << std::endl;
    } else {
        std::cout << "Error: " << error_result.error().message() << std::endl;
    }

    // Test makeResult with void function
    auto void_result = makeResult([]() {
        std::cout << "Void function executed successfully" << std::endl;
    });

    std::cout << "Void result: " << (void_result ? "Success" : "Failed") << std::endl;
}

void demonstrateStructuredLogging() {
    std::cout << "\n=== Structured Logging ===" << std::endl;

    // Initialize logger
    StructuredLogger::initialize("/tmp/system_test.log", LogLevel::DEBUG);

    // Log messages with different levels
    LOG_INFO("Application started");
    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message");

    // Log with custom context
    ErrorContext context;
    context.function_name = "demonstrateStructuredLogging";
    context.metadata["component"] = "system_test";
    context.metadata["version"] = "1.0.0";

    StructuredLogger::log(LogLevel::INFO, "Custom context message", context);

    // Give logger time to write
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "Logs written to /tmp/system_test.log" << std::endl;

    // Read and display log content
    std::ifstream log_file("/tmp/system_test.log");
    if (log_file.is_open()) {
        std::cout << "Log content:" << std::endl;
        std::string line;
        while (std::getline(log_file, line)) {
            std::cout << "  " << line << std::endl;
        }
    }
}

void demonstrateSystemMonitoring() {
    std::cout << "\n=== System Monitoring ===" << std::endl;

    // Start monitoring
    SystemMonitor::startMonitoring(std::chrono::seconds(1));

    std::cout << "Monitoring system for 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Get current system info
    auto current_info = SystemMonitor::getCurrentSystemInfo();
    std::cout << "Current System Info:" << std::endl;
    std::cout << "  Hostname: " << current_info.hostname << std::endl;
    std::cout << "  CPU Usage: " << current_info.cpu_usage_percent << "%" << std::endl;
    std::cout << "  Memory Used: " << (current_info.memory_used_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Memory Total: " << (current_info.memory_total_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Disk Used: " << (current_info.disk_used_bytes / 1024 / 1024 / 1024) << " GB" << std::endl;
    std::cout << "  Disk Total: " << (current_info.disk_total_bytes / 1024 / 1024 / 1024) << " GB" << std::endl;

    // Get monitoring history
    auto history = SystemMonitor::getHistory();
    std::cout << "Monitoring history: " << history.size() << " entries" << std::endl;

    if (!history.empty()) {
        std::cout << "Memory usage over time:" << std::endl;
        for (const auto& info : history) {
            double memory_percent = (static_cast<double>(info.memory_used_bytes) / info.memory_total_bytes) * 100.0;
            std::cout << "  " << std::fixed << std::setprecision(1) << memory_percent << "%" << std::endl;
        }
    }

    // Stop monitoring
    SystemMonitor::stopMonitoring();
}

int main() {
    try {
        demonstrateBasicErrorHandling();
        demonstrateErrorContext();
        demonstrateResultType();
        demonstrateVoidResult();
        demonstrateMakeResult();
        demonstrateStructuredLogging();
        demonstrateSystemMonitoring();

        // Cleanup
        StructuredLogger::shutdown();

        std::cout << "\n=== All system tests completed successfully! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
