/*
 * env_path.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: PATH environment variable management implementation

**************************************************/

#include "env_path.hpp"

#include <algorithm>
#include <filesystem>
#include <unordered_set>

#include "env_core.hpp"
#include <spdlog/spdlog.h>

namespace atom::utils {

// Static member initializations
PathCacheEntry EnvPath::sPathCache;
std::mutex EnvPath::sCacheMutex;
std::atomic<bool> EnvPath::sCachingEnabled{false};
std::atomic<int> EnvPath::sCacheTtlSeconds{60};
std::atomic<size_t> EnvPath::sCacheHits{0};
std::atomic<size_t> EnvPath::sCacheMisses{0};
HashMap<String, String> EnvPath::sPathBackups;
std::mutex EnvPath::sBackupMutex;

auto EnvPath::getPathSeparator() -> char {
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

auto EnvPath::splitPathString(const String& pathStr) -> Vector<String> {
    Vector<String> result;
    if (pathStr.empty()) {
        return result;
    }

    char separator = getPathSeparator();
    size_t start = 0;
    size_t end = pathStr.find(separator);

    while (end != String::npos) {
        String path = pathStr.substr(start, end - start);
        if (!path.empty()) {
            // Trim whitespace
            while (!path.empty() && std::isspace(path.front())) {
                path.erase(0, 1);
            }
            while (!path.empty() && std::isspace(path.back())) {
                path.pop_back();
            }
            if (!path.empty()) {
                result.push_back(normalizePath(path));
            }
        }
        start = end + 1;
        end = pathStr.find(separator, start);
    }

    // Handle the last path
    if (start < pathStr.length()) {
        String path = pathStr.substr(start);
        // Trim whitespace
        while (!path.empty() && std::isspace(path.front())) {
            path.erase(0, 1);
        }
        while (!path.empty() && std::isspace(path.back())) {
            path.pop_back();
        }
        if (!path.empty()) {
            result.push_back(normalizePath(path));
        }
    }

    return result;
}

auto EnvPath::joinPathString(const Vector<String>& paths) -> String {
    if (paths.empty()) {
        return "";
    }

    String result;
    char separator = getPathSeparator();

    for (size_t i = 0; i < paths.size(); ++i) {
        if (i > 0) {
            result += separator;
        }
        result += paths[i];
    }

    return result;
}

auto EnvPath::normalizePath(const String& path) -> String {
    if (path.empty()) {
        return path;
    }

    try {
        std::filesystem::path p(std::string(path.data(), path.length()));
        std::filesystem::path normalized = p.lexically_normal();
        return String(normalized.string());
    } catch (const std::exception&) {
        // If normalization fails, return the original path
        return path;
    }
}

auto EnvPath::getPathEntries() -> Vector<String> {
    String pathVar = EnvCore::getEnv("PATH", "");
    return splitPathString(pathVar);
}

auto EnvPath::isInPath(const String& path) -> bool {
    Vector<String> entries = getPathEntries();
    String normalizedPath = normalizePath(path);

    for (const auto& entry : entries) {
        if (normalizePath(entry) == normalizedPath) {
            return true;
        }
    }

    return false;
}

auto EnvPath::addToPath(const String& path, bool prepend) -> PathOperationResult {
    if (path.empty()) {
        spdlog::error("Cannot add empty path to PATH");
        return PathOperationResult::INVALID_PATH;
    }

    if (!isValidPath(path)) {
        spdlog::error("Invalid path: {}", path);
        return PathOperationResult::INVALID_PATH;
    }

    String normalizedPath = normalizePath(path);

    // Check if path already exists
    if (isInPath(normalizedPath)) {
        spdlog::debug("Path already exists in PATH: {}", normalizedPath);
        return PathOperationResult::ALREADY_EXISTS;
    }

    Vector<String> entries = getPathEntries();

    if (prepend) {
        entries.insert(entries.begin(), normalizedPath);
    } else {
        entries.push_back(normalizedPath);
    }

    String newPath = joinPathString(entries);
    bool result = EnvCore::setEnv("PATH", newPath);

    if (result) {
        spdlog::info("Successfully {} path to PATH: {}",
                     prepend ? "prepended" : "appended", normalizedPath);
        // Clear cache since PATH changed
        clearCache();
        return PathOperationResult::SUCCESS;
    } else {
        spdlog::error("Failed to add path to PATH: {}", normalizedPath);
        return PathOperationResult::SYSTEM_ERROR;
    }
}

auto EnvPath::removeFromPath(const String& path) -> PathOperationResult {
    if (path.empty()) {
        spdlog::error("Cannot remove empty path from PATH");
        return PathOperationResult::INVALID_PATH;
    }

    String normalizedPath = normalizePath(path);
    Vector<String> entries = getPathEntries();

    auto originalSize = entries.size();
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                       [&normalizedPath](const String& entry) {
                           return normalizePath(entry) == normalizedPath;
                       }),
        entries.end());

