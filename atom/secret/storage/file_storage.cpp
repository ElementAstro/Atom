#include "file_storage.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>

#include "spdlog/spdlog.h"

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace atom::secret {

FileStorage::FileStorage(std::string_view appName,
                         const std::string& customPath)
    : appName_(appName), storageDir_(getStorageDirectory(appName, customPath)) {
    spdlog::info("Using file-based secure storage at: {}",
                 storageDir_.string());
}

Result<void> FileStorage::store(std::string_view key, std::string_view data) {
    return store(key, std::vector<uint8_t>(data.begin(), data.end()));
}

Result<void> FileStorage::store(std::string_view key,
                                const std::vector<uint8_t>& data) {
    if (key.empty()) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "Key cannot be empty");
    }

    std::filesystem::path filePath = getFilePath(key);

    try {
        std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
        if (!outFile) {
            return Result<void>::error(
                ErrorCode::StorageWriteFailed,
                "Failed to open file for writing: " + filePath.string());
        }

        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outFile.close();

        if (!outFile) {
            return Result<void>::error(ErrorCode::StorageWriteFailed,
                                       "Failed to write data to file");
        }

        updateIndex(sanitizeKey(key), true);
        spdlog::debug("Data stored successfully to file: {}", sanitizeKey(key));
        return Result<void>::success();

    } catch (const std::exception& e) {
        return Result<void>::error(
            ErrorCode::StorageWriteFailed,
            std::string("Failed to store data: ") + e.what());
    }
}

Result<std::string> FileStorage::retrieve(std::string_view key) {
    auto result = retrieveBytes(key);
    if (result.isError()) {
        return Result<std::string>::error(result.errorCode(),
                                          result.errorMessage());
    }
    const auto& bytes = result.value();
    return Result<std::string>::success(
        std::string(bytes.begin(), bytes.end()));
}

Result<std::vector<uint8_t>> FileStorage::retrieveBytes(std::string_view key) {
    if (key.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Key cannot be empty");
    }

    std::filesystem::path filePath = getFilePath(key);

    try {
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::StorageKeyNotFound, "Key not found");
        }

        inFile.seekg(0, std::ios::end);
        size_t size = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        inFile.read(reinterpret_cast<char*>(data.data()), size);

        return Result<std::vector<uint8_t>>::success(std::move(data));

    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::StorageReadFailed,
            std::string("Failed to retrieve data: ") + e.what());
    }
}

Result<void> FileStorage::remove(std::string_view key) {
    if (key.empty()) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "Key cannot be empty");
    }

    std::filesystem::path filePath = getFilePath(key);

    try {
        std::error_code ec;
        if (std::filesystem::remove(filePath, ec) ||
            !std::filesystem::exists(filePath)) {
            updateIndex(sanitizeKey(key), false);
            spdlog::debug("Successfully removed file for key: {}",
                          std::string(key));
            return Result<void>::success();
        }

        return Result<void>::error(ErrorCode::StorageDeleteFailed,
                                   "Failed to delete file: " + ec.message());

    } catch (const std::exception& e) {
        return Result<void>::error(
            ErrorCode::StorageDeleteFailed,
            std::string("Failed to remove data: ") + e.what());
    }
}

bool FileStorage::exists(std::string_view key) {
    if (key.empty())
        return false;
    return std::filesystem::exists(getFilePath(key));
}

Result<std::vector<std::string>> FileStorage::getAllKeys() {
    std::vector<std::string> keys;
    std::filesystem::path indexPath = storageDir_ / "index.txt";

    try {
        std::ifstream indexFile(indexPath);
        if (indexFile) {
            std::string line;
            while (std::getline(indexFile, line)) {
                if (!line.empty()) {
                    keys.push_back(line);
                }
            }
        } else {
            // Fallback: scan directory
            for (const auto& entry :
                 std::filesystem::directory_iterator(storageDir_)) {
                if (entry.is_regular_file() &&
                    entry.path().extension() == ".dat") {
                    std::string filename = entry.path().stem().string();
                    keys.push_back(filename);
                }
            }
        }
        return Result<std::vector<std::string>>::success(std::move(keys));

    } catch (const std::exception& e) {
        return Result<std::vector<std::string>>::error(
            ErrorCode::StorageReadFailed,
            std::string("Failed to get keys: ") + e.what());
    }
}

Result<void> FileStorage::clear() {
    try {
        auto keysResult = getAllKeys();
        if (keysResult.isError()) {
            return Result<void>::error(keysResult.errorCode(),
                                       keysResult.errorMessage());
        }

        for (const auto& key : keysResult.value()) {
            auto removeResult = remove(key);
            if (removeResult.isError()) {
                spdlog::warn("Failed to remove key during clear: {}", key);
            }
        }

        // Remove index file
        std::filesystem::path indexPath = storageDir_ / "index.txt";
        std::filesystem::remove(indexPath);

        return Result<void>::success();

    } catch (const std::exception& e) {
        return Result<void>::error(
            ErrorCode::StorageDeleteFailed,
            std::string("Failed to clear storage: ") + e.what());
    }
}

std::filesystem::path FileStorage::getFilePath(std::string_view key) const {
    return storageDir_ / (sanitizeKey(key) + ".dat");
}

std::string FileStorage::sanitizeKey(std::string_view key) {
    std::string sanitized;
    sanitized.reserve(key.length());

    for (char c : key) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' ||
            c == '_') {
            sanitized += c;
        } else {
            sanitized += '_';
        }
    }

    if (sanitized.length() > 100) {
        sanitized.resize(100);
    }

    return sanitized;
}

void FileStorage::updateIndex(const std::string& key, bool add) {
    std::filesystem::path indexPath = storageDir_ / "index.txt";

    auto keysResult = getAllKeys();
    std::vector<std::string> keys;
    if (keysResult.isSuccess()) {
        keys = std::move(keysResult.value());
    }

    if (add) {
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
            keys.push_back(key);
        }
    } else {
        keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
    }

    try {
        std::ofstream indexFile(indexPath, std::ios::binary | std::ios::trunc);
        if (indexFile) {
            for (const auto& k : keys) {
                indexFile << k << '\n';
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to update index file: {}", e.what());
    }
}

std::filesystem::path FileStorage::getStorageDirectory(
    std::string_view appName, const std::string& customPath) {
    std::filesystem::path storageDir;

    if (!customPath.empty()) {
        storageDir = customPath;
    } else {
#if defined(_WIN32)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr,
                                           &path))) {
            storageDir = path;
            storageDir /= appName;
            CoTaskMemFree(path);
        } else {
            const char* appDataPath = std::getenv("LOCALAPPDATA");
            if (appDataPath) {
                storageDir = appDataPath;
                storageDir /= appName;
            } else {
                storageDir = std::string(".") + std::string(appName);
            }
        }
#elif defined(__APPLE__)
        const char* homeDir = std::getenv("HOME");
        if (!homeDir) {
            struct passwd* pw = getpwuid(getuid());
            if (pw)
                homeDir = pw->pw_dir;
        }
        if (homeDir) {
            storageDir = homeDir;
            storageDir /= "Library/Application Support";
            storageDir /= appName;
        } else {
            storageDir = std::string(".") + std::string(appName);
        }
#elif defined(__linux__)
        const char* homeDir = std::getenv("HOME");
        if (!homeDir) {
            struct passwd* pw = getpwuid(getuid());
            if (pw)
                homeDir = pw->pw_dir;
        }
        if (homeDir) {
            storageDir = homeDir;
            storageDir /= ".local/share";
            storageDir /= appName;
        } else {
            storageDir = std::string(".") + std::string(appName);
        }
#else
        storageDir = std::string(".") + std::string(appName);
#endif
    }

    // Create directory if it doesn't exist
    std::error_code ec;
    if (!std::filesystem::exists(storageDir, ec) && !ec) {
        if (!std::filesystem::create_directories(storageDir, ec) && ec) {
            spdlog::error("Failed to create storage directory '{}': {}",
                          storageDir.string(), ec.message());
        }
#if !defined(_WIN32)
        chmod(storageDir.c_str(), 0700);
#endif
    }

    return storageDir;
}

}  // namespace atom::secret
