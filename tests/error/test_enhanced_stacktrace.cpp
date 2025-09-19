/*
 * test_enhanced_stacktrace.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit tests for enhanced stacktrace functionality with external library support

**************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "atom/error/stacktrace.hpp"

namespace atom::error::test {

class EnhancedStackTraceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset to default configuration
        StackTrace::setDefaultConfig(StackTraceConfig{});
        StackTrace::setPreferredBackend("auto");
    }
};

// ============================================================================
// StackFrame Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, StackFrameToString) {
    StackFrame frame;
    frame.address = reinterpret_cast<void*>(0x12345678);
    frame.function = "testFunction";
    frame.module = "/path/to/module.so";
    frame.sourceFile = "/path/to/source.cpp";
    frame.sourceLine = 42;
    frame.offset = 0x100;
    
    StackTraceConfig config;
    std::string result = frame.toString(config);
    
    EXPECT_TRUE(result.find("testFunction") != std::string::npos);
    EXPECT_TRUE(result.find("0x12345678") != std::string::npos);
    EXPECT_TRUE(result.find("module.so") != std::string::npos);
    EXPECT_TRUE(result.find("source.cpp:42") != std::string::npos);
}

TEST_F(EnhancedStackTraceTest, StackFrameConfigOptions) {
    StackFrame frame;
    frame.address = reinterpret_cast<void*>(0x12345678);
    frame.function = "testFunction";
    frame.module = "/path/to/module.so";
    frame.sourceFile = "/path/to/source.cpp";
    frame.sourceLine = 42;
    
    // Test without addresses
    StackTraceConfig config;
    config.includeAddresses = false;
    std::string result = frame.toString(config);
    EXPECT_TRUE(result.find("0x12345678") == std::string::npos);
    
    // Test without modules
    config.includeAddresses = true;
    config.includeModules = false;
    result = frame.toString(config);
    EXPECT_TRUE(result.find("module.so") == std::string::npos);
    
    // Test without source info
    config.includeModules = true;
    config.includeSourceInfo = false;
    result = frame.toString(config);
    EXPECT_TRUE(result.find("source.cpp:42") == std::string::npos);
}

// ============================================================================
// StackTrace Configuration Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, DefaultConfiguration) {
    StackTraceConfig config;
    EXPECT_EQ(config.maxDepth, 128);
    EXPECT_EQ(config.skipFrames, 1);
    EXPECT_TRUE(config.includeAddresses);
    EXPECT_TRUE(config.includeModules);
    EXPECT_TRUE(config.includeSourceInfo);
    EXPECT_TRUE(config.demangle);
    EXPECT_TRUE(config.prettify);
}

TEST_F(EnhancedStackTraceTest, CustomConfiguration) {
    StackTraceConfig config;
    config.maxDepth = 64;
    config.skipFrames = 2;
    config.includeAddresses = false;
    config.framePrefix = "  -> ";
    config.unknownFunction = "<no function>";
    
    StackTrace trace(config);
    std::string result = trace.toString();
    
    // Should use custom prefix
    EXPECT_TRUE(result.find("  -> ") != std::string::npos);
}

TEST_F(EnhancedStackTraceTest, GlobalDefaultConfig) {
    StackTraceConfig customConfig;
    customConfig.maxDepth = 32;
    customConfig.framePrefix = ">>> ";
    
    StackTrace::setDefaultConfig(customConfig);
    
    const auto& retrievedConfig = StackTrace::getDefaultConfig();
    EXPECT_EQ(retrievedConfig.maxDepth, 32);
    EXPECT_EQ(retrievedConfig.framePrefix, ">>> ");
    
    // New traces should use the default config
    StackTrace trace;
    std::string result = trace.toString();
    EXPECT_TRUE(result.find(">>> ") != std::string::npos);
}

// ============================================================================
// Backend Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, AvailableBackends) {
    auto backends = StackTrace::getAvailableBackends();
    
    // Builtin should always be available
    EXPECT_TRUE(std::find(backends.begin(), backends.end(), "builtin") != backends.end());
    
    // Check for external libraries if compiled with them
#ifdef ATOM_USE_CPPTRACE
    EXPECT_TRUE(std::find(backends.begin(), backends.end(), "cpptrace") != backends.end());
#endif

#ifdef ATOM_USE_BACKWARD_CPP
    EXPECT_TRUE(std::find(backends.begin(), backends.end(), "backward") != backends.end());
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
    EXPECT_TRUE(std::find(backends.begin(), backends.end(), "boost") != backends.end());
#endif
}

TEST_F(EnhancedStackTraceTest, PreferredBackend) {
    // Test setting preferred backend
    StackTrace::setPreferredBackend("builtin");
    
    StackTrace trace;
    EXPECT_EQ(trace.getBackendName(), "builtin");
    
    // Reset to auto
    StackTrace::setPreferredBackend("auto");
}

// ============================================================================
// Stacktrace Capture Tests
// ============================================================================

// Helper functions to create a call stack
void deepFunction3() {
    StackTrace trace;
    EXPECT_FALSE(trace.empty());
    EXPECT_GT(trace.size(), 0);
    
    // Should contain this function name (if symbols are available)
    std::string traceStr = trace.toString();
    EXPECT_FALSE(traceStr.empty());
    EXPECT_TRUE(traceStr.find("Stack trace:") != std::string::npos);
}

void deepFunction2() {
    deepFunction3();
}

void deepFunction1() {
    deepFunction2();
}

TEST_F(EnhancedStackTraceTest, BasicCapture) {
    deepFunction1();
}

TEST_F(EnhancedStackTraceTest, EmptyTrace) {
    StackTraceConfig config;
    config.maxDepth = 0;
    
    StackTrace trace(config);
    EXPECT_TRUE(trace.empty());
    EXPECT_EQ(trace.size(), 0);
    
    std::string result = trace.toString();
    EXPECT_TRUE(result.find("<empty>") != std::string::npos);
}

TEST_F(EnhancedStackTraceTest, LimitedDepth) {
    StackTraceConfig config;
    config.maxDepth = 3;
    
    StackTrace trace(config);
    EXPECT_LE(trace.size(), 3);
}

// ============================================================================
// Frame Filtering Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, FrameFiltering) {
    StackTraceConfig config;
    config.frameFilter = [](const std::string& frameInfo, int frameIndex) {
        // Filter out even-numbered frames
        return frameIndex % 2 == 0;
    };
    
    StackTrace trace;
    std::string filtered = trace.toString(config);
    std::string unfiltered = trace.toString();
    
    // Filtered version should be shorter (unless trace is very short)
    if (trace.size() > 2) {
        EXPECT_LT(filtered.length(), unfiltered.length());
    }
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, UtilityFunctions) {
    // Test address formatting
    std::string addr = stacktrace_utils::formatAddress(0x12345678);
    EXPECT_TRUE(addr.find("0x") == 0);
    EXPECT_TRUE(addr.find("12345678") != std::string::npos);
    
    // Test base name extraction
    std::string baseName = stacktrace_utils::getBaseName("/path/to/file.cpp");
    EXPECT_EQ(baseName, "file.cpp");
    
    baseName = stacktrace_utils::getBaseName("C:\\path\\to\\file.cpp");
    EXPECT_EQ(baseName, "file.cpp");
    
    baseName = stacktrace_utils::getBaseName("file.cpp");
    EXPECT_EQ(baseName, "file.cpp");
    
    // Test mangled name detection
    EXPECT_TRUE(stacktrace_utils::containsMangledNames("_Z3foov"));
    EXPECT_TRUE(stacktrace_utils::containsMangledNames("__Z3foov"));
    EXPECT_TRUE(stacktrace_utils::containsMangledNames("?foo@@YAXXZ")); // MSVC
    EXPECT_FALSE(stacktrace_utils::containsMangledNames("plain_function"));
}

TEST_F(EnhancedStackTraceTest, Prettification) {
    std::string input = "std::__1::vector<int, std::allocator<int>> class MyClass::method()";
    std::string prettified = stacktrace_utils::prettify(input);
    
    // Should remove std::__1:: and std::allocator
    EXPECT_TRUE(prettified.find("std::__1::") == std::string::npos);
    EXPECT_TRUE(prettified.find("std::allocator") == std::string::npos);
    EXPECT_TRUE(prettified.find("class ") == std::string::npos);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, ConvenienceFunctions) {
    // Test basic current() function
    std::string trace1 = stacktrace::current();
    EXPECT_FALSE(trace1.empty());
    EXPECT_TRUE(trace1.find("Stack trace:") != std::string::npos);
    
    // Test current() with depth limit
    std::string trace2 = stacktrace::current(5);
    EXPECT_FALSE(trace2.empty());
    
    // Test current() with custom config
    StackTraceConfig config;
    config.framePrefix = "## ";
    std::string trace3 = stacktrace::current(config);
    EXPECT_TRUE(trace3.find("## ") != std::string::npos);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, ThreadSafety) {
    const int numThreads = 4;
    const int tracesPerThread = 10;
    std::vector<std::thread> threads;
    std::vector<std::string> results(numThreads * tracesPerThread);
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&results, t, tracesPerThread]() {
            for (int i = 0; i < tracesPerThread; ++i) {
                StackTrace trace;
                results[t * tracesPerThread + i] = trace.toString();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All results should be non-empty
    for (const auto& result : results) {
        EXPECT_FALSE(result.empty());
        EXPECT_TRUE(result.find("Stack trace:") != std::string::npos);
    }
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(EnhancedStackTraceTest, PerformanceBaseline) {
    const int numTraces = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numTraces; ++i) {
        StackTrace trace;
        volatile std::string result = trace.toString();
        (void)result; // Prevent optimization
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should be reasonably fast (less than 5 seconds for 100 traces)
    EXPECT_LT(duration.count(), 5000);
    
    std::cout << "Captured " << numTraces << " stack traces in " 
              << duration.count() << "ms" << std::endl;
}

} // namespace atom::error::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