    if (entries.size() == originalSize) {
        spdlog::debug("Path not found in PATH: {}", normalizedPath);
        return PathOperationResult::NOT_FOUND;
    }

    String newPath = joinPathString(entries);
    bool result = EnvCore::setEnv("PATH", newPath);

    if (result) {
        spdlog::info("Successfully removed path from PATH: {}", normalizedPath);
        // Clear cache since PATH changed
        clearCache();
        return PathOperationResult::SUCCESS;
    } else {
        spdlog::error("Failed to remove path from PATH: {}", normalizedPath);
        return PathOperationResult::SYSTEM_ERROR;
    }
}

auto EnvPath::isValidPath(const String& path) -> bool {
    if (path.empty()) {
        return false;
    }

    try {
        std::filesystem::path p(std::string(path.data(), path.length()));
        return std::filesystem::exists(p) && std::filesystem::is_directory(p);
    } catch (const std::exception&) {
        return false;
    }
}

auto EnvPath::removeDuplicatesFromPath() -> bool {
    Vector<String> entries = getPathEntries();
    std::unordered_set<String> seen;
    Vector<String> uniqueEntries;

    for (const auto& entry : entries) {
        String normalizedEntry = normalizePath(entry);
        if (seen.find(normalizedEntry) == seen.end()) {
            seen.insert(normalizedEntry);
            uniqueEntries.push_back(entry);
        }
    }

    if (uniqueEntries.size() == entries.size()) {
        spdlog::debug("No duplicates found in PATH");
        return true;
    }

    String newPath = joinPathString(uniqueEntries);
    bool result = EnvCore::setEnv("PATH", newPath);

    if (result) {
        spdlog::info("Successfully removed {} duplicate entries from PATH",
                     entries.size() - uniqueEntries.size());
    } else {
        spdlog::error("Failed to remove duplicates from PATH");
    }

    return result;
}

auto EnvPath::cleanupPath() -> size_t {
    Vector<String> entries = getPathEntries();
    std::unordered_set<String> seen;
    Vector<String> cleanEntries;

    for (const auto& entry : entries) {
        String normalizedEntry = normalizePath(entry);

        // Skip duplicates
        if (seen.find(normalizedEntry) != seen.end()) {
            continue;
        }

        seen.insert(normalizedEntry);

        // Keep valid paths
        if (isValidPath(entry)) {
            cleanEntries.push_back(entry);
        } else {
            spdlog::debug("Removing invalid path: {}", entry);
        }
    }

    size_t removedCount = entries.size() - cleanEntries.size();

    if (removedCount == 0) {
        spdlog::debug("PATH is already clean");
        return 0;
    }

    String newPath = joinPathString(cleanEntries);
    bool result = EnvCore::setEnv("PATH", newPath);

    if (result) {
        spdlog::info("Successfully cleaned PATH: removed {} invalid/duplicate entries", removedCount);
        // Clear cache since PATH changed
        clearCache();
        return removedCount;
    } else {
        spdlog::error("Failed to clean PATH");
        return 0;
    }
}

// ========== NEW ENHANCED FUNCTIONALITY ==========

auto EnvPath::addMultiplePaths(const Vector<String>& paths, bool prepend) -> size_t {
    size_t successCount = 0;

    for (const auto& path : paths) {
        if (addToPath(path, prepend) == PathOperationResult::SUCCESS) {
            successCount++;
        }
    }

    spdlog::info("Successfully added {}/{} paths to PATH", successCount, paths.size());
    return successCount;
}

