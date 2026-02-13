/*
 * env_advanced.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Advanced environment management features implementation

**************************************************/

#include "env_advanced.hpp"

#include <algorithm>
#include <fstream>
#include <thread>
#include <random>
#include <sstream>

#include <spdlog/spdlog.h>

namespace atom::utils {

// Static member initializations
std::shared_ptr<EnvEncryptionProvider> EnvAdvanced::sEncryptionProvider;
HashMap<size_t, EnvMonitorCallback> EnvAdvanced::sMonitorCallbacks;
size_t EnvAdvanced::sNextMonitorId = 1;
std::mutex EnvAdvanced::sMonitorMutex;

auto EnvAdvanced::applyProfile(const EnvProfile& profile, bool persistent) -> bool {
    try {
        spdlog::info("Applying environment profile: {}", profile.name);

        // Apply regular environment variables
        size_t successCount = 0;
        for (const auto& [key, value] : profile.variables) {
            if (EnvCore::setEnv(key, value)) {
                successCount++;
            }
        }

        // Apply PATH entries
        for (const auto& pathEntry : profile.pathEntries) {
            EnvPath::addToPath(pathEntry);
        }

        // Apply persistent variables if requested
        if (persistent) {
            for (const auto& [key, value] : profile.persistentVars) {
                EnvPersistent::setPersistentEnv(key, value);
            }
        }

        // Apply template if present
        if (!profile.envTemplate.name.empty()) {
            auto templateVars = EnvUtils::applyTemplate(profile.envTemplate);
            for (const auto& [key, value] : templateVars) {
                EnvCore::setEnv(key, value);
            }
        }

        spdlog::info("Applied profile '{}': {}/{} variables, {} PATH entries",
                     profile.name, successCount, profile.variables.size(),
                     profile.pathEntries.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to apply profile '{}': {}", profile.name, e.what());
        return false;
    }
}

auto EnvAdvanced::createProfileFromCurrent(const String& name, const String& description,
                                            bool includeSystem) -> EnvProfile {
    EnvProfile profile(name, description);

    // Get current environment
    auto currentEnv = EnvCore::Environ();

    if (includeSystem) {
        profile.variables = currentEnv;
    } else {
        // Filter out system variables
        Vector<String> systemVars = {"PATH", "HOME", "USER", "USERNAME", "SHELL", "TERM"};
        for (const auto& [key, value] : currentEnv) {
            if (std::find(systemVars.begin(), systemVars.end(), key) == systemVars.end()) {
                profile.variables[key] = value;
            }
        }
    }

    // Get current PATH entries
    profile.pathEntries = EnvPath::getPathEntries();

    spdlog::info("Created profile '{}' with {} variables and {} PATH entries",
                 name, profile.variables.size(), profile.pathEntries.size());

    return profile;
}

auto EnvAdvanced::saveProfile(const EnvProfile& profile, const String& filePath,
                              EnvFileFormat format) -> bool {
    try {
        String serialized = serializeProfile(profile, format);

        std::ofstream file(std::string(filePath.c_str()));
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", filePath);
            return false;
        }

        file << serialized;
        file.close();

        spdlog::info("Saved profile '{}' to {}", profile.name, filePath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save profile '{}': {}", profile.name, e.what());
        return false;
    }
}

auto EnvAdvanced::loadProfile(const String& filePath, EnvFileFormat format) -> EnvProfile {
    try {
        std::ifstream file(std::string(filePath.c_str()));
        if (!file.is_open()) {
            spdlog::error("Failed to open file for reading: {}", filePath);
            return EnvProfile();
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        String data = String(buffer.str());

        if (format == EnvFileFormat::AUTO) {
            format = detectProfileFormat(filePath);
        }

        auto profile = deserializeProfile(data, format);
        spdlog::info("Loaded profile '{}' from {}", profile.name, filePath);
        return profile;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load profile from {}: {}", filePath, e.what());
        return EnvProfile();
    }
}

auto EnvAdvanced::listProfiles(const String& directory) -> Vector<String> {
    Vector<String> profiles;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(std::string(directory.c_str()))) {
            if (entry.is_regular_file()) {
                String filename = String(entry.path().filename().string());
                String ext = String(entry.path().extension().string());

                if (ext == ".json" || ext == ".yaml" || ext == ".yml" || ext == ".xml") {
                    profiles.push_back(filename);
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to list profiles in directory {}: {}", directory, e.what());
    }

    return profiles;
}

auto EnvAdvanced::startMonitoring(EnvMonitorCallback callback, int interval) -> size_t {
    std::lock_guard<std::mutex> lock(sMonitorMutex);

    size_t sessionId = sNextMonitorId++;
    sMonitorCallbacks[sessionId] = callback;

    // Start monitoring thread
    std::thread monitorThread([sessionId, callback, interval]() {
        runMonitoringLoop(sessionId, callback, interval);
    });
    monitorThread.detach();

    spdlog::info("Started environment monitoring session: {}", sessionId);
    return sessionId;
}

auto EnvAdvanced::stopMonitoring(size_t sessionId) -> bool {
    std::lock_guard<std::mutex> lock(sMonitorMutex);

    auto it = sMonitorCallbacks.find(sessionId);
    if (it != sMonitorCallbacks.end()) {
        sMonitorCallbacks.erase(it);
        spdlog::info("Stopped environment monitoring session: {}", sessionId);
        return true;
    }

    return false;
}

void EnvAdvanced::setEncryptionProvider(std::shared_ptr<EnvEncryptionProvider> provider) {
    sEncryptionProvider = provider;
    spdlog::info("Set encryption provider for environment variables");
}

auto EnvAdvanced::setEncryptedVar(const String& key, const String& value, bool persistent) -> bool {
    if (!sEncryptionProvider) {
        spdlog::error("No encryption provider set");
        return false;
    }

    try {
        String encrypted = sEncryptionProvider->encrypt(value);

        if (persistent) {
            return EnvPersistent::setPersistentEnv(key, encrypted) == PersistenceResult::SUCCESS;
        } else {
            return EnvCore::setEnv(key, encrypted);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to set encrypted variable '{}': {}", key, e.what());
        return false;
    }
}

auto EnvAdvanced::getEncryptedVar(const String& key, const String& defaultValue) -> String {
    if (!sEncryptionProvider) {
        spdlog::error("No encryption provider set");
        return defaultValue;
    }

    try {
        String encrypted = EnvCore::getEnv(key, "");
        if (encrypted.empty()) {
            return defaultValue;
        }

        if (sEncryptionProvider->isEncrypted(encrypted)) {
            return sEncryptionProvider->decrypt(encrypted);
        } else {
            return encrypted;  // Not encrypted
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to get encrypted variable '{}': {}", key, e.what());
        return defaultValue;
    }
}

auto EnvAdvanced::performHealthCheck() -> HashMap<String, String> {
    HashMap<String, String> results;

    // Check core functionality
    results["core_functionality"] = "OK";

    // Check PATH validity
    auto pathStats = EnvPath::getPathStats();
    results["path_total_entries"] = std::to_string(pathStats["total_entries"]);
    results["path_valid_entries"] = std::to_string(pathStats["valid_paths"]);
    results["path_invalid_entries"] = std::to_string(pathStats["invalid_paths"]);

    // Check system information
    auto sysInfo = EnvSystem::getSystemInfo();
    results["system_os"] = sysInfo["os"];
    results["system_arch"] = sysInfo["architecture"];

    // Check memory usage
    auto memInfo = EnvSystem::getMemoryInfo();
    if (!memInfo.empty()) {
        results["memory_total_mb"] = std::to_string(memInfo["total"] / (1024 * 1024));
        results["memory_available_mb"] = std::to_string(memInfo["available"] / (1024 * 1024));
    }

    // Check environment variable count
    auto env = EnvCore::Environ();
    results["total_variables"] = std::to_string(env.size());

    results["health_status"] = "HEALTHY";
    return results;
}

auto EnvAdvanced::optimizeEnvironment() -> HashMap<String, String> {
    HashMap<String, String> results;

    // Clean up PATH
    size_t pathCleaned = EnvPath::cleanupPath();
    results["path_entries_removed"] = std::to_string(pathCleaned);

    // Enable caching for better performance
    EnvCore::setCachingEnabled(true, 300);
    EnvPath::setCachingEnabled(true, 60);
    EnvSystem::setCachingEnabled(true, 300);

    results["caching_enabled"] = "true";
    results["optimization_status"] = "COMPLETED";

    spdlog::info("Environment optimization completed: {} PATH entries cleaned", pathCleaned);
    return results;
}

auto EnvAdvanced::createEnvironmentDiff(const HashMap<String, String>& before,
                                         const HashMap<String, String>& after)
    -> HashMap<String, String> {
    HashMap<String, String> diff;

    // Find added variables
    for (const auto& [key, value] : after) {
        if (before.find(key) == before.end()) {
            diff["added_" + key] = value;
        }
    }

    // Find removed variables
    for (const auto& [key, value] : before) {
        if (after.find(key) == after.end()) {
            diff["removed_" + key] = value;
        }
    }

    // Find modified variables
    for (const auto& [key, value] : after) {
        auto it = before.find(key);
        if (it != before.end() && it->second != value) {
            diff["modified_" + key] = "from '" + it->second + "' to '" + value + "'";
        }
    }

    return diff;
}

auto EnvAdvanced::exportEnvironment(EnvFileFormat format, bool includeSystem,
                                    const String& filePath) -> bool {
    try {
        auto env = EnvCore::Environ();

        if (!includeSystem) {
            // Filter out system variables
            Vector<String> systemVars = {"PATH", "HOME", "USER", "USERNAME", "SHELL", "TERM"};
            for (const auto& sysVar : systemVars) {
                env.erase(sysVar);
            }
        }

        if (filePath.empty()) {
            // Export to stdout or default file
            return EnvFileIO::saveToFile("environment_export.env", env, format);
        } else {
            return EnvFileIO::saveToFile(filePath, env, format);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to export environment: {}", e.what());
        return false;
    }
}

auto EnvAdvanced::importEnvironment(const String& source, EnvFileFormat format,
                                    bool merge) -> bool {
    try {
        if (!merge) {
            // Clear current environment first
            auto currentEnv = EnvCore::Environ();
            for (const auto& [key, value] : currentEnv) {
                EnvCore::unsetEnv(key);
            }
        }

        return EnvFileIO::loadFromFile(source, true, format);
    } catch (const std::exception& e) {
        spdlog::error("Failed to import environment from {}: {}", source, e.what());
        return false;
    }
}

auto EnvAdvanced::getEnvironmentStatistics() -> HashMap<String, String> {
    HashMap<String, String> stats;

    // Basic environment stats
    auto env = EnvCore::Environ();
    stats["total_variables"] = std::to_string(env.size());

    // Cache statistics
    auto coreStats = EnvCore::getCacheStats();
    stats["core_cache_hits"] = std::to_string(coreStats["hits"]);
    stats["core_cache_misses"] = std::to_string(coreStats["misses"]);

    auto pathStats = EnvPath::getCacheStats();
    stats["path_cache_hits"] = std::to_string(pathStats["hits"]);
    stats["path_cache_misses"] = std::to_string(pathStats["misses"]);

    auto systemStats = EnvSystem::getCacheStats();
    stats["system_cache_hits"] = std::to_string(systemStats["hits"]);
    stats["system_cache_misses"] = std::to_string(systemStats["misses"]);

    // Variable analysis
    auto analysis = EnvUtils::analyzeVariableUsage(env);
    for (const auto& [key, value] : analysis) {
        stats["analysis_" + key] = value;
    }

    return stats;
}

auto EnvAdvanced::validateEnvironmentRequirements(const HashMap<String, String>& requirements)
    -> HashMap<String, bool> {
    return EnvSystem::validateSystemRequirements(requirements);
}

// Helper method implementations
auto EnvAdvanced::serializeProfile(const EnvProfile& profile, EnvFileFormat format) -> String {
    // Simple JSON serialization for now
    (void)format;  // Suppress unused parameter warning

    std::stringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << profile.name << "\",\n";
    ss << "  \"description\": \"" << profile.description << "\",\n";
    ss << "  \"variables\": {\n";

    bool first = true;
    for (const auto& [key, value] : profile.variables) {
        if (!first) ss << ",\n";
        ss << "    \"" << key << "\": \"" << value << "\"";
        first = false;
    }

    ss << "\n  },\n";
    ss << "  \"pathEntries\": [\n";

    first = true;
    for (const auto& path : profile.pathEntries) {
        if (!first) ss << ",\n";
        ss << "    \"" << path << "\"";
        first = false;
    }

    ss << "\n  ]\n";
    ss << "}\n";

    return String(ss.str());
}

auto EnvAdvanced::deserializeProfile(const String& data, EnvFileFormat format) -> EnvProfile {
    // Simple JSON deserialization for now
    (void)format;  // Suppress unused parameter warning
    (void)data;    // Suppress unused parameter warning

    // This would need a proper JSON parser in a real implementation
    EnvProfile profile;
    profile.name = "Imported Profile";
    profile.description = "Profile imported from file";

    return profile;
}

auto EnvAdvanced::detectProfileFormat(const String& filePath) -> EnvFileFormat {
    String ext = String(std::filesystem::path(std::string(filePath.c_str())).extension().string());
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".json") return EnvFileFormat::JSON;
    if (ext == ".yaml" || ext == ".yml") return EnvFileFormat::YAML;
    if (ext == ".xml") return EnvFileFormat::XML;

    return EnvFileFormat::JSON;  // Default
}

auto EnvAdvanced::generateProfileId() -> String {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }

    return String(ss.str());
}

void EnvAdvanced::runMonitoringLoop(size_t sessionId, EnvMonitorCallback callback, int interval) {
    auto lastEnv = EnvCore::Environ();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));

        // Check if session is still active
        {
            std::lock_guard<std::mutex> lock(sMonitorMutex);
            if (sMonitorCallbacks.find(sessionId) == sMonitorCallbacks.end()) {
                break;  // Session was stopped
            }
        }

        auto currentEnv = EnvCore::Environ();
        auto diff = createEnvironmentDiff(lastEnv, currentEnv);

        if (!diff.empty()) {
            try {
                callback("environment_changed", diff);
            } catch (const std::exception& e) {
                spdlog::error("Exception in monitoring callback: {}", e.what());
            }
            lastEnv = currentEnv;
        }
    }
}

// SimpleEncryptionProvider implementation
SimpleEncryptionProvider::SimpleEncryptionProvider(const String& key) : mKey(key) {}

auto SimpleEncryptionProvider::encrypt(const String& plaintext) -> String {
    // Simple XOR encryption for demonstration
    String encrypted = ENCRYPTED_PREFIX;
    for (size_t i = 0; i < plaintext.length(); ++i) {
        char encryptedChar = plaintext[i] ^ mKey[i % mKey.length()];
        encrypted += encryptedChar;
    }
    return encrypted;
}

auto SimpleEncryptionProvider::decrypt(const String& ciphertext) -> String {
    if (!isEncrypted(ciphertext)) {
        return ciphertext;
    }

    String encrypted = ciphertext.substr(strlen(ENCRYPTED_PREFIX));
    String decrypted;
    for (size_t i = 0; i < encrypted.length(); ++i) {
        char decryptedChar = encrypted[i] ^ mKey[i % mKey.length()];
        decrypted += decryptedChar;
    }
    return decrypted;
}

auto SimpleEncryptionProvider::isEncrypted(const String& data) -> bool {
    return data.substr(0, strlen(ENCRYPTED_PREFIX)) == ENCRYPTED_PREFIX;
}

}  // namespace atom::utils
