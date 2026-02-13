/*
 * env_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Example usage of optimized environment components

**************************************************/

#include "env_advanced.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

using namespace atom::utils;

void demonstrateBasicFeatures() {
    std::cout << "\n=== Basic Environment Features ===\n";

    // Set and get environment variables
    EnvCore::setEnv("DEMO_VAR", "Hello World");
    std::cout << "DEMO_VAR = " << EnvCore::getEnv("DEMO_VAR", "not found") << "\n";

    // Batch operations
    HashMap<String, String> vars = {
        {"VAR1", "value1"},
        {"VAR2", "value2"},
        {"VAR3", "value3"}
    };
    size_t setCount = EnvCore::setBatch(vars);
    std::cout << "Set " << setCount << " variables in batch\n";

    // Get batch
    Vector<String> keys = {"VAR1", "VAR2", "VAR3", "NONEXISTENT"};
    auto retrieved = EnvCore::getBatch(keys);
    std::cout << "Retrieved " << retrieved.size() << " variables\n";
}

void demonstratePathManagement() {
    std::cout << "\n=== PATH Management ===\n";

    // Add paths
    auto result1 = EnvPath::addToPath("/usr/local/custom/bin");
    auto result2 = EnvPath::addToPath("/opt/tools/bin", true); // prepend

    std::cout << "Added paths: " << (result1 == PathOperationResult::SUCCESS ? "success" : "failed")
              << ", " << (result2 == PathOperationResult::SUCCESS ? "success" : "failed") << "\n";

    // Get PATH statistics
    auto pathStats = EnvPath::getPathStats();
    std::cout << "PATH entries: " << pathStats["total_entries"] << " total, "
              << pathStats["valid_paths"] << " valid\n";

    // Find executables
    auto bashPaths = EnvPath::findExecutable("bash");
    std::cout << "Found bash in " << bashPaths.size() << " locations\n";

    // Clean up PATH
    size_t cleaned = EnvPath::cleanupPath();
    std::cout << "Cleaned " << cleaned << " invalid PATH entries\n";
}

void demonstrateFileOperations() {
    std::cout << "\n=== File I/O Operations ===\n";

    HashMap<String, String> testVars = {
        {"APP_NAME", "MyApplication"},
        {"APP_VERSION", "1.0.0"},
        {"APP_DEBUG", "true"}
    };

    // Save to different formats
    EnvFileIO::saveToFile("test.env", testVars, EnvFileFormat::DOTENV);
    EnvFileIO::saveToFile("test.json", testVars, EnvFileFormat::JSON);
    EnvFileIO::saveToFile("test.sh", testVars, EnvFileFormat::SHELL);

    std::cout << "Saved environment to multiple formats\n";

    // Validate files
    bool valid = EnvFileIO::validateFile("test.env");
    std::cout << "test.env is " << (valid ? "valid" : "invalid") << "\n";

    // Merge files
    std::vector<std::filesystem::path> files = {"test.env", "test.json"};
    EnvFileIO::mergeFiles(files, "merged.env");
    std::cout << "Merged multiple environment files\n";
}

void demonstrateScopedEnvironment() {
    std::cout << "\n=== Scoped Environment ===\n";

    std::cout << "Original PATH: " << EnvCore::getEnv("PATH", "").substr(0, 50) << "...\n";

    {
        // Create scoped environment
        ScopedEnv scopedVar("TEMP_VAR", "temporary_value");

        // Create batch scoped environment
        HashMap<String, String> scopedVars = {
            {"SCOPED_VAR1", "scoped1"},
            {"SCOPED_VAR2", "scoped2"}
        };
        auto batch = EnvScoped::createScopedEnvBatch(scopedVars);

        std::cout << "Inside scope - TEMP_VAR: " << EnvCore::getEnv("TEMP_VAR", "not found") << "\n";
        std::cout << "Inside scope - SCOPED_VAR1: " << EnvCore::getEnv("SCOPED_VAR1", "not found") << "\n";

        // Variables will be automatically restored when scope ends
    }

    std::cout << "Outside scope - TEMP_VAR: " << EnvCore::getEnv("TEMP_VAR", "not found") << "\n";
}

void demonstrateSystemInformation() {
    std::cout << "\n=== System Information ===\n";

    auto sysInfo = EnvSystem::getSystemInfo();
    std::cout << "OS: " << sysInfo["os"] << "\n";
    std::cout << "Architecture: " << sysInfo["architecture"] << "\n";
    std::cout << "Hostname: " << sysInfo["hostname"] << "\n";
    std::cout << "Username: " << sysInfo["username"] << "\n";

    // Memory information
    auto memInfo = EnvSystem::getMemoryInfo();
    if (!memInfo.empty()) {
        std::cout << "Total Memory: " << (memInfo["total"] / (1024 * 1024)) << " MB\n";
        std::cout << "Available Memory: " << (memInfo["available"] / (1024 * 1024)) << " MB\n";
    }

    // CPU information
    auto cpuInfo = EnvSystem::getCpuInfo();
    std::cout << "CPU Cores: " << cpuInfo["cores"] << "\n";

    // Disk space
    auto diskInfo = EnvSystem::getDiskSpaceInfo(".");
    if (!diskInfo.empty()) {
        std::cout << "Disk Free: " << (diskInfo["free"] / (1024 * 1024 * 1024)) << " GB\n";
    }
}