auto EnvPath::removeMultiplePaths(const Vector<String>& paths) -> size_t {
    size_t successCount = 0;

    for (const auto& path : paths) {
        auto result = removeFromPath(path);
        if (result == PathOperationResult::SUCCESS || result == PathOperationResult::NOT_FOUND) {
            successCount++;
        }
    }

    spdlog::info("Successfully processed {}/{} paths for removal", successCount, paths.size());
    return successCount;
}

void EnvPath::setCachingEnabled(bool enabled, int ttl_seconds) {
    sCachingEnabled.store(enabled);
    sCacheTtlSeconds.store(ttl_seconds);
    spdlog::debug("PATH caching {}, TTL: {} seconds",
                  enabled ? "enabled" : "disabled", ttl_seconds);

    if (!enabled) {
        clearCache();
    }
}

void EnvPath::clearCache() {
    std::lock_guard<std::mutex> lock(sCacheMutex);
    sPathCache = PathCacheEntry();
    sCacheHits.store(0);
    sCacheMisses.store(0);
    spdlog::debug("PATH cache cleared");
}

auto EnvPath::getCacheStats() -> HashMap<String, size_t> {
    HashMap<String, size_t> stats;
    stats["hits"] = sCacheHits.load();
    stats["misses"] = sCacheMisses.load();
    stats["enabled"] = sCachingEnabled.load() ? 1 : 0;
    stats["ttl_seconds"] = static_cast<size_t>(sCacheTtlSeconds.load());
    stats["entries"] = sPathCache.isValid ? sPathCache.paths.size() : 0;
    return stats;
}

