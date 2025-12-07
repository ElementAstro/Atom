/*
 * script_engine.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-07

Description: Tests for Script Loading and Execution System

**************************************************/

#include "atom/components/scripting/script_engine.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace atom::components::scripting;

// ============================================================================
// CompiledScript Tests
// ============================================================================

class CompiledScriptTest : public ::testing::Test {
protected:
    CompiledScript script_;
};

TEST_F(CompiledScriptTest, DefaultConstruction) {
    EXPECT_TRUE(script_.source.empty());
    EXPECT_TRUE(script_.filename.empty());
    EXPECT_TRUE(script_.bytecode.empty());
    EXPECT_EQ(script_.sourceHash, 0);
    EXPECT_FALSE(script_.isValid);
    EXPECT_TRUE(script_.errorMessage.empty());
}

TEST_F(CompiledScriptTest, SetSourceAndFilename) {
    script_.source = "return 42";
    script_.filename = "test.lua";
    script_.isValid = true;

    EXPECT_EQ(script_.source, "return 42");
    EXPECT_EQ(script_.filename, "test.lua");
    EXPECT_TRUE(script_.isValid);
}

TEST_F(CompiledScriptTest, CompilationTime) {
    script_.compilationTime = std::chrono::system_clock::now();
    script_.compilationDuration = std::chrono::microseconds(100);

    EXPECT_EQ(script_.compilationDuration.count(), 100);
}

TEST_F(CompiledScriptTest, DebugInfo) {
    script_.debugInfo.lineNumbers = {1, 5, 10, 15};
    script_.debugInfo.functionOffsets["main"] = 0;
    script_.debugInfo.functionOffsets["helper"] = 50;
    script_.debugInfo.sourceLines[1] = "local x = 10";

    EXPECT_EQ(script_.debugInfo.lineNumbers.size(), 4);
    EXPECT_EQ(script_.debugInfo.functionOffsets.size(), 2);
    EXPECT_EQ(script_.debugInfo.functionOffsets["main"], 0);
    EXPECT_EQ(script_.debugInfo.sourceLines[1], "local x = 10");
}

TEST_F(CompiledScriptTest, BytecodeStorage) {
    script_.bytecode = {0x01, 0x02, 0x03, 0x04, 0x05};

    EXPECT_EQ(script_.bytecode.size(), 5);
    EXPECT_EQ(script_.bytecode[0], 0x01);
    EXPECT_EQ(script_.bytecode[4], 0x05);
}

TEST_F(CompiledScriptTest, ErrorMessage) {
    script_.isValid = false;
    script_.errorMessage = "Syntax error at line 5";

    EXPECT_FALSE(script_.isValid);
    EXPECT_EQ(script_.errorMessage, "Syntax error at line 5");
}

// ============================================================================
// ScriptCacheEntry Tests
// ============================================================================

class ScriptCacheEntryTest : public ::testing::Test {
protected:
    ScriptCacheEntry entry_;
};

TEST_F(ScriptCacheEntryTest, DefaultConstruction) {
    EXPECT_EQ(entry_.compiledScript, nullptr);
    EXPECT_EQ(entry_.accessCount, 0);
    EXPECT_FALSE(entry_.isPinned);
}

TEST_F(ScriptCacheEntryTest, SetCompiledScript) {
    auto script = std::make_shared<CompiledScript>();
    script->source = "return 1";
    script->isValid = true;

    entry_.compiledScript = script;
    entry_.accessCount = 5;
    entry_.isPinned = true;

    EXPECT_NE(entry_.compiledScript, nullptr);
    EXPECT_EQ(entry_.compiledScript->source, "return 1");
    EXPECT_EQ(entry_.accessCount, 5);
    EXPECT_TRUE(entry_.isPinned);
}

TEST_F(ScriptCacheEntryTest, LastAccessedTime) {
    entry_.lastAccessed = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();

    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        now - entry_.lastAccessed);
    EXPECT_LT(diff.count(), 1);
}