void demonstrateUtilities() {
    std::cout << "\n=== Utility Functions ===\n";

    // Variable expansion
    EnvCore::setEnv("BASE_DIR", "/usr/local");
    EnvCore::setEnv("APP_DIR", "${BASE_DIR}/myapp");

    String expanded = EnvUtils::expandVariables("${APP_DIR}/bin");
    std::cout << "Expanded: " << expanded << "\n";

    // Variable validation
    HashMap<String, String> testVars = {
        {"VALID_VAR", "valid_value"},
        {"123INVALID", "value"},  // Invalid: starts with digit
        {"VALID_VAR2", "another_value"}
    };

    auto validationResults = EnvUtils::validateEnvironmentVariables(testVars);
    for (const auto& [name, result] : validationResults) {
        std::cout << name << ": " << (result.isValid ? "valid" : "invalid");
        if (!result.isValid) {
            std::cout << " (" << result.errorMessage << ")";
        }
        std::cout << "\n";
    }

    // Variable analysis
    auto analysis = EnvUtils::analyzeVariableUsage(EnvCore::Environ());
    std::cout << "Environment analysis:\n";
    std::cout << "  Total variables: " << analysis["total_variables"] << "\n";
    std::cout << "  Empty variables: " << analysis["empty_variables"] << "\n";
    std::cout << "  Variables with references: " << analysis["variables_with_references"] << "\n";
}

void demonstrateAdvancedFeatures() {
    std::cout << "\n=== Advanced Features ===\n";

    // Create and apply profile
    auto profile = EnvAdvanced::createProfileFromCurrent("demo_profile", "Demonstration profile");
    std::cout << "Created profile with " << profile.variables.size() << " variables\n";

    // Save profile
    EnvAdvanced::saveProfile(profile, "demo_profile.json");
    std::cout << "Saved profile to demo_profile.json\n";

    // Perform health check
    auto healthCheck = EnvAdvanced::performHealthCheck();
    std::cout << "Health check status: " << healthCheck["health_status"] << "\n";
    std::cout << "Total variables: " << healthCheck["total_variables"] << "\n";

    // Optimize environment
    auto optimization = EnvAdvanced::optimizeEnvironment();
    std::cout << "Optimization completed: " << optimization["optimization_status"] << "\n";
    std::cout << "PATH entries cleaned: " << optimization["path_entries_removed"] << "\n";

    // Get comprehensive statistics
    auto stats = EnvAdvanced::getEnvironmentStatistics();
    std::cout << "Environment statistics:\n";
    std::cout << "  Core cache hits: " << stats["core_cache_hits"] << "\n";
    std::cout << "  Path cache hits: " << stats["path_cache_hits"] << "\n";
    std::cout << "  System cache hits: " << stats["system_cache_hits"] << "\n";
}

void demonstrateEncryption() {
    std::cout << "\n=== Encryption Features ===\n";

    // Set up encryption provider
    auto encProvider = std::make_shared<SimpleEncryptionProvider>("my_secret_key");
    EnvAdvanced::setEncryptionProvider(encProvider);

    // Set encrypted variable
    bool success = EnvAdvanced::setEncryptedVar("SECRET_TOKEN", "super_secret_value");
    std::cout << "Set encrypted variable: " << (success ? "success" : "failed") << "\n";

    // Get encrypted variable
    String decrypted = EnvAdvanced::getEncryptedVar("SECRET_TOKEN", "default");
    std::cout << "Decrypted value: " << decrypted << "\n";

    // Show raw encrypted value
    String raw = EnvCore::getEnv("SECRET_TOKEN", "");
    std::cout << "Raw encrypted value: " << raw.substr(0, 20) << "...\n";
}

int main() {
    // Set up logging
    spdlog::set_level(spdlog::level::info);

    std::cout << "=== Environment Management System Demo ===\n";
    std::cout << "Demonstrating optimized environment components\n";

    try {
        demonstrateBasicFeatures();
        demonstratePathManagement();
        demonstrateFileOperations();
        demonstrateScopedEnvironment();
        demonstrateSystemInformation();
        demonstrateUtilities();
        demonstrateAdvancedFeatures();
        demonstrateEncryption();

        std::cout << "\n=== Demo completed successfully! ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during demo: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
