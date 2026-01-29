/*
 * script_sandbox_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Script Sandbox Security ExampleDemonstrates security features,
resource limits, permission system, and safe script execution with the component
system.

**************************************************/

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/core/registry.hpp"
#include "atom/components/scripting/script_sandbox.hpp"
#include "atom/components/scripting/scripting_api.hpp"

// Conditional includes for scripting engines
#if ATOM_ENABLE_LUA
#include "atom/components/scripting/lua_engine.hpp"
#endif

#if ATOM_ENABLE_PYTHON
#include "atom/components/scripting/python_engine.hpp"
#endif

// Note: Component and Registry are in global namespace, not
// atom::componentsusing namespace atom::components::scripting;

/**
 * @brief Secure component that demonstrates sandbox protection
 */
class SecureComponent : public Component {
public:
    explicit SecureComponent(const std::string& name) : Component(name) {
        std::cout << "SecureComponent '" << name << "' created" << std::endl;

        // Add variables with different security levels
        addVariable<std::string>("public_data", "This is public information");
        addVariable<std::string>("private_data", "This is private information");
        addVariable<int>("security_level", 1);
        addVariable<bool>("access_granted", false);

        // Public commands (always accessible)
        def("getPublicData", [this]() -> std::string {
            auto data = getVariable<std::string>("public_data");
            return data ? data->get() : "No data";
        });

        def("getSecurityLevel", [this]() -> int {
            auto level = getVariable<int>("security_level");
            return level ? level->get() : 0;
        });

        // Protected commands (require permission)
        def("getPrivateData", [this]() -> std::string {
            auto granted = getVariable<bool>("access_granted");
            if (granted && granted->get()) {
                auto data = getVariable<std::string>("private_data");
                return data ? data->get() : "No private data";
            } else {
                throw std::runtime_error(
                    "Access denied: insufficient permissions");
            }
        });

        def("setSecurityLevel", [this](int level) {
            auto granted = getVariable<bool>("access_granted");
            if (granted && granted->get()) {
                setValue("security_level", level);
                std::cout << "  [" << getName()
                          << "] Security level set to: " << level << std::endl;
            } else {
                throw std::runtime_error(
                    "Access denied: cannot modify security level");
            }
        });

        def("grantAccess", [this](const std::string& password) -> bool {
            if (password == "secure123") {
                setValue("access_granted", true);
                std::cout << "  [" << getName() << "] Access granted"
                          << std::endl;
                return true;
            } else {
                std::cout << "  [" << getName()
                          << "] Access denied: invalid password" << std::endl;
                return false;
            }
        });

        def("revokeAccess", [this]() {
            setValue("access_granted", false);
            std::cout << "  [" << getName() << "] Access revoked" << std::endl;
        });

        // Dangerous command (should be blocked in sandbox)
        def("dangerousOperation", [this]() -> std::string {
            std::cout << "  [" << getName()
                      << "] WARNING: Executing dangerous operation!"
                      << std::endl;
            return "Dangerous operation completed";
        });

        // Resource-intensive command
        def("intensiveComputation", [this](int iterations) -> double {
            std::cout << "  [" << getName()
                      << "] Starting intensive computation..." << std::endl;
            double result = 0.0;
            for (int i = 0; i < iterations; ++i) {
                result += std::sin(i) * std::cos(i);
                if (i % 10000 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            std::cout << "  [" << getName() << "] Computation completed"
                      << std::endl;
            return result;
        });
    }
};

void demonstrateBasicSandbox() {
    std::cout << "\n=== Basic Sandbox Demo ===" << std::endl;

    std::cout << "\n1. Creating sandbox environment..." << std::endl;

    // Create sandbox with restrictive settings
    ResourceLimits limits;
    limits.maxMemoryUsage = 16 * 1024 * 1024;                   // 16MB
    limits.maxExecutionTime = std::chrono::milliseconds(5000);  // 5 seconds
    limits.maxStackDepth = 100;
    limits.maxFileSize = 1024 * 1024;  // 1MB
    limits.maxOpenFiles = 10;
    limits.maxNetworkConnections = 0;  // No network access
    limits.maxThreads = 1;
    limits.maxCpuUsage = 0.5;  // 50% CPU

    Permission permissions =
        Permission::ComponentAccess | Permission::ReadFiles;

    SandboxConfig config;
    config.limits = limits;
    config.permissions = permissions;
    auto sandbox = std::make_unique<ScriptSandbox>(config);

    std::cout << "Sandbox created with restrictive limits:" << std::endl;
    std::cout << "  Memory limit: " << (limits.maxMemoryUsage / 1024 / 1024)
              << " MB" << std::endl;
    std::cout << "  Time limit: " << limits.maxExecutionTime.count() << " ms"
              << std::endl;
    std::cout << "  No network access" << std::endl;
    std::cout << "  Component access allowed" << std::endl;
    std::cout << "  File read access allowed" << std::endl;
}

void demonstratePermissionSystem() {
    std::cout << "\n=== Permission System Demo ===" << std::endl;

    auto& registry = ::Registry::instance();
    auto component = registry.createComponent<SecureComponent>("SecureComp");

    std::cout << "\n2. Testing permission-based access control..." << std::endl;

    // Test public access (should work)
    std::cout << "\n--- Testing public access ---" << std::endl;
    try {
        auto publicData = component->runCommand("getPublicData", {});
        std::cout << "Public data: " << std::any_cast<std::string>(publicData)
                  << std::endl;

        auto securityLevel = component->runCommand("getSecurityLevel", {});
        std::cout << "Security level: " << std::any_cast<int>(securityLevel)
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error accessing public data: " << e.what() << std::endl;
    }

    // Test private access without permission (should fail)
    std::cout << "\n--- Testing private access without permission ---"
              << std::endl;
    try {
        auto privateData = component->runCommand("getPrivateData", {});
        std::cout << "Unexpected success: "
                  << std::any_cast<std::string>(privateData) << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }

    // Grant access and try again
    std::cout << "\n--- Granting access with correct password ---" << std::endl;
    try {
        std::vector<std::any> grantArgs = {std::any(std::string("secure123"))};
        auto granted = component->runCommand("grantAccess", grantArgs);
        std::cout << "Access granted: " << std::any_cast<bool>(granted)
                  << std::endl;

        auto privateData = component->runCommand("getPrivateData", {});
        std::cout << "Private data: " << std::any_cast<std::string>(privateData)
                  << std::endl;

        std::vector<std::any> levelArgs = {std::any(std::string("5"))};
        [[maybe_unused]] auto setResult =
            component->runCommand("setSecurityLevel", levelArgs);
        auto newLevel = component->runCommand("getSecurityLevel", {});
        std::cout << "New security level: " << std::any_cast<int>(newLevel)
                  << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    // Test wrong password
    std::cout << "\n--- Testing wrong password ---" << std::endl;
    [[maybe_unused]] auto revokeResult =
        component->runCommand("revokeAccess", {});
    try {
        std::vector<std::any> wrongArgs = {
            std::any(std::string("wrongpassword"))};
        auto granted = component->runCommand("grantAccess", wrongArgs);
        std::cout << "Access granted: " << std::any_cast<bool>(granted)
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void demonstrateResourceLimits() {
    std::cout << "\n=== Resource Limits Demo ===" << std::endl;

    auto& registry = ::Registry::instance();
    auto component = registry.getComponent("SecureComp");

    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n3. Testing resource limits..." << std::endl;

    // Test memory limits with large data structures
    std::cout << "\n--- Testing memory limits ---" << std::endl;

#if ATOM_ENABLE_LUA || ATOM_ENABLE_PYTHON
    // Create a sandbox with very low memory limit
    ResourceLimits strictLimits;
    strictLimits.maxMemoryUsage = 1024 * 1024;  // 1MB
    strictLimits.maxExecutionTime = std::chrono::milliseconds(1000);

    Permission permissions = Permission::ComponentAccess;
    auto sandbox = std::make_unique<ScriptSandbox>(strictLimits, permissions);

    // Test script that tries to allocate too much memory
    std::string memoryScript;
#if ATOM_ENABLE_LUA
    memoryScript = R"(
        local bigTable = {}
        for i = 1, 100000 do
            bigTable[i] = string.rep("x", 1000)
        end
        return #bigTable
    )";
#elif ATOM_ENABLE_PYTHON
    memoryScript = R"(
big_list = []
for i in range(100000):
    big_list.append("x" * 1000)
len(big_list)
    )";
#endif

    if (!memoryScript.empty()) {
        std::cout << "Attempting to allocate large amount of memory..."
                  << std::endl;

        try {
            auto result = sandbox->executeScript(memoryScript, "memory_test");
            if (result.success) {
                std::cout << "Memory test result: " << result.output
                          << std::endl;
            } else {
                std::cout << "Expected memory limit violation: "
                          << result.errorMessage << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Expected exception: " << e.what() << std::endl;
        }
    }
#endif

    // Test execution time limits
    std::cout << "\n--- Testing execution time limits ---" << std::endl;

    try {
        auto start = std::chrono::high_resolution_clock::now();

        // This should be interrupted by time limit
        std::vector<std::any> computeArgs = {std::any(std::string("1000000"))};
        auto result =
            component->runCommand("intensiveComputation", computeArgs);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Computation result: "
                  << std::any_cast<std::string>(result) << std::endl;
        std::cout << "Execution time: " << duration.count() << " ms"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Time limit exception: " << e.what() << std::endl;
    }
}

void demonstrateSandboxViolations() {
    std::cout << "\n=== Sandbox Violations Demo ===" << std::endl;

    std::cout << "\n4. Testing sandbox violation detection..." << std::endl;

    // Create sandbox with violation monitoring
    ResourceLimits limits;
    limits.maxMemoryUsage = 8 * 1024 * 1024;  // 8MB
    limits.maxExecutionTime = std::chrono::milliseconds(2000);
    limits.maxStackDepth = 50;

    Permission permissions = Permission::ComponentAccess;
    SandboxConfig config;
    config.limits = limits;
    config.permissions = permissions;
    auto sandbox = std::make_unique<ScriptSandbox>(config);

    // Note: setViolationHandler is not available in current API
    // Violations will be tracked automatically and can be retrieved via
    // getRecentViolations

    // Test various violations
    std::vector<std::pair<std::string, std::string>> violationTests = {
        {"Stack Overflow",
         "function recursive() recursive() end recursive()"},  // Lua
        {"Infinite Loop", "while true do end"},                // Lua
        {"Permission Violation", "os.execute('ls')"}  // Lua - OS access
    };

#if ATOM_ENABLE_LUA || ATOM_ENABLE_PYTHON
    for (const auto& test : violationTests) {
        std::cout << "\n--- Testing: " << test.first << " ---" << std::endl;

        try {
            auto result = sandbox->executeScript(test.second, test.first);
            if (result.success) {
                std::cout << "Unexpected success: " << result.output
                          << std::endl;
            } else {
                std::cout << "Expected failure: " << result.errorMessage
                          << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Expected exception: " << e.what() << std::endl;
        }
    }
#endif

    // Get violation history
    std::cout << "\n--- Violation History ---" << std::endl;
    auto violations = sandbox->getRecentViolations();
    std::cout << "Total violations detected: " << violations.size()
              << std::endl;

    for (size_t i = 0; i < std::min(size_t(3), violations.size()); ++i) {
        const auto& v = violations[i];
        std::cout << "  " << (i + 1) << ". " << v.description << " in "
                  << v.scriptName << std::endl;
    }
}

void demonstrateSandboxConfiguration() {
    std::cout << "\n=== Sandbox Configuration Demo ===" << std::endl;

    std::cout << "\n5. Testing different sandbox configurations..."
              << std::endl;

    // Permissive sandbox
    std::cout << "\n--- Permissive Sandbox ---" << std::endl;
    ResourceLimits permissiveLimits;
    permissiveLimits.maxMemoryUsage = 64 * 1024 * 1024;  // 64MB
    permissiveLimits.maxExecutionTime =
        std::chrono::milliseconds(10000);  // 10 seconds

    Permission permissivePerms = Permission::ComponentAccess |
                                 Permission::ReadFiles | Permission::SystemInfo;

    SandboxConfig permissiveConfig;
    permissiveConfig.limits = permissiveLimits;
    permissiveConfig.permissions = permissivePerms;
    auto permissiveSandbox = std::make_unique<ScriptSandbox>(permissiveConfig);

    std::cout << "Permissive sandbox allows:" << std::endl;
    std::cout << "  Component access: "
              << hasPermission(permissivePerms, Permission::ComponentAccess)
              << std::endl;
    std::cout << "  File reading: "
              << hasPermission(permissivePerms, Permission::ReadFiles)
              << std::endl;
    std::cout << "  System info: "
              << hasPermission(permissivePerms, Permission::SystemInfo)
              << std::endl;
    std::cout << "  Network access: "
              << hasPermission(permissivePerms, Permission::NetworkAccess)
              << std::endl;

    // Restrictive sandbox
    std::cout << "\n--- Restrictive Sandbox ---" << std::endl;
    ResourceLimits restrictiveLimits;
    restrictiveLimits.maxMemoryUsage = 4 * 1024 * 1024;  // 4MB
    restrictiveLimits.maxExecutionTime =
        std::chrono::milliseconds(1000);  // 1 second
    restrictiveLimits.maxStackDepth = 20;

    Permission restrictivePerms = Permission::None;

    SandboxConfig restrictiveConfig;
    restrictiveConfig.limits = restrictiveLimits;
    restrictiveConfig.permissions = restrictivePerms;
    auto restrictiveSandbox =
        std::make_unique<ScriptSandbox>(restrictiveConfig);

    std::cout << "Restrictive sandbox allows:" << std::endl;
    std::cout << "  Component access: "
              << hasPermission(restrictivePerms, Permission::ComponentAccess)
              << std::endl;
    std::cout << "  File reading: "
              << hasPermission(restrictivePerms, Permission::ReadFiles)
              << std::endl;
    std::cout << "  System info: "
              << hasPermission(restrictivePerms, Permission::SystemInfo)
              << std::endl;
    std::cout << "  Network access: "
              << hasPermission(restrictivePerms, Permission::NetworkAccess)
              << std::endl;

    // Development sandbox
    std::cout << "\n--- Development Sandbox ---" << std::endl;
    ResourceLimits devLimits;
    devLimits.maxMemoryUsage = 32 * 1024 * 1024;                   // 32MB
    devLimits.maxExecutionTime = std::chrono::milliseconds(5000);  // 5 seconds

    Permission devPerms = Permission::ComponentAccess | Permission::ReadFiles |
                          Permission::WriteFiles | Permission::SystemInfo;

    SandboxConfig devConfig;
    devConfig.limits = devLimits;
    devConfig.permissions = devPerms;
    auto devSandbox = std::make_unique<ScriptSandbox>(devConfig);

    std::cout << "Development sandbox allows:" << std::endl;
    std::cout << "  Component access: "
              << hasPermission(devPerms, Permission::ComponentAccess)
              << std::endl;
    std::cout << "  File reading: "
              << hasPermission(devPerms, Permission::ReadFiles) << std::endl;
    std::cout << "  File writing: "
              << hasPermission(devPerms, Permission::WriteFiles) << std::endl;
    std::cout << "  System info: "
              << hasPermission(devPerms, Permission::SystemInfo) << std::endl;
    std::cout << "  Execute commands: "
              << hasPermission(devPerms, Permission::ExecuteCommands)
              << std::endl;
}

void demonstrateSandboxStatistics() {
    std::cout << "\n=== Sandbox Statistics Demo ===" << std::endl;

    std::cout << "\n6. Sandbox performance and security statistics..."
              << std::endl;

    ResourceLimits limits;
    limits.maxMemoryUsage = 16 * 1024 * 1024;
    limits.maxExecutionTime = std::chrono::milliseconds(3000);

    Permission permissions = Permission::ComponentAccess;
    SandboxConfig config;
    config.limits = limits;
    config.permissions = permissions;
    auto sandbox = std::make_unique<ScriptSandbox>(config);

#if ATOM_ENABLE_LUA || ATOM_ENABLE_PYTHON
    // Execute several test scripts
    std::vector<std::string> testScripts = {
        "return 2 + 2", "return 'Hello World'",
        "local sum = 0; for i = 1, 100 do sum = sum + i end; return sum",
        "return math.sqrt(16)", "return string.upper('test')"};

    for (size_t i = 0; i < testScripts.size(); ++i) {
        sandbox->executeScript(testScripts[i], "test_" + std::to_string(i));
    }
#endif

    // Get and display statistics
    auto stats = sandbox->getStatistics();
    std::cout << "Sandbox Statistics:" << std::endl;
    std::cout << "  Total executions: " << stats.totalExecutions << std::endl;
    std::cout << "  Successful executions: " << stats.successfulExecutions
              << std::endl;
    std::cout << "  Scripts terminated: " << stats.scriptsTerminated
              << std::endl;
    std::cout << "  Violations detected: " << stats.violationsDetected
              << std::endl;
    std::cout << "  Total execution time: " << stats.totalExecutionTime.count()
              << " microseconds" << std::endl;
}

int main() {
    std::cout << "=== Atom Component Script Sandbox Security Examples ==="
              << std::endl;

    try {
        demonstrateBasicSandbox();
        demonstratePermissionSystem();
        demonstrateResourceLimits();
        demonstrateSandboxViolations();
        demonstrateSandboxConfiguration();
        demonstrateSandboxStatistics();

        std::cout
            << "\n=== All Script Sandbox Examples Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in script sandbox examples: " << e.what()
                  << std::endl;
        return 1;
    }

    return 0;
}