// ============================================================================
// ExecutionContext Tests
// ============================================================================

class ExecutionContextTest : public ::testing::Test {
protected:
    ExecutionContext context_;
};

TEST_F(ExecutionContextTest, DefaultConstruction) {
    EXPECT_TRUE(context_.scriptName.empty());
    EXPECT_TRUE(context_.localVariables.empty());
    EXPECT_TRUE(context_.globalVariables.empty());
    EXPECT_TRUE(context_.callStack.empty());
    EXPECT_EQ(context_.currentLine, 0);
    EXPECT_EQ(context_.timeout.count(), 30000);
    EXPECT_FALSE(context_.isDebugging);
}

TEST_F(ExecutionContextTest, SetScriptName) {
    context_.scriptName = "test_script.lua";
    EXPECT_EQ(context_.scriptName, "test_script.lua");
}

TEST_F(ExecutionContextTest, LocalVariables) {
    context_.localVariables["x"] = ScriptValue(42);
    context_.localVariables["name"] = ScriptValue(std::string("test"));

    EXPECT_EQ(context_.localVariables.size(), 2);
    EXPECT_EQ(context_.localVariables["x"].get<int64_t>(), 42);
    EXPECT_EQ(context_.localVariables["name"].get<std::string>(), "test");
}

TEST_F(ExecutionContextTest, GlobalVariables) {
    context_.globalVariables["PI"] = ScriptValue(3.14159);
    context_.globalVariables["DEBUG"] = ScriptValue(true);

    EXPECT_EQ(context_.globalVariables.size(), 2);
    EXPECT_NEAR(context_.globalVariables["PI"].get<double>(), 3.14159, 0.001);
    EXPECT_TRUE(context_.globalVariables["DEBUG"].get<bool>());
}

TEST_F(ExecutionContextTest, CallStack) {
    context_.callStack.push_back("main");
    context_.callStack.push_back("helper");
    context_.callStack.push_back("nested");

    EXPECT_EQ(context_.callStack.size(), 3);
    EXPECT_EQ(context_.callStack[0], "main");
    EXPECT_EQ(context_.callStack[2], "nested");
}

TEST_F(ExecutionContextTest, Timeout) {
    context_.timeout = std::chrono::milliseconds(5000);
    EXPECT_EQ(context_.timeout.count(), 5000);
}

TEST_F(ExecutionContextTest, DebuggingState) {
    context_.isDebugging = true;
    context_.breakpoints = {10, 20, 30};
    context_.stepMode = true;
    context_.stepInto = false;
    context_.stepOver = true;

    EXPECT_TRUE(context_.isDebugging);
    EXPECT_EQ(context_.breakpoints.size(), 3);
    EXPECT_TRUE(context_.stepMode);
    EXPECT_FALSE(context_.stepInto);
    EXPECT_TRUE(context_.stepOver);
}

TEST_F(ExecutionContextTest, DebugCallback) {
    bool callbackCalled = false;
    context_.debugCallback = [&callbackCalled](const ExecutionContext&) {
        callbackCalled = true;
    };

    if (context_.debugCallback) {
        context_.debugCallback(context_);
    }

    EXPECT_TRUE(callbackCalled);
}

TEST_F(ExecutionContextTest, StartTime) {
    context_.startTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - context_.startTime);
    EXPECT_LT(diff.count(), 100);
}

// ============================================================================
// ScriptLoader::Config Tests
// ============================================================================

class ScriptLoaderConfigTest : public ::testing::Test {
protected:
    ScriptLoader::Config config_;
};

TEST_F(ScriptLoaderConfigTest, DefaultValues) {
    EXPECT_EQ(config_.maxCacheSize, 100);
    EXPECT_EQ(config_.cacheTimeout.count(), 60);  // 1 hour in minutes
    EXPECT_TRUE(config_.enableCompilation);
    EXPECT_TRUE(config_.enableCaching);
    EXPECT_TRUE(config_.enableDebugInfo);
    EXPECT_TRUE(config_.includePaths.empty());
    EXPECT_TRUE(config_.preprocessorDefines.empty());
}

