/*
 * error_handling_demo.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive demonstration of the Atom Error Handling System

**************************************************/

#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>

#include "atom/error/error_code.hpp"
#include "atom/error/error_context.hpp"
#include "atom/error/error_handler.hpp"
#include "atom/error/error_recovery.hpp"
#include "atom/error/error_formatter.hpp"
#include "atom/error/exception.hpp"

using namespace atom::error;

// Simulated file operations that might fail
class FileProcessor {
private:
    std::mt19937 rng_;
    std::uniform_int_distribution<int> dist_;
    
public:
    FileProcessor() : rng_(std::random_device{}()), dist_(1, 10) {}
    
    std::string readFile(const std::string& filename) {
        // Simulate random failures
        int chance = dist_(rng_);
        
        if (chance <= 3) {
            throw std::runtime_error("File not found: " + filename);
        } else if (chance <= 5) {
            throw std::runtime_error("Permission denied: " + filename);
        } else if (chance <= 6) {
            throw std::runtime_error("Disk full");
        }
        
        return "File content from " + filename;
    }
    
    void writeFile(const std::string& filename, const std::string& content) {
        int chance = dist_(rng_);
        
        if (chance <= 2) {
            throw std::runtime_error("Cannot write to " + filename);
        } else if (chance <= 4) {
            throw std::runtime_error("Disk full");
        }
        
        // Success - no exception thrown
    }
};

// Simulated network operations
class NetworkClient {
private:
    std::mt19937 rng_;
    std::uniform_int_distribution<int> dist_;
    
public:
    NetworkClient() : rng_(std::random_device{}()), dist_(1, 10) {}
    
    std::string fetchData(const std::string& url) {
        int chance = dist_(rng_);
        
        if (chance <= 2) {
            throw std::runtime_error("Connection timeout");
        } else if (chance <= 4) {
            throw std::runtime_error("HTTP 404: Not found");
        } else if (chance <= 5) {
            throw std::runtime_error("HTTP 500: Internal server error");
        }
        
        return "Data from " + url;
    }
};

// Custom error handler for demonstration
class DemoErrorHandler {
public:
    static void handleError(std::shared_ptr<ErrorContext> context) {
        std::cout << "\n=== CUSTOM ERROR HANDLER ===" << std::endl;
        
        // Use different formatters based on severity
        std::unique_ptr<ErrorFormatter> formatter;
        
        if (context->getSeverity() >= ErrorSeverity::Critical) {
            formatter = ErrorFormatterFactory::createFormatter(OutputFormat::Colored);
        } else if (context->getSeverity() >= ErrorSeverity::Warning) {
            formatter = ErrorFormatterFactory::createFormatter(OutputFormat::Json);
        } else {
            formatter = ErrorFormatterFactory::createFormatter(OutputFormat::Plain);
        }
        
        std::cout << formatter->format(context) << std::endl;
        
        // Simulate sending alerts for critical errors
        if (context->getSeverity() >= ErrorSeverity::Critical) {
            std::cout << "🚨 ALERT: Critical error detected! Sending notification..." << std::endl;
        }
    }
};

// Demonstrate basic error reporting
void demonstrateBasicErrorReporting() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "BASIC ERROR REPORTING DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Simple error reporting
    REPORT_ERROR(100, "File not found: config.txt");
    
    // Error with correlation ID
    REPORT_ERROR_WITH_CORRELATION(200, "request-12345", "Database connection failed");
    
    // Create rich error context
    auto context = ErrorContext::create(300, "Complex operation failed");
    context->addTag("critical");
    context->addTag("user-facing");
    context->setUserData("user_id", std::string("user_12345"));
    context->setUserData("operation_id", 67890);
    context->setSystemInfo("component", "file_processor");
    context->setSystemInfo("version", "1.0.0");
    
    GlobalErrorHandler::getInstance().reportError(context);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// Demonstrate error recovery mechanisms
void demonstrateErrorRecovery() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "ERROR RECOVERY DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    FileProcessor processor;
    
    // Demonstrate retry with exponential backoff
    std::cout << "\n--- Retry with Exponential Backoff ---" << std::endl;
    
    ErrorRecoveryExecutor<std::string> retryExecutor;
    retryExecutor.withRetryPolicy(
        RecoveryStrategyFactory::createExponentialBackoff(3, std::chrono::milliseconds(100))
    );
    
    try {
        std::string result = retryExecutor.execute([&processor]() {
            return processor.readFile("important.txt");
        });
        std::cout << "✅ Success: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cout << "❌ All retry attempts failed: " << e.what() << std::endl;
    }
    
    // Demonstrate fallback strategy
    std::cout << "\n--- Fallback Strategy ---" << std::endl;
    
    ErrorRecoveryExecutor<std::string> fallbackExecutor;
    fallbackExecutor.withFallback(
        RecoveryStrategyFactory::createDefaultFallback<std::string>("Default content")
    );
    
    try {
        std::string result = fallbackExecutor.execute([&processor]() {
            return processor.readFile("missing.txt");
        });
        std::cout << "✅ Result (with fallback): " << result << std::endl;
    } catch (const std::exception& e) {
        std::cout << "❌ Even fallback failed: " << e.what() << std::endl;
    }
    
    // Demonstrate circuit breaker
    std::cout << "\n--- Circuit Breaker Pattern ---" << std::endl;
    
    auto circuitBreaker = RecoveryStrategyFactory::createCircuitBreaker(2, std::chrono::milliseconds(500));
    ErrorRecoveryExecutor<std::string> circuitExecutor;
    circuitExecutor.withCircuitBreaker(circuitBreaker);
    
    NetworkClient client;
    
    for (int i = 0; i < 5; ++i) {
        try {
            std::string result = circuitExecutor.execute([&client]() {
                return client.fetchData("https://api.example.com/data");
            });
            std::cout << "✅ Attempt " << (i + 1) << ": " << result << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ Attempt " << (i + 1) << ": " << e.what() << std::endl;
            std::cout << "   Circuit state: " << static_cast<int>(circuitBreaker->getState()) << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Demonstrate error formatting
void demonstrateErrorFormatting() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "ERROR FORMATTING DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Create a sample error context
    auto context = ErrorContext::create(500, "Database connection timeout");
    context->addTag("database");
    context->addTag("timeout");
    context->setCorrelationId("req-789");
    context->setUserData("query", std::string("SELECT * FROM users"));
    context->setUserData("timeout_ms", 5000);
    context->setSystemInfo("database_host", "db.example.com");
    context->setSystemInfo("connection_pool_size", "10");
    
    // Demonstrate different formatters
    std::cout << "\n--- Plain Text Format ---" << std::endl;
    auto plainFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Plain);
    std::cout << plainFormatter->format(context) << std::endl;
    
    std::cout << "\n--- JSON Format ---" << std::endl;
    auto jsonFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Json);
    std::cout << jsonFormatter->format(context) << std::endl;
    
    std::cout << "\n--- Colored Terminal Format ---" << std::endl;
    auto coloredFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Colored);
    std::cout << coloredFormatter->format(context) << std::endl;
    
    std::cout << "\n--- Structured Logging Format ---" << std::endl;
    auto structuredFormatter = ErrorFormatterFactory::createFormatter(OutputFormat::Structured);
    std::cout << structuredFormatter->format(context) << std::endl;
    
    // Demonstrate template formatting
    std::cout << "\n--- Template Format ---" << std::endl;
    std::string templateStr = "[{timestamp}] {severity} in {category}: {message} (ID: {error_id})";
    TemplateFormatter templateFormatter(templateStr);
    std::cout << templateFormatter.format(context) << std::endl;
}

// Demonstrate error aggregation and statistics
void demonstrateErrorAggregation() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "ERROR AGGREGATION DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    ErrorAggregator aggregator;
    aggregator.addStrategy(AggregationStrategy::BySeverity);
    aggregator.addStrategy(AggregationStrategy::ByCategory);
    
    // Generate various errors
    std::vector<std::shared_ptr<ErrorContext>> contexts = {
        ErrorContext::create(100, "File not found"),
        ErrorContext::create(200, "Network timeout"),
        ErrorContext::create(300, "Invalid input"),
        ErrorContext::create(400, "Permission denied"),
        ErrorContext::create(500, "Database error"),
        ErrorContext::create(600, "Memory allocation failed")
    };
    
    // Add errors to aggregator
    for (const auto& context : contexts) {
        aggregator.addError(context);
    }
    
    // Display aggregated results
    auto results = aggregator.getAggregatedErrors();
    
    std::cout << "\nAggregated Error Summary:" << std::endl;
    for (const auto& [key, errors] : results) {
        std::cout << "  " << key << ": " << errors.size() << " errors" << std::endl;
    }
    
    // Display error reporter statistics
    auto& reporter = GlobalErrorHandler::getInstance().getReporter();
    auto stats = reporter.getStatistics();
    
    std::cout << "\nError Reporter Statistics:" << std::endl;
    for (const auto& [key, value] : stats) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
}

// Demonstrate concurrent error handling
void demonstrateConcurrentErrorHandling() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "CONCURRENT ERROR HANDLING DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    const int numThreads = 5;
    const int errorsPerThread = 20;
    std::atomic<int> totalErrors(0);
    
    std::vector<std::thread> threads;
    
    // Launch threads that generate errors concurrently
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, errorsPerThread, &totalErrors]() {
            FileProcessor processor;
            
            for (int i = 0; i < errorsPerThread; ++i) {
                try {
                    processor.readFile("thread_" + std::to_string(t) + "_file_" + std::to_string(i) + ".txt");
                } catch (const std::exception& e) {
                    auto context = ErrorContext::create(100 + t, e.what());
                    context->addTag("concurrent_test");
                    context->addTag("thread_" + std::to_string(t));
                    context->setUserData("thread_id", t);
                    context->setUserData("iteration", i);
                    
                    GlobalErrorHandler::getInstance().reportError(context);
                    totalErrors++;
                }
                
                // Small delay to simulate work
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "Generated " << totalErrors.load() << " errors from " << numThreads << " threads" << std::endl;
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Display final statistics
    auto& reporter = GlobalErrorHandler::getInstance().getReporter();
    auto stats = reporter.getStatistics();
    std::cout << "Processed " << stats["processed_errors"] << " errors" << std::endl;
}

int main() {
    std::cout << "🚀 Atom Error Handling System Demonstration" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Initialize the global error handler
    GlobalErrorHandler::getInstance().initialize();
    
    // Set up custom error handler
    GlobalErrorHandler::getInstance().setGlobalHandler(DemoErrorHandler::handleError);
    
    try {
        // Run demonstrations
        demonstrateBasicErrorReporting();
        demonstrateErrorRecovery();
        demonstrateErrorFormatting();
        demonstrateErrorAggregation();
        demonstrateConcurrentErrorHandling();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "DEMONSTRATION COMPLETED SUCCESSFULLY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Demonstration failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Clean up
    GlobalErrorHandler::getInstance().shutdown();
    ErrorContextManager::getInstance().clear();
    
    std::cout << "\n✅ All demonstrations completed successfully!" << std::endl;
    std::cout << "Check the output above to see the error handling system in action." << std::endl;
    
    return 0;
}
