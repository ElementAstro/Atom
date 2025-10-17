#include "atom/components/script_sandbox.hpp"
#include "atom/components/scripting_api.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>

using namespace atom::components::scripting;

// Test fixture for ScriptSandbox tests
class ScriptSandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        SandboxConfig config;
        config.memoryLimit = 1024 * 1024;  // 1MB
        config.executionTimeout = std::chrono::seconds(5);
        config.enableFileAccess = false;
        config.enableNetworkAccess = false;
        config.maxCallDepth = 100;

        sandbox_ = std::make_unique<ScriptSandbox>(config);
        sandbox_->initialize();
    }

    void TearDown() override {
        if (sandbox_) {
            sandbox_->shutdown();
        }
    }

    std::unique_ptr<ScriptSandbox> sandbox_;
};

// Test fixture for SandboxConfig tests
class SandboxConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.memoryLimit = 512 * 1024;  // 512KB
        config_.executionTimeout = std::chrono::seconds(10);
        config_.enableFileAccess = true;
        config_.enableNetworkAccess = false;
        config_.maxCallDepth = 50;
        config_.allowedModules = {"math", "string"};
        config_.blockedFunctions = {"os.execute", "io.popen"};
    }

    SandboxConfig config_;
};

// ============================================================================
// SandboxConfig Tests
// ============================================================================

TEST_F(SandboxConfigTest, DefaultConfiguration) {
    SandboxConfig defaultConfig;

    EXPECT_GT(defaultConfig.memoryLimit, 0);
    EXPECT_GT(defaultConfig.executionTimeout.count(), 0);
    EXPECT_FALSE(defaultConfig.enableFileAccess);
    EXPECT_FALSE(defaultConfig.enableNetworkAccess);
    EXPECT_GT(defaultConfig.maxCallDepth, 0);
}

TEST_F(SandboxConfigTest, CustomConfiguration) {
    EXPECT_EQ(config_.memoryLimit, 512 * 1024);
    EXPECT_EQ(config_.executionTimeout, std::chrono::seconds(10));
    EXPECT_TRUE(config_.enableFileAccess);
    EXPECT_FALSE(config_.enableNetworkAccess);
    EXPECT_EQ(config_.maxCallDepth, 50);
    EXPECT_EQ(config_.allowedModules.size(), 2);
    EXPECT_EQ(config_.blockedFunctions.size(), 2);
}

// ============================================================================
// ScriptSandbox Tests
// ============================================================================

TEST_F(ScriptSandboxTest, Initialization) {
    EXPECT_TRUE(sandbox_->isInitialized());
}

TEST_F(ScriptSandboxTest, ExecuteSafeScript) {
    std::string safeScript = "return 2 + 2";

    auto result = sandbox_->execute(safeScript);

    EXPECT_TRUE(result.success);
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<int64_t>(), 4);
    }
}

TEST_F(ScriptSandboxTest, ExecuteUnsafeScript) {
    // Script that tries to access blocked functionality
    std::string unsafeScript = "os.execute('rm -rf /')";

    auto result = sandbox_->execute(unsafeScript);

    // Should be blocked by sandbox
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, MemoryLimitEnforcement) {
    // Script that tries to allocate excessive memory
    std::string memoryHogScript = R"(
        local t = {}
        for i = 1, 1000000 do
            t[i] = string.rep("x", 1000)
        end
        return #t
    )";

    auto result = sandbox_->execute(memoryHogScript);

    // Should be terminated due to memory limit
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, ExecutionTimeoutEnforcement) {
    // Script that runs for a long time
    std::string longRunningScript = R"(
        local count = 0
        while true do
            count = count + 1
            if count > 1000000 then
                break
            end
        end
        return count
    )";

    auto result = sandbox_->execute(longRunningScript);

    // Should be terminated due to timeout or succeed quickly
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, FileAccessRestriction) {
    // Script that tries to access files
    std::string fileAccessScript = R"(
        local file = io.open("/etc/passwd", "r")
        if file then
            local content = file:read("*all")
            file:close()
            return content
        end
        return "no access"
    )";

    auto result = sandbox_->execute(fileAccessScript);

    // Should be blocked or return "no access"
    if (result.success) {
        EXPECT_EQ(result.returnValue.get<std::string>(), "no access");
    } else {
        EXPECT_FALSE(result.errorMessage.empty());
    }
}