auto EnvPath::findExecutable(const String& executable) -> Vector<String> {
    Vector<String> results;
    Vector<String> paths;

    // Try cache first if enabled
    if (sCachingEnabled.load()) {
        auto cachedPaths = getCachedPaths();
        if (cachedPaths.has_value()) {
            paths = cachedPaths.value();
            sCacheHits.fetch_add(1);
        } else {
            paths = getPathEntries();
            setCachedPaths(paths);
            sCacheMisses.fetch_add(1);
        }
    } else {
        paths = getPathEntries();
    }

    for (const auto& pathEntry : paths) {
        std::filesystem::path dir(std::string(pathEntry.data(), pathEntry.length()));

        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
            continue;
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    String filename = String(entry.path().filename().string());

                    // Check exact match or with common extensions
                    if (filename == executable ||
                        filename == executable + ".exe" ||
                        filename == executable + ".bat" ||
                        filename == executable + ".cmd") {

                        if (isExecutableFile(entry.path())) {
                            results.push_back(String(entry.path().string()));
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::debug("Error scanning directory {}: {}", pathEntry, e.what());
        }
    }

    spdlog::debug("Found {} instances of executable '{}'", results.size(), executable);
    return results;
}

auto EnvPath::findExecutablesMatching(const String& pattern) -> Vector<String> {
    Vector<String> results;
    Vector<String> paths = getPathEntries();

    for (const auto& pathEntry : paths) {
        std::filesystem::path dir(std::string(pathEntry.data(), pathEntry.length()));

        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
            continue;
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    String filename = String(entry.path().filename().string());

                    if (matchesPattern(filename, pattern) && isExecutableFile(entry.path())) {
                        results.push_back(String(entry.path().string()));
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::debug("Error scanning directory {}: {}", pathEntry, e.what());
        }
    }

    spdlog::debug("Found {} executables matching pattern '{}'", results.size(), pattern);
    return results;
}

auto EnvPath::getPathStats() -> HashMap<String, size_t> {
    Vector<String> entries = getPathEntries();
    HashMap<String, size_t> stats;

    size_t validPaths = 0;
    size_t duplicates = 0;
    std::unordered_set<String> seen;

    for (const auto& entry : entries) {
        String normalized = normalizePath(entry);

        if (seen.find(normalized) != seen.end()) {
            duplicates++;
        } else {
            seen.insert(normalized);
        }

        if (isValidPath(entry)) {
            validPaths++;
        }
    }

    stats["total_entries"] = entries.size();
    stats["valid_paths"] = validPaths;
    stats["invalid_paths"] = entries.size() - validPaths;
    stats["duplicates"] = duplicates;
    stats["unique_paths"] = seen.size();

    return stats;
}

auto EnvPath::backupPath(const String& name) -> bool {
    try {
        std::lock_guard<std::mutex> lock(sBackupMutex);
        String currentPath = EnvCore::getEnv("PATH", "");
        sPathBackups[name] = currentPath;
        spdlog::debug("Created PATH backup: {}", name);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create PATH backup '{}': {}", name, e.what());
        return false;
    }
}

auto EnvPath::restorePath(const String& name) -> bool {
    try {
        std::lock_guard<std::mutex> lock(sBackupMutex);
        auto it = sPathBackups.find(name);
        if (it == sPathBackups.end()) {
            spdlog::error("PATH backup '{}' not found", name);
            return false;
        }

        bool result = EnvCore::setEnv("PATH", it->second);
        if (result) {
            clearCache();
            spdlog::debug("Restored PATH from backup: {}", name);
        }
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Failed to restore PATH backup '{}': {}", name, e.what());
        return false;
    }
}

auto EnvPath::listPathBackups() -> Vector<String> {
    std::lock_guard<std::mutex> lock(sBackupMutex);
    Vector<String> names;
    names.reserve(sPathBackups.size());

    for (const auto& [name, path] : sPathBackups) {
        names.push_back(name);
    }

    return names;
}

auto EnvPath::optimizePath() -> bool {
    // This is a placeholder for path optimization based on usage frequency
    // In a real implementation, this could track executable usage and reorder paths
    spdlog::info("PATH optimization not yet implemented");
    return true;
}

auto EnvPath::validateAllPaths() -> Vector<String> {
    Vector<String> invalidPaths;
    Vector<String> entries = getPathEntries();

    for (const auto& entry : entries) {
        if (!isValidPath(entry)) {
            invalidPaths.push_back(entry);
        }
    }

    spdlog::debug("Found {} invalid paths out of {} total", invalidPaths.size(), entries.size());
    return invalidPaths;
}

auto EnvPath::getFirstPathContaining(const String& executable) -> String {
    Vector<String> paths = findExecutable(executable);
    if (!paths.empty()) {
        std::filesystem::path fullPath(std::string(paths[0].data(), paths[0].length()));
        return String(fullPath.parent_path().string());
    }
    return "";
}

// Helper method implementations
auto EnvPath::getCachedPaths() -> std::optional<Vector<String>> {
    std::lock_guard<std::mutex> lock(sCacheMutex);
    if (sPathCache.isValid && isCacheValid()) {
        return sPathCache.paths;
    }
    return std::nullopt;
}

void EnvPath::setCachedPaths(const Vector<String>& paths) {
    std::lock_guard<std::mutex> lock(sCacheMutex);
    sPathCache = PathCacheEntry(paths);
}

auto EnvPath::isCacheValid() -> bool {
    if (!sPathCache.isValid) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - sPathCache.timestamp);
    return elapsed.count() < sCacheTtlSeconds.load();
}

auto EnvPath::isExecutableFile(const std::filesystem::path& filePath) -> bool {
    try {
        if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
            return false;
        }

#ifdef _WIN32
        // On Windows, check file extension
        String ext = String(filePath.extension().string());
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com";
#else
        // On Unix-like systems, check execute permissions
        auto perms = std::filesystem::status(filePath).permissions();
        return (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ||
               (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none ||
               (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none;
#endif
    } catch (const std::exception&) {
        return false;
    }
}

auto EnvPath::matchesPattern(const String& filename, const String& pattern) -> bool {
    // Simple wildcard matching - in a real implementation, use a proper pattern matcher
    if (pattern == "*") {
        return true;
    }

    if (pattern.find('*') == String::npos) {
        return filename == pattern;
    }

    // Basic wildcard support
    if (pattern.back() == '*') {
        String prefix = pattern.substr(0, pattern.length() - 1);
        return filename.substr(0, prefix.length()) == prefix;
    }

    if (pattern.front() == '*') {
        String suffix = pattern.substr(1);
        return filename.length() >= suffix.length() &&
               filename.substr(filename.length() - suffix.length()) == suffix;
    }

    return filename == pattern;
}

}  // namespace atom::utils