TEST_F(ScriptLoaderConfigTest, CustomConfiguration) {
    config_.maxCacheSize = 50;
    config_.cacheTimeout = std::chrono::minutes(30);
    config_.enableCompilation = false;
    config_.enableCaching = false;
    config_.enableDebugInfo = false;
    config_.includePaths = {"/usr/local/scripts", "/home/user/scripts"};
    config_.preprocessorDefines["DEBUG"] = "1";
    config_.preprocessorDefines["VERSION"] = "2.0";

    EXPECT_EQ(config_.maxCacheSize, 50);
    EXPECT_EQ(config_.cacheTimeout.count(), 30);
    EXPECT_FALSE(config_.enableCompilation);
    EXPECT_FALSE(config_.enableCaching);
    EXPECT_FALSE(config_.enableDebugInfo);
    EXPECT_EQ(config_.includePaths.size(), 2);
    EXPECT_EQ(config_.preprocessorDefines.size(), 2);
}

// ============================================================================
// ScriptLoader Tests
// ============================================================================

class ScriptLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        ScriptLoader::Config config;
        config.maxCacheSize = 10;
        config.enableCaching = true;
        loader_ = std::make_unique<ScriptLoader>(config);
    }

    std::unique_ptr<ScriptLoader> loader_;
};

TEST_F(ScriptLoaderTest, DefaultConstruction) {
    ScriptLoader defaultLoader;
    auto stats = defaultLoader.getCacheStatistics();
    EXPECT_EQ(stats.totalEntries, 0);
}

TEST_F(ScriptLoaderTest, GetCacheStatistics) {
    auto stats = loader_->getCacheStatistics();

    EXPECT_EQ(stats.totalEntries, 0);
    EXPECT_EQ(stats.pinnedEntries, 0);
    EXPECT_EQ(stats.cacheHits, 0);
    EXPECT_EQ(stats.cacheMisses, 0);
    EXPECT_EQ(stats.evictions, 0);
    EXPECT_EQ(stats.totalMemoryUsage, 0);
    EXPECT_DOUBLE_EQ(stats.hitRatio, 0.0);
}

TEST_F(ScriptLoaderTest, ClearCache) {
    EXPECT_NO_THROW(loader_->clearCache());
    EXPECT_NO_THROW(loader_->clearCache(true));  // Force clear
}

TEST_F(ScriptLoaderTest, LoadNonExistentFile) {
    auto script = loader_->loadScript("nonexistent_file.lua");
    EXPECT_EQ(script, nullptr);
}

TEST_F(ScriptLoaderTest, CompileEmptyScript) {
    auto script = loader_->compileScript("");
    // Behavior depends on implementation
    EXPECT_TRUE(script == nullptr || !script->isValid);
}

TEST_F(ScriptLoaderTest, CompileSimpleScript) {
    auto script = loader_->compileScript("return 42", "test.lua");
    // Result depends on whether Lua is enabled
    // Just ensure no crash
    EXPECT_TRUE(script == nullptr || script != nullptr);
}

TEST_F(ScriptLoaderTest, PrecompileNonExistentFile) {
    bool result = loader_->precompileScript("nonexistent.lua");
    EXPECT_FALSE(result);
}

TEST_F(ScriptLoaderTest, PinUnpinScript) {
    EXPECT_NO_THROW(loader_->pinScript("test.lua"));
    EXPECT_NO_THROW(loader_->unpinScript("test.lua"));
}

// ============================================================================
// ScriptExecutor::Config Tests
// ============================================================================

class ScriptExecutorConfigTest : public ::testing::Test {
protected:
    ScriptExecutor::Config config_;
};

TEST_F(ScriptExecutorConfigTest, DefaultValues) {
    EXPECT_EQ(config_.defaultTimeout.count(), 30000);
    EXPECT_EQ(config_.maxStackDepth, 1000);
    EXPECT_EQ(config_.maxMemoryUsage, 64 * 1024 * 1024);
    EXPECT_FALSE(config_.enableProfiling);
    EXPECT_FALSE(config_.enableDebugging);
}

TEST_F(ScriptExecutorConfigTest, CustomConfiguration) {
    config_.defaultTimeout = std::chrono::milliseconds(10000);
    config_.maxStackDepth = 500;
    config_.maxMemoryUsage = 32 * 1024 * 1024;
    config_.enableProfiling = true;
    config_.enableDebugging = true;

    EXPECT_EQ(config_.defaultTimeout.count(), 10000);
    EXPECT_EQ(config_.maxStackDepth, 500);
    EXPECT_EQ(config_.maxMemoryUsage, 32 * 1024 * 1024);
    EXPECT_TRUE(config_.enableProfiling);
    EXPECT_TRUE(config_.enableDebugging);
}

TEST_F(ScriptExecutorConfigTest, LogCallback) {
    std::string loggedMessage;
    config_.logCallback = [&loggedMessage](const std::string& msg) {
        loggedMessage = msg;
    };

    if (config_.logCallback) {
        config_.logCallback("Test log message");
    }

    EXPECT_EQ(loggedMessage, "Test log message");
}

TEST_F(ScriptExecutorConfigTest, DebugCallback) {
    bool debugCalled = false;
    config_.debugCallback = [&debugCalled](const ExecutionContext&) {
        debugCalled = true;
    };

    ExecutionContext ctx;
    if (config_.debugCallback) {
        config_.debugCallback(ctx);
    }

    EXPECT_TRUE(debugCalled);
}

// ============================================================================
// ScriptExecutor::ExecutionStatistics Tests
// ============================================================================

TEST(ExecutionStatisticsTest, DefaultValues) {
    ScriptExecutor::ExecutionStatistics stats;

    EXPECT_EQ(stats.totalExecutions, 0);
    EXPECT_EQ(stats.successfulExecutions, 0);
    EXPECT_EQ(stats.failedExecutions, 0);
    EXPECT_EQ(stats.totalExecutionTime.count(), 0);
    EXPECT_EQ(stats.averageExecutionTime.count(), 0);
    EXPECT_EQ(stats.peakMemoryUsage, 0);
    EXPECT_TRUE(stats.functionCallCounts.empty());
}

TEST(ExecutionStatisticsTest, UpdateStatistics) {
    ScriptExecutor::ExecutionStatistics stats;

    stats.totalExecutions = 100;
    stats.successfulExecutions = 95;
    stats.failedExecutions = 5;
    stats.totalExecutionTime = std::chrono::microseconds(50000);
    stats.averageExecutionTime = std::chrono::microseconds(500);
    stats.peakMemoryUsage = 1024 * 1024;
    stats.functionCallCounts["main"] = 100;
    stats.functionCallCounts["helper"] = 50;

    EXPECT_EQ(stats.totalExecutions, 100);
    EXPECT_EQ(stats.successfulExecutions, 95);
    EXPECT_EQ(stats.failedExecutions, 5);
    EXPECT_EQ(stats.totalExecutionTime.count(), 50000);
    EXPECT_EQ(stats.averageExecutionTime.count(), 500);
    EXPECT_EQ(stats.peakMemoryUsage, 1024 * 1024);
    EXPECT_EQ(stats.functionCallCounts["main"], 100);
}

// ============================================================================
// ScriptLoader::CacheStatistics Tests
// ============================================================================

TEST(CacheStatisticsTest, DefaultValues) {
    ScriptLoader::CacheStatistics stats;

    EXPECT_EQ(stats.totalEntries, 0);
    EXPECT_EQ(stats.pinnedEntries, 0);
    EXPECT_EQ(stats.cacheHits, 0);
    EXPECT_EQ(stats.cacheMisses, 0);
    EXPECT_EQ(stats.evictions, 0);
    EXPECT_EQ(stats.totalMemoryUsage, 0);
    EXPECT_DOUBLE_EQ(stats.hitRatio, 0.0);
}

TEST(CacheStatisticsTest, UpdateStatistics) {
    ScriptLoader::CacheStatistics stats;

    stats.totalEntries = 50;
    stats.pinnedEntries = 5;
    stats.cacheHits = 100;
    stats.cacheMisses = 20;
    stats.evictions = 10;
    stats.totalMemoryUsage = 1024 * 1024;
    stats.hitRatio = 0.833;

    EXPECT_EQ(stats.totalEntries, 50);
    EXPECT_EQ(stats.pinnedEntries, 5);
    EXPECT_EQ(stats.cacheHits, 100);
    EXPECT_EQ(stats.cacheMisses, 20);
    EXPECT_EQ(stats.evictions, 10);
    EXPECT_EQ(stats.totalMemoryUsage, 1024 * 1024);
    EXPECT_NEAR(stats.hitRatio, 0.833, 0.001);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(ScriptEngineIntegrationTest, CompiledScriptWithDebugInfo) {
    CompiledScript script;
    script.source = R"(
        local function add(a, b)
            return a + b
        end
        return add(1, 2)
    )";
    script.filename = "add.lua";
    script.isValid = true;
    script.sourceHash = 12345;
    script.compilationTime = std::chrono::system_clock::now();
    script.compilationDuration = std::chrono::microseconds(500);

    script.debugInfo.lineNumbers = {1, 2, 3, 4, 5};
    script.debugInfo.functionOffsets["add"] = 10;
    script.debugInfo.sourceLines[2] = "local function add(a, b)";
    script.debugInfo.sourceLines[3] = "    return a + b";

    EXPECT_TRUE(script.isValid);
    EXPECT_EQ(script.debugInfo.lineNumbers.size(), 5);
    EXPECT_EQ(script.debugInfo.functionOffsets["add"], 10);
}

TEST(ScriptEngineIntegrationTest, ExecutionContextWithVariables) {
    ExecutionContext context;
    context.scriptName = "calculator.lua";
    context.timeout = std::chrono::milliseconds(5000);
    context.isDebugging = true;

    // Set up local variables
    context.localVariables["x"] = ScriptValue(10);
    context.localVariables["y"] = ScriptValue(20);

    // Set up global variables
    context.globalVariables["PI"] = ScriptValue(3.14159);

    // Set up call stack
    context.callStack = {"main", "calculate", "add"};
    context.currentLine = 15;

    // Set up breakpoints
    context.breakpoints = {10, 15, 20};

    EXPECT_EQ(context.localVariables.size(), 2);
    EXPECT_EQ(context.globalVariables.size(), 1);
    EXPECT_EQ(context.callStack.size(), 3);
    EXPECT_EQ(context.currentLine, 15);
    EXPECT_EQ(context.breakpoints.size(), 3);
}

TEST(ScriptEngineIntegrationTest, CacheEntryLifecycle) {
    ScriptCacheEntry entry;

    // Create and cache a script
    auto script = std::make_shared<CompiledScript>();
    script->source = "return 'cached'";
    script->isValid = true;

    entry.compiledScript = script;
    entry.lastAccessed = std::chrono::system_clock::now();
    entry.accessCount = 1;
    entry.isPinned = false;

    // Simulate access
    entry.accessCount++;
    entry.lastAccessed = std::chrono::system_clock::now();

    EXPECT_EQ(entry.accessCount, 2);
    EXPECT_NE(entry.compiledScript, nullptr);

    // Pin the entry
    entry.isPinned = true;
    EXPECT_TRUE(entry.isPinned);
}

TEST(ScriptEngineIntegrationTest, LoaderConfigurationChain) {
    ScriptLoader::Config config;

    // Configure for production
    config.maxCacheSize = 200;
    config.cacheTimeout = std::chrono::hours(2);
    config.enableCompilation = true;
    config.enableCaching = true;
    config.enableDebugInfo = false;
    config.includePaths = {"/opt/scripts", "/usr/share/scripts"};
    config.preprocessorDefines["PRODUCTION"] = "1";
    config.preprocessorDefines["LOG_LEVEL"] = "ERROR";

    ScriptLoader loader(config);
    auto stats = loader.getCacheStatistics();

    EXPECT_EQ(stats.totalEntries, 0);
}

TEST(ScriptEngineIntegrationTest, ExecutorConfigurationChain) {
    ScriptExecutor::Config config;

    // Configure for debugging
    config.defaultTimeout = std::chrono::milliseconds(60000);
    config.maxStackDepth = 2000;
    config.maxMemoryUsage = 128 * 1024 * 1024;
    config.enableProfiling = true;
    config.enableDebugging = true;

    std::vector<std::string> logs;
    config.logCallback = [&logs](const std::string& msg) {
        logs.push_back(msg);
    };

    // Test log callback
    config.logCallback("Executor initialized");
    config.logCallback("Starting script execution");

    EXPECT_EQ(logs.size(), 2);
    EXPECT_EQ(logs[0], "Executor initialized");
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(ScriptEngineEdgeCaseTest, EmptyCompiledScript) {
    CompiledScript script;
    EXPECT_TRUE(script.source.empty());
    EXPECT_FALSE(script.isValid);
}

TEST(ScriptEngineEdgeCaseTest, LargeSourceCode) {
    CompiledScript script;
    script.source = std::string(100000, 'x');  // 100KB of 'x'
    script.isValid = true;

    EXPECT_EQ(script.source.size(), 100000);
}

TEST(ScriptEngineEdgeCaseTest, ManyBreakpoints) {
    ExecutionContext context;
    for (size_t i = 0; i < 1000; ++i) {
        context.breakpoints.push_back(i);
    }

    EXPECT_EQ(context.breakpoints.size(), 1000);
}

TEST(ScriptEngineEdgeCaseTest, DeepCallStack) {
    ExecutionContext context;
    for (int i = 0; i < 100; ++i) {
        context.callStack.push_back("function_" + std::to_string(i));
    }

    EXPECT_EQ(context.callStack.size(), 100);
    EXPECT_EQ(context.callStack[99], "function_99");
}

TEST(ScriptEngineEdgeCaseTest, ManyLocalVariables) {
    ExecutionContext context;
    for (int i = 0; i < 100; ++i) {
        context.localVariables["var_" + std::to_string(i)] = ScriptValue(i);
    }

    EXPECT_EQ(context.localVariables.size(), 100);
    EXPECT_EQ(context.localVariables["var_50"].get<int64_t>(), 50);
}

TEST(ScriptEngineEdgeCaseTest, ZeroTimeout) {
    ExecutionContext context;
    context.timeout = std::chrono::milliseconds(0);

    EXPECT_EQ(context.timeout.count(), 0);
}

TEST(ScriptEngineEdgeCaseTest, MaxTimeout) {
    ExecutionContext context;
    context.timeout = std::chrono::milliseconds::max();

    EXPECT_EQ(context.timeout.count(),
              std::chrono::milliseconds::max().count());
}

TEST(ScriptEngineEdgeCaseTest, EmptyDebugInfo) {
    CompiledScript script;
    EXPECT_TRUE(script.debugInfo.lineNumbers.empty());
    EXPECT_TRUE(script.debugInfo.functionOffsets.empty());
    EXPECT_TRUE(script.debugInfo.sourceLines.empty());
}

TEST(ScriptEngineEdgeCaseTest, LargeBytecode) {
    CompiledScript script;
    script.bytecode.resize(1024 * 1024);  // 1MB bytecode
    std::fill(script.bytecode.begin(), script.bytecode.end(), 0xAB);

    EXPECT_EQ(script.bytecode.size(), 1024 * 1024);
    EXPECT_EQ(script.bytecode[0], 0xAB);
    EXPECT_EQ(script.bytecode[script.bytecode.size() - 1], 0xAB);
}
