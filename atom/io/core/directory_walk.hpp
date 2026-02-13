/*
 * directory_walk.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file directory_walk.hpp
 * @brief Directory traversal utilities: walk, jwalk, fwalk.
 */

#ifndef ATOM_IO_CORE_DIRECTORY_WALK_HPP
#define ATOM_IO_CORE_DIRECTORY_WALK_HPP

#include <functional>
#include <string>

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

#include "atom/io/core/types.hpp"
#include "atom/io/core/file_query.hpp"

namespace atom::io {

/**
 * @brief Recursively walks through a directory and its subdirectories, applying
 * a callback function to each file.
 *
 * This function traverses a directory and its subdirectories, calling the
 * specified callback function for each file encountered.
 *
 * @param root The root path of the directory to walk.
 * @return a json string containing the file information.
 */
template <PathLike P>
[[nodiscard]] auto jwalk(const P& root) -> std::string;

/**
 * @brief Recursively walks through a directory and its subdirectories, applying
 * a callback function to each file.
 *
 * This function traverses a directory and its subdirectories, calling the
 * specified callback function for each file encountered.
 *
 * @param root     The root path of the directory to walk.
 * @param callback The callback function to execute for each file.
 */
template <PathLike P>
void fwalk(const P& root, const std::function<void(const fs::path&)>& callback);

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Function to walk through directories and apply a callback
inline void walk(const fs::path& root, bool recursive,
                 const std::function<void(const fs::path&)>& callback) {
    spdlog::info("walk called with root: {}, recursive: {}", root.string(),
                 recursive);

    try {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (ec) {
                spdlog::error("Error traversing directory {}: {}",
                              root.string(), ec.message());
                continue;
            }

            callback(entry.path());

            if (recursive && fs::is_directory(entry, ec)) {
                if (!ec) {
                    walk(entry.path(), recursive, callback);
                } else {
                    spdlog::error("Error checking if {} is directory: {}",
                                  entry.path().string(), ec.message());
                }
            }
        }

        spdlog::info("walk completed for root: {}", root.string());
    } catch (const std::exception& e) {
        spdlog::error("Error walking directory {}: {}", root.string(),
                      e.what());
    }
}

// Helper function to build JSON structure
inline auto buildJsonStructure(const fs::path& root,
                               bool recursive) -> nlohmann::json {
    spdlog::info("buildJsonStructure called with root: {}, recursive: {}",
                 root.string(), recursive);

    nlohmann::json folder = {{"path", root.generic_string()},
                             {"directories", nlohmann::json::array()},
                             {"files", nlohmann::json::array()}};

    try {
        walk(root, recursive, [&](const fs::path& entry) {
            std::error_code ec;
            if (fs::is_directory(entry, ec)) {
                if (!ec) {
                    folder["directories"].push_back(
                        buildJsonStructure(entry, recursive));
                }
            } else if (!ec) {
                folder["files"].push_back(entry.generic_string());
            }
        });
    } catch (const std::exception& e) {
        spdlog::error("Error building JSON structure for {}: {}", root.string(),
                      e.what());
    }

    spdlog::info("buildJsonStructure completed for root: {}", root.string());
    return folder;
}

// ---------------------------------------------------------------------------
// Template implementations
// ---------------------------------------------------------------------------

template <PathLike P>
[[nodiscard]] auto jwalk(const P& root) -> std::string {
    spdlog::info("jwalk called with root: {}", fs::path(root).string());
    fs::path rootPath(root);

    try {
        if (!isFolderExists(rootPath)) {
            spdlog::warn("Folder does not exist: {}", rootPath.string());
            return "";
        }

        std::string result = buildJsonStructure(rootPath, true).dump();
        spdlog::info("jwalk completed for root: {}", rootPath.string());
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in jwalk for {}: {}", rootPath.string(), e.what());
        return "";
    } catch (...) {
        spdlog::error("Unknown error in jwalk for {}", rootPath.string());
        return "";
    }
}

template <PathLike P>
void fwalk(const P& root,
           const std::function<void(const fs::path&)>& callback) {
    spdlog::info("fwalk called with root: {}", fs::path(root).string());

    try {
        fs::path rootPath(root);
        walk(rootPath, true, callback);
        spdlog::info("fwalk completed for root: {}", rootPath.string());
    } catch (const std::exception& e) {
        spdlog::error("Error in fwalk for {}: {}", fs::path(root).string(),
                      e.what());
    } catch (...) {
        spdlog::error("Unknown error in fwalk for {}", fs::path(root).string());
    }
}

}  // namespace atom::io

#endif  // ATOM_IO_CORE_DIRECTORY_WALK_HPP