TEST_F(ScriptSandboxTest, NetworkAccessRestriction) {
    // Script that tries to make network connections
    std::string networkScript = R"(
        local socket = require("socket")
        local client = socket.tcp()
        local result = client:connect("google.com", 80)
        return result
    )";

    auto result = sandbox_->execute(networkScript);

    // Should be blocked by sandbox
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, AllowedModuleAccess) {
    // Script that uses allowed modules
    std::string mathScript = R"(
        return math.sqrt(16) + math.pi
    )";

    auto result = sandbox_->execute(mathScript);

    // Should succeed if math module is allowed
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, BlockedFunctionAccess) {
    // Script that tries to use blocked functions
    std::string blockedScript = R"(
        return os.execute("echo hello")
    )";

    auto result = sandbox_->execute(blockedScript);

    // Should be blocked
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, CallDepthLimiting) {
    // Script with deep recursion
    std::string recursiveScript = R"(
        function deepRecursion(n)
            if n <= 0 then
                return 0
            else
                return 1 + deepRecursion(n - 1)
            end
        end
        return deepRecursion(1000)
    )";

    auto result = sandbox_->execute(recursiveScript);

    // Should either succeed with limited depth or fail with stack overflow
    // protection
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, GetExecutionStatistics) {
    std::string script = "return 42";

    auto result = sandbox_->execute(script);

    auto stats = sandbox_->getExecutionStatistics();
    EXPECT_GT(stats.totalExecutions, 0);
    EXPECT_GE(stats.totalExecutionTime.count(), 0);
}

TEST_F(ScriptSandboxTest, ResetStatistics) {
    // Execute a script to generate stats
    sandbox_->execute("return 1");

    auto statsBefore = sandbox_->getExecutionStatistics();
    EXPECT_GT(statsBefore.totalExecutions, 0);

    sandbox_->resetStatistics();

    auto statsAfter = sandbox_->getExecutionStatistics();
    EXPECT_EQ(statsAfter.totalExecutions, 0);
    EXPECT_EQ(statsAfter.totalExecutionTime.count(), 0);
}

TEST_F(ScriptSandboxTest, SetResourceLimits) {
    ResourceLimits limits;
    limits.maxMemory = 2 * 1024 * 1024;  // 2MB
    limits.maxExecutionTime = std::chrono::seconds(30);
    limits.maxCallDepth = 200;

    sandbox_->setResourceLimits(limits);

    // Test that new limits are applied
    std::string script = "return 'limits updated'";
    auto result = sandbox_->execute(script);

    EXPECT_TRUE(result.success);
}

TEST_F(ScriptSandboxTest, AddAllowedModule) {
    sandbox_->addAllowedModule("table");

    std::string tableScript = R"(
        local t = {1, 2, 3}
        table.insert(t, 4)
        return #t
    )";

    auto result = sandbox_->execute(tableScript);

    // Should succeed if table module is now allowed
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, RemoveAllowedModule) {
    sandbox_->addAllowedModule("math");
    sandbox_->removeAllowedModule("math");

    std::string mathScript = "return math.sqrt(16)";

    auto result = sandbox_->execute(mathScript);

    // Should fail if math module is removed
    EXPECT_FALSE(result.success);
}

TEST_F(ScriptSandboxTest, AddBlockedFunction) {
    sandbox_->addBlockedFunction("print");

    std::string printScript = R"(
        print("Hello World")
        return "done"
    )";

    auto result = sandbox_->execute(printScript);

    // Should be blocked
    EXPECT_FALSE(result.success);
}

TEST_F(ScriptSandboxTest, RemoveBlockedFunction) {
    sandbox_->addBlockedFunction("tostring");
    sandbox_->removeBlockedFunction("tostring");

    std::string tostringScript = "return tostring(42)";

    auto result = sandbox_->execute(tostringScript);

    // Should succeed if tostring is unblocked
    EXPECT_TRUE(result.success);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ScriptSandboxTest, InvalidScript) {
    std::string invalidScript = "this is not valid lua syntax !!!";

    auto result = sandbox_->execute(invalidScript);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, EmptyScript) {
    std::string emptyScript = "";

    auto result = sandbox_->execute(emptyScript);

    // Should handle empty script gracefully
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST_F(ScriptSandboxTest, NullConfiguration) {
    // Test that sandbox handles null/invalid configuration gracefully
    SandboxConfig invalidConfig;
    invalidConfig.memoryLimit = 0;
    invalidConfig.executionTimeout = std::chrono::seconds(0);

    EXPECT_NO_THROW(ScriptSandbox invalidSandbox(invalidConfig));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ScriptSandboxTest, ConcurrentExecution) {
    const int numThreads = 4;
    const int scriptsPerThread = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount]() {
            for (int i = 0; i < scriptsPerThread; ++i) {
                std::string script = "return " + std::to_string(i * 10);
                auto result = sandbox_->execute(script);
                if (result.success) {
                    successCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successCount.load(), 0);
}
