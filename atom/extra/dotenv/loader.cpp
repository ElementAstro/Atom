#include "loader.hpp"

#include <algorithm>
#include <sstream>
#include "exceptions.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <fstream>
#endif

namespace dotenv {

FileLoader::FileLoader(const LoadOptions& options) : options_(options) {}

std::string FileLoader::load(const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath)) {
        if (options_.create_if_missing) {
            // Create empty file
            std::ofstream file(filepath);
            if (!file) {
                throw FileException("Cannot create file: " + filepath.string());
            }
            return "";
        } else {
            throw FileException("File not found: " + filepath.string());
        }
    }

    if (!isAccessible(filepath)) {
        throw FileException("File not accessible: " + filepath.string());
    }

    return readFile(filepath);
}

std::string FileLoader::loadMultiple(
    const std::vector<std::filesystem::path>& filepaths) {
    std::ostringstream combined;

    for (const auto& filepath : filepaths) {
        try {
            std::string content = load(filepath);
            if (!content.empty()) {
                combined << "# Content from: " << filepath.string() << "\n";
                combined << content;
                if (!content.empty() && content.back() != '\n') {
                    combined << "\n";
                }
                combined << "\n";
            }
        } catch (const FileException& e) {
            // Continue with other files, optionally log warning
            continue;
        }
    }

    return combined.str();
}

std::string FileLoader::autoLoad(const std::filesystem::path& base_path) {
    auto discovered_files = discoverFiles(base_path);
    return loadMultiple(discovered_files);
}

void FileLoader::save(
    const std::filesystem::path& filepath,
    const std::unordered_map<std::string, std::string>& env_vars) {
    std::ofstream file(filepath);
    if (!file) {
        throw FileException("Cannot create/write file: " + filepath.string());
    }

    file << "# Environment variables generated by dotenv-cpp\n";
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    file << "# Generated at: " << std::ctime(&now_time) << "\n";

    for (const auto& [key, value] : env_vars) {
        // Escape special characters in value
        std::string escaped_value = value;
        bool needs_quotes = value.find(' ') != std::string::npos ||
                            value.find('\t') != std::string::npos ||
                            value.find('\n') != std::string::npos ||
                            value.find('"') != std::string::npos;

        if (needs_quotes) {
            // Escape existing quotes
            size_t pos = 0;
            while ((pos = escaped_value.find('"', pos)) != std::string::npos) {
                escaped_value.replace(pos, 1, "\\\"");
                pos += 2;
            }
            escaped_value = "\"" + escaped_value + "\"";
        }

        file << key << "=" << escaped_value << "\n";
    }
}

bool FileLoader::isAccessible(const std::filesystem::path& filepath) {
    std::error_code ec;
    auto perms = std::filesystem::status(filepath, ec).permissions();
    if (ec)
        return false;

    return (perms & std::filesystem::perms::owner_read) !=
               std::filesystem::perms::none ||
           (perms & std::filesystem::perms::group_read) !=
               std::filesystem::perms::none ||
           (perms & std::filesystem::perms::others_read) !=
               std::filesystem::perms::none;
}

std::filesystem::file_time_type FileLoader::getModificationTime(
    const std::filesystem::path& filepath) {
    std::error_code ec;
    auto time = std::filesystem::last_write_time(filepath, ec);
    if (ec) {
        throw FileException("Cannot get modification time for: " +
                            filepath.string());
    }
    return time;
}

std::string FileLoader::readFile(const std::filesystem::path& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw FileException("Cannot open file: " + filepath.string());
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read entire file
    std::string content(size, '\0');
    file.read(&content[0], size);

    if (!file && !file.eof()) {
        throw FileException("Error reading file: " + filepath.string());
    }

    // Handle encoding conversion if needed
    std::string detected_encoding = detectEncoding(content);
    if (detected_encoding != options_.encoding) {
        content = convertEncoding(content, detected_encoding);
    }

    return content;
}

std::vector<std::filesystem::path> FileLoader::discoverFiles(
    const std::filesystem::path& base_path) {
    std::vector<std::filesystem::path> discovered;

    for (const auto& search_path : options_.search_paths) {
        std::filesystem::path full_search_path = base_path / search_path;

        if (!std::filesystem::exists(full_search_path))
            continue;

        for (const auto& pattern : options_.file_patterns) {
            std::filesystem::path candidate = full_search_path / pattern;

            if (std::filesystem::exists(candidate) && isAccessible(candidate)) {
                discovered.push_back(candidate);
            }
        }

        // Also check for pattern-based discovery
        try {
            for (const auto& entry :
                 std::filesystem::directory_iterator(full_search_path)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    for (const auto& pattern : options_.file_patterns) {
                        if (matchesPattern(filename, pattern)) {
                            discovered.push_back(entry.path());
                        }
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Skip directories we can't read
            continue;
        }
    }

    // Remove duplicates and sort
    std::sort(discovered.begin(), discovered.end());
    discovered.erase(std::unique(discovered.begin(), discovered.end()),
                     discovered.end());

    return discovered;
}

bool FileLoader::matchesPattern(const std::string& filename,
                                const std::string& pattern) {
    // Simple wildcard matching (* and ?)
    size_t pattern_pos = 0;
    size_t filename_pos = 0;

    while (pattern_pos < pattern.length() && filename_pos < filename.length()) {
        char p = pattern[pattern_pos];
        char f = filename[filename_pos];

        if (p == '*') {
            // Skip consecutive asterisks
            while (pattern_pos < pattern.length() &&
                   pattern[pattern_pos] == '*') {
                ++pattern_pos;
            }

            if (pattern_pos >= pattern.length()) {
                return true;  // Pattern ends with *, matches everything
            }

            // Find next matching character
            char next_char = pattern[pattern_pos];
            while (filename_pos < filename.length() &&
                   filename[filename_pos] != next_char) {
                ++filename_pos;
            }
        } else if (p == '?' || p == f) {
            ++pattern_pos;
            ++filename_pos;
        } else {
            return false;
        }
    }

    // Handle remaining asterisks in pattern
    while (pattern_pos < pattern.length() && pattern[pattern_pos] == '*') {
        ++pattern_pos;
    }

    return pattern_pos >= pattern.length() && filename_pos >= filename.length();
}

std::string FileLoader::detectEncoding(const std::string& content) {
    // Simple UTF-8 BOM detection
    if (content.length() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        return "utf-8-bom";
    }

    // For now, assume UTF-8 (could be extended with more sophisticated
    // detection)
    return "utf-8";
}

std::string FileLoader::convertEncoding(const std::string& content,
                                        const std::string& from_encoding) {
    // Remove UTF-8 BOM if present
    if (from_encoding == "utf-8-bom" && content.length() >= 3) {
        return content.substr(3);
    }

    // For now, just return as-is (could be extended with iconv or ICU)
    return content;
}

}  // namespace dotenv