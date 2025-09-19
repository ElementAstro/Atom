/*
 * enhanced_stacktrace_demo.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Demonstration of enhanced stacktrace functionality with external library support

**************************************************/

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "atom/error/stacktrace.hpp"

using namespace atom::error;

// Helper functions to create a meaningful call stack
class DatabaseConnection {
public:
    void connect() {
        authenticate();
    }
    
private:
    void authenticate() {
        validateCredentials();
    }
    
    void validateCredentials() {
        // This is where we'll capture the stack trace
        demonstrateBasicCapture();
    }
    
    void demonstrateBasicCapture() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "BASIC STACKTRACE CAPTURE" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        StackTrace trace;
        std::cout << "Backend used: " << trace.getBackendName() << std::endl;
        std::cout << "Frames captured: " << trace.size() << std::endl;
        std::cout << "\n" << trace.toString() << std::endl;
    }
};

void demonstrateAvailableBackends() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "AVAILABLE STACKTRACE BACKENDS" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    auto backends = StackTrace::getAvailableBackends();
    std::cout << "Available backends:" << std::endl;
    for (const auto& backend : backends) {
        std::cout << "  - " << backend << std::endl;
    }
    
    // Test each backend if available
    for (const auto& backend : backends) {
        std::cout << "\nTesting backend: " << backend << std::endl;
        StackTrace::setPreferredBackend(backend);
        
        StackTrace trace;
        std::cout << "  Frames captured: " << trace.size() << std::endl;
        std::cout << "  Backend confirmed: " << trace.getBackendName() << std::endl;
    }
    
    // Reset to auto selection
    StackTrace::setPreferredBackend("auto");
}

void demonstrateCustomConfiguration() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "CUSTOM CONFIGURATION DEMO" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Configuration 1: Minimal output
    std::cout << "\n--- Minimal Configuration ---" << std::endl;
    StackTraceConfig minimalConfig;
    minimalConfig.includeAddresses = false;
    minimalConfig.includeModules = false;
    minimalConfig.includeSourceInfo = false;
    minimalConfig.framePrefix = "  ";
    minimalConfig.maxDepth = 5;
    
    StackTrace minimalTrace(minimalConfig);
    std::cout << minimalTrace.toString() << std::endl;
    
    // Configuration 2: Detailed output
    std::cout << "\n--- Detailed Configuration ---" << std::endl;
    StackTraceConfig detailedConfig;
    detailedConfig.includeAddresses = true;
    detailedConfig.includeModules = true;
    detailedConfig.includeSourceInfo = true;
    detailedConfig.framePrefix = ">>> ";
    detailedConfig.maxDepth = 10;
    detailedConfig.prettify = true;
    
    StackTrace detailedTrace(detailedConfig);
    std::cout << detailedTrace.toString() << std::endl;
    
    // Configuration 3: With frame filtering
    std::cout << "\n--- Filtered Configuration ---" << std::endl;
    StackTraceConfig filteredConfig;
    filteredConfig.frameFilter = [](const std::string& frameInfo, int frameIndex) {
        // Only show frames that contain certain keywords or are at specific indices
        return frameIndex < 5 && (
            frameInfo.find("demonstrate") != std::string::npos ||
            frameInfo.find("main") != std::string::npos ||
            frameIndex == 0
        );
    };
    
    StackTrace filteredTrace;
    std::cout << filteredTrace.toString(filteredConfig) << std::endl;
}

void demonstrateUtilityFunctions() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "UTILITY FUNCTIONS DEMO" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Address formatting
    std::cout << "Address formatting examples:" << std::endl;
    std::cout << "  " << stacktrace_utils::formatAddress(0x12345678) << std::endl;
    std::cout << "  " << stacktrace_utils::formatAddress(0xDEADBEEF) << std::endl;
    
    // Path manipulation
    std::cout << "\nPath manipulation examples:" << std::endl;
    std::cout << "  " << stacktrace_utils::getBaseName("/usr/local/lib/libexample.so") 
              << " (from /usr/local/lib/libexample.so)" << std::endl;
    std::cout << "  " << stacktrace_utils::getBaseName("C:\\Windows\\System32\\kernel32.dll") 
              << " (from C:\\Windows\\System32\\kernel32.dll)" << std::endl;
    
    // Mangled name detection
    std::cout << "\nMangled name detection:" << std::endl;
    std::vector<std::string> testNames = {
        "_Z3foov",
        "__Z12exampleFunci",
        "?foo@@YAXXZ",
        "plain_function_name",
        "std::vector<int>::push_back"
    };
    
    for (const auto& name : testNames) {
        bool isMangled = stacktrace_utils::containsMangledNames(name);
        std::cout << "  " << std::setw(25) << std::left << name 
                  << " -> " << (isMangled ? "mangled" : "not mangled") << std::endl;
    }
    
    // Demangling examples (if available)
    std::cout << "\nDemangling examples:" << std::endl;
    std::vector<std::string> mangledNames = {
        "_Z3foov",
        "_Z12exampleFunci",
        "plain_function"
    };
    
    for (const auto& mangled : mangledNames) {
        std::string demangled = stacktrace_utils::demangle(mangled);
        std::cout << "  " << std::setw(20) << std::left << mangled 
                  << " -> " << demangled << std::endl;
    }
}

void demonstrateConvenienceFunctions() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "CONVENIENCE FUNCTIONS DEMO" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Basic current() function
    std::cout << "\n--- stacktrace::current() ---" << std::endl;
    std::cout << stacktrace::current() << std::endl;
    
    // With depth limit
    std::cout << "\n--- stacktrace::current(3) ---" << std::endl;
    std::cout << stacktrace::current(3) << std::endl;
    
    // With custom configuration
    std::cout << "\n--- stacktrace::current(config) ---" << std::endl;
    StackTraceConfig config;
    config.framePrefix = ">> ";
    config.maxDepth = 5;
    config.includeAddresses = false;
    std::cout << stacktrace::current(config) << std::endl;
}

void demonstratePerformance() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "PERFORMANCE DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    const int numTraces = 1000;
    
    // Test different backends if available
    auto backends = StackTrace::getAvailableBackends();
    
    for (const auto& backend : backends) {
        std::cout << "\nTesting performance of " << backend << " backend:" << std::endl;
        StackTrace::setPreferredBackend(backend);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numTraces; ++i) {
            StackTrace trace;
            volatile std::string result = trace.toString();
            (void)result; // Prevent optimization
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avgTime = static_cast<double>(duration.count()) / numTraces;
        std::cout << "  " << numTraces << " traces in " << duration.count() << " μs" << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(2) << avgTime << " μs per trace" << std::endl;
        std::cout << "  Rate: " << std::fixed << std::setprecision(0) << (1000000.0 / avgTime) << " traces/second" << std::endl;
    }
    
    // Reset to auto selection
    StackTrace::setPreferredBackend("auto");
}

void demonstrateThreadSafety() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "THREAD SAFETY DEMONSTRATION" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    const int numThreads = 4;
    const int tracesPerThread = 50;
    std::vector<std::thread> threads;
    
    std::cout << "Launching " << numThreads << " threads, each capturing " 
              << tracesPerThread << " stack traces..." << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, tracesPerThread]() {
            for (int i = 0; i < tracesPerThread; ++i) {
                StackTrace trace;
                volatile std::string result = trace.toString();
                (void)result; // Prevent optimization
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int totalTraces = numThreads * tracesPerThread;
    std::cout << "Successfully captured " << totalTraces << " traces across " 
              << numThreads << " threads in " << duration.count() << "ms" << std::endl;
    std::cout << "Average: " << (static_cast<double>(duration.count()) / totalTraces) 
              << "ms per trace" << std::endl;
}

void demonstrateGlobalConfiguration() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "GLOBAL CONFIGURATION DEMO" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Save current default config
    auto originalConfig = StackTrace::getDefaultConfig();
    
    // Set custom global configuration
    StackTraceConfig globalConfig;
    globalConfig.framePrefix = "*** ";
    globalConfig.maxDepth = 8;
    globalConfig.includeAddresses = false;
    globalConfig.unknownFunction = "<mystery function>";
    
    StackTrace::setDefaultConfig(globalConfig);
    
    std::cout << "Set global configuration with custom prefix and settings." << std::endl;
    std::cout << "New traces will use these settings by default:" << std::endl;
    
    StackTrace trace1;
    std::cout << trace1.toString() << std::endl;
    
    // Traces with explicit config override global settings
    std::cout << "\nTrace with explicit config (overrides global):" << std::endl;
    StackTraceConfig explicitConfig;
    explicitConfig.framePrefix = ">>> ";
    explicitConfig.maxDepth = 3;
    
    StackTrace trace2(explicitConfig);
    std::cout << trace2.toString() << std::endl;
    
    // Restore original configuration
    StackTrace::setDefaultConfig(originalConfig);
    std::cout << "\nRestored original global configuration." << std::endl;
}

int main() {
    std::cout << "🚀 Enhanced Stacktrace System Demonstration" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        // Create a meaningful call stack
        DatabaseConnection db;
        db.connect();
        
        // Demonstrate various features
        demonstrateAvailableBackends();
        demonstrateCustomConfiguration();
        demonstrateUtilityFunctions();
        demonstrateConvenienceFunctions();
        demonstrateGlobalConfiguration();
        demonstratePerformance();
        demonstrateThreadSafety();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "DEMONSTRATION COMPLETED SUCCESSFULLY" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Demonstration failed: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n✅ All demonstrations completed successfully!" << std::endl;
    std::cout << "The enhanced stacktrace system supports:" << std::endl;
    std::cout << "  • Multiple backends (builtin, cpptrace, backward-cpp, boost)" << std::endl;
    std::cout << "  • Configurable output formatting" << std::endl;
    std::cout << "  • Frame filtering and customization" << std::endl;
    std::cout << "  • Thread-safe operation" << std::endl;
    std::cout << "  • High-performance capture" << std::endl;
    std::cout << "  • Backward compatibility" << std::endl;
    
    return 0;
}
