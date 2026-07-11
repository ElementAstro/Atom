#include "atom/components/scripting/script_sandbox.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>

using namespace atom::components::scripting;

// ============================================================================
// Permission bit operations
// ============================================================================

TEST(SandboxPermissionTest, BitwiseOperators) {
    Permission combined = Permission::ReadFiles | Permission::WriteFiles;
    EXPECT_TRUE(hasPermission(combined, Permission::ReadFiles));
    EXPECT_TRUE(hasPermission(combined, Permission::WriteFiles));
    EXPECT_FALSE(hasPermission(combined, Permission::NetworkAccess));
}

TEST(SandboxPermissionTest, AllAndNone) {
    EXPECT_TRUE(hasPermission(Permission::All, Permission::RegistryAccess));
    EXPECT_TRUE(hasPermission(Permission::All, Permission::ThreadAccess));
    EXPECT_FALSE(hasPermission(Permission::None, Permission::ReadFiles));
}

TEST(SandboxPermissionTest, RequiredSubsetSemantics) {
    Permission granted = Permission::ReadFiles | Permission::NetworkAccess;
    Permission required = Permission::ReadFiles | Permission::WriteFiles;
    // Both required bits must be present.
    EXPECT_FALSE(hasPermission(granted, required));
}

// ============================================================================
// ResourceLimits / SandboxConfig defaults
// ============================================================================

TEST(SandboxConfigTest, ResourceLimitDefaults) {
    ResourceLimits limits;
    EXPECT_EQ(limits.maxMemoryUsage, 64u * 1024 * 1024);
    EXPECT_EQ(limits.maxExecutionTime, std::chrono::milliseconds(30000));
    EXPECT_EQ(limits.maxStackDepth, 1000u);
    EXPECT_EQ(limits.maxOpenFiles, 100u);
    EXPECT_EQ(limits.maxThreads, 4u);
    EXPECT_DOUBLE_EQ(limits.maxCpuUsage, 0.8);
}

TEST(SandboxConfigTest, ConfigDefaults) {
    SandboxConfig config;
    EXPECT_TRUE(hasPermission(config.permissions, Permission::ComponentAccess));
    EXPECT_FALSE(hasPermission(config.permissions, Permission::NetworkAccess));
    EXPECT_TRUE(config.enableLogging);
    EXPECT_FALSE(config.enableProfiling);
    EXPECT_TRUE(config.strictMode);
    EXPECT_TRUE(config.allowedPaths.empty());
    EXPECT_TRUE(config.blockedFunctions.empty());
}

// ============================================================================
// ScriptSandbox lifecycle
// ============================================================================

class ScriptSandboxTest : public ::testing::Test {
protected:
    void SetUp() override { sandbox_ = std::make_unique<ScriptSandbox>(); }
    void TearDown() override { sandbox_->shutdown(); }

    std::unique_ptr<ScriptSandbox> sandbox_;
};

TEST_F(ScriptSandboxTest, InitializeSucceeds) {
    EXPECT_TRUE(sandbox_->initialize());
}

TEST_F(ScriptSandboxTest, ExecuteWithoutInitializeFails) {
    auto result = sandbox_->executeInSandbox("return 1", "uninitialized");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, "Sandbox not initialized");
}

TEST_F(ScriptSandboxTest, StatisticsStartAtZero) {
    const auto& stats = sandbox_->getStatistics();
    EXPECT_EQ(stats.totalExecutions, 0u);
    EXPECT_EQ(stats.successfulExecutions, 0u);
    EXPECT_EQ(stats.violationsDetected, 0u);
    EXPECT_EQ(stats.scriptsTerminated, 0u);
}

TEST_F(ScriptSandboxTest, ResetStatistics) {
    sandbox_->resetStatistics();
    EXPECT_EQ(sandbox_->getStatistics().totalExecutions, 0u);
}

TEST_F(ScriptSandboxTest, NoViolationsInitially) {
    EXPECT_TRUE(sandbox_->getRecentViolations().empty());
}

TEST_F(ScriptSandboxTest, UpdateConfigDoesNotThrow) {
    SandboxConfig config;
    config.permissions = Permission::None;
    config.strictMode = false;
    EXPECT_NO_THROW(sandbox_->updateConfig(config));
}

TEST_F(ScriptSandboxTest, TerminateUnknownScriptFails) {
    ASSERT_TRUE(sandbox_->initialize());
    EXPECT_FALSE(sandbox_->terminateScript("no_such_script"));
}

TEST_F(ScriptSandboxTest, ExecuteAfterInitializeDoesNotThrow) {
    ASSERT_TRUE(sandbox_->initialize());
    // No real script engines are compiled in this configuration; the sandbox
    // must fail gracefully instead of crashing.
    ScriptResult result;
    EXPECT_NO_THROW(result = sandbox_->executeInSandbox("return 1", "smoke"));
    if (!result.success) {
        EXPECT_FALSE(result.errorMessage.empty());
    }
}
