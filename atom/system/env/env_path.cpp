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

auto EnvPath::addToPath(const String& path, bool prepend) -> bool {
    if (path.empty()) {
        spdlog::error("Cannot add empty path to PATH");
        return false;
    }

    String normalizedPath = normalizePath(path);

    // Check if path already exists
    if (isInPath(normalizedPath)) {
        spdlog::debug("Path already exists in PATH: {}", normalizedPath);
        return true;
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
    } else {
        spdlog::error("Failed to add path to PATH: {}", normalizedPath);
    }

    return result;
}

auto EnvPath::removeFromPath(const String& path) -> bool {
    if (path.empty()) {
        spdlog::error("Cannot remove empty path from PATH");
        return false;
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
        return true;
    }

    String newPath = joinPathString(entries);
    bool result = EnvCore::setEnv("PATH", newPath);

    if (result) {
        spdlog::info("Successfully removed path from PATH: {}", normalizedPath);
    } else {
        spdlog::error("Failed to remove path from PATH: {}", normalizedPath);
    }

    return result;
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

auto EnvPath::cleanupPath() -> bool {
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

    if (cleanEntries.size() == entries.size()) {
        spdlog::debug("PATH is already clean");
        return true;
    }

    String newPath = joinPathString(cleanEntries);
    bool result = EnvCore::setEnv("PATH", newPath);

    if (result) {
        spdlog::info("Successfully cleaned PATH: removed {} invalid/duplicate entries",
                     entries.size() - cleanEntries.size());
    } else {
        spdlog::error("Failed to clean PATH");
    }

    return result;
}

}  // namespace atom::utils
