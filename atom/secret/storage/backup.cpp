#include "backup.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "../crypto/encryption.hpp"

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <pwd.h>
#include <unistd.h>
#endif

namespace atom::secret {

Result<BackupInfo> BackupManager::createBackup(
    const std::vector<uint8_t>& data, const std::filesystem::path& backupDir,
    const BackupOptions& options) {
    // Ensure backup directory exists
    std::error_code ec;
    if (!std::filesystem::exists(backupDir, ec)) {
        if (!std::filesystem::create_directories(backupDir, ec)) {
            return Result<BackupInfo>::error(
                ErrorCode::StorageWriteFailed,
                "Failed to create backup directory: " + ec.message());
        }
    }

    // Generate filename
    std::string filename = generateBackupFilename();
    std::filesystem::path backupPath = backupDir / filename;

    // Prepare backup data
    std::vector<uint8_t> backupData;

    // Write header
    // [magic:4][version:1][flags:1][timestamp:8][data_size:4][data]
    backupData.reserve(data.size() + 64);

    // Magic number
    backupData.push_back((BACKUP_MAGIC >> 24) & 0xFF);
    backupData.push_back((BACKUP_MAGIC >> 16) & 0xFF);
    backupData.push_back((BACKUP_MAGIC >> 8) & 0xFF);
    backupData.push_back(BACKUP_MAGIC & 0xFF);

    // Version
    backupData.push_back(BACKUP_VERSION);

    // Flags
    uint8_t flags = 0;
    if (options.encrypt)
        flags |= 0x01;
    if (options.compress)
        flags |= 0x02;
    if (options.includeSettings)
        flags |= 0x04;
    if (options.includeHistory)
        flags |= 0x08;
    backupData.push_back(flags);

    // Timestamp
    auto now = std::chrono::system_clock::now();
    int64_t timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    for (int i = 7; i >= 0; --i) {
        backupData.push_back((timestamp >> (i * 8)) & 0xFF);
    }

    // Data size
    uint32_t dataSize = static_cast<uint32_t>(data.size());
    backupData.push_back((dataSize >> 24) & 0xFF);
    backupData.push_back((dataSize >> 16) & 0xFF);
    backupData.push_back((dataSize >> 8) & 0xFF);
    backupData.push_back(dataSize & 0xFF);

    // Encrypt if requested
    std::vector<uint8_t> processedData;
    if (options.encrypt && !options.password.empty()) {
        auto encResult = Encryption::encrypt(
            std::string(data.begin(), data.end()), options.password);
        if (encResult.isError()) {
            return Result<BackupInfo>::error(
                ErrorCode::EncryptionFailed,
                "Failed to encrypt backup: " + encResult.errorMessage());
        }
        processedData = encResult.value().serialize();
    } else {
        processedData = data;
    }

    // Append data
    backupData.insert(backupData.end(), processedData.begin(),
                      processedData.end());

    // Write to file
    try {
        std::ofstream outFile(backupPath, std::ios::binary);
        if (!outFile) {
            return Result<BackupInfo>::error(ErrorCode::StorageWriteFailed,
                                             "Failed to create backup file");
        }
        outFile.write(reinterpret_cast<const char*>(backupData.data()),
                      backupData.size());
        outFile.close();

        if (!outFile) {
            return Result<BackupInfo>::error(ErrorCode::StorageWriteFailed,
                                             "Failed to write backup data");
        }
    } catch (const std::exception& e) {
        return Result<BackupInfo>::error(
            ErrorCode::StorageWriteFailed,
            std::string("Backup failed: ") + e.what());
    }

    // Cleanup old backups if needed
    if (options.maxBackups > 0) {
        cleanupOldBackups(backupDir, options.maxBackups);
    }

    BackupInfo info;
    info.filename = filename;
    info.path = backupPath;
    info.created = now;
    info.size = backupData.size();
    info.version = std::to_string(BACKUP_VERSION);
    info.encrypted = options.encrypt;

    return Result<BackupInfo>::success(std::move(info));
}

Result<std::vector<uint8_t>> BackupManager::restoreBackup(
    const std::filesystem::path& backupPath, const RestoreOptions& options) {
    // Read backup file
    std::vector<uint8_t> backupData;
    try {
        std::ifstream inFile(backupPath, std::ios::binary);
        if (!inFile) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::StorageReadFailed, "Failed to open backup file");
        }

        inFile.seekg(0, std::ios::end);
        size_t size = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        backupData.resize(size);
        inFile.read(reinterpret_cast<char*>(backupData.data()), size);
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::StorageReadFailed,
            std::string("Failed to read backup: ") + e.what());
    }

    // Verify header
    if (backupData.size() < 18) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::DataCorrupted,
                                                   "Backup file too small");
    }

    size_t pos = 0;

    // Check magic
    uint32_t magic = (static_cast<uint32_t>(backupData[pos]) << 24) |
                     (static_cast<uint32_t>(backupData[pos + 1]) << 16) |
                     (static_cast<uint32_t>(backupData[pos + 2]) << 8) |
                     static_cast<uint32_t>(backupData[pos + 3]);
    pos += 4;

    if (magic != BACKUP_MAGIC) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidFormat, "Invalid backup file format");
    }

    // Version
    uint8_t version = backupData[pos++];
    if (version > BACKUP_VERSION) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::UnsupportedVersion, "Unsupported backup version");
    }

    // Flags
    uint8_t flags = backupData[pos++];
    bool encrypted = (flags & 0x01) != 0;

    // Skip timestamp
    pos += 8;

    // Data size
    uint32_t dataSize = (static_cast<uint32_t>(backupData[pos]) << 24) |
                        (static_cast<uint32_t>(backupData[pos + 1]) << 16) |
                        (static_cast<uint32_t>(backupData[pos + 2]) << 8) |
                        static_cast<uint32_t>(backupData[pos + 3]);
    pos += 4;

    // Extract data
    std::vector<uint8_t> rawData(backupData.begin() + pos, backupData.end());

    // Decrypt if needed
    if (encrypted) {
        if (options.password.empty()) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::InvalidArgument,
                "Password required for encrypted backup");
        }

        auto encDataResult = EncryptedData::deserialize(rawData);
        if (encDataResult.isError()) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::DataCorrupted, "Failed to parse encrypted data");
        }

        auto decResult =
            Encryption::decrypt(encDataResult.value(), options.password);
        if (decResult.isError()) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::DecryptionFailed,
                "Failed to decrypt backup: " + decResult.errorMessage());
        }

        const auto& decrypted = decResult.value();
        return Result<std::vector<uint8_t>>::success(
            std::vector<uint8_t>(decrypted.begin(), decrypted.end()));
    }

    return Result<std::vector<uint8_t>>::success(std::move(rawData));
}

Result<std::vector<BackupInfo>> BackupManager::listBackups(
    const std::filesystem::path& backupDir) {
    std::vector<BackupInfo> backups;

    if (!std::filesystem::exists(backupDir)) {
        return Result<std::vector<BackupInfo>>::success(std::move(backups));
    }

    try {
        for (const auto& entry :
             std::filesystem::directory_iterator(backupDir)) {
            if (!entry.is_regular_file())
                continue;

            std::string ext = entry.path().extension().string();
            if (ext != ".atombak")
                continue;

            auto verifyResult = verifyBackup(entry.path());
            if (verifyResult.isSuccess()) {
                backups.push_back(std::move(verifyResult.value()));
            }
        }

        // Sort by creation time (newest first)
        std::sort(backups.begin(), backups.end(),
                  [](const BackupInfo& a, const BackupInfo& b) {
                      return a.created > b.created;
                  });

    } catch (const std::exception& e) {
        return Result<std::vector<BackupInfo>>::error(
            ErrorCode::StorageReadFailed,
            std::string("Failed to list backups: ") + e.what());
    }

    return Result<std::vector<BackupInfo>>::success(std::move(backups));
}

Result<void> BackupManager::deleteBackup(
    const std::filesystem::path& backupPath) {
    std::error_code ec;
    if (!std::filesystem::remove(backupPath, ec)) {
        if (ec) {
            return Result<void>::error(
                ErrorCode::StorageDeleteFailed,
                "Failed to delete backup: " + ec.message());
        }
    }
    return Result<void>::success();
}

Result<int> BackupManager::cleanupOldBackups(
    const std::filesystem::path& backupDir, int keepCount) {
    auto listResult = listBackups(backupDir);
    if (listResult.isError()) {
        return Result<int>::error(listResult.errorCode(),
                                  listResult.errorMessage());
    }

    const auto& backups = listResult.value();
    int deleted = 0;

    for (size_t i = keepCount; i < backups.size(); ++i) {
        auto delResult = deleteBackup(backups[i].path);
        if (delResult.isSuccess()) {
            ++deleted;
        }
    }

    return Result<int>::success(deleted);
}

Result<BackupInfo> BackupManager::verifyBackup(
    const std::filesystem::path& backupPath, const std::string& password) {
    std::vector<uint8_t> header(18);

    try {
        std::ifstream inFile(backupPath, std::ios::binary);
        if (!inFile) {
            return Result<BackupInfo>::error(ErrorCode::StorageReadFailed,
                                             "Failed to open backup file");
        }

        inFile.read(reinterpret_cast<char*>(header.data()), 18);
        if (inFile.gcount() < 18) {
            return Result<BackupInfo>::error(ErrorCode::DataCorrupted,
                                             "Backup file too small");
        }

        // Get file size
        inFile.seekg(0, std::ios::end);
        size_t fileSize = inFile.tellg();

        // Parse header
        size_t pos = 0;

        uint32_t magic = (static_cast<uint32_t>(header[pos]) << 24) |
                         (static_cast<uint32_t>(header[pos + 1]) << 16) |
                         (static_cast<uint32_t>(header[pos + 2]) << 8) |
                         static_cast<uint32_t>(header[pos + 3]);
        pos += 4;

        if (magic != BACKUP_MAGIC) {
            return Result<BackupInfo>::error(ErrorCode::InvalidFormat,
                                             "Invalid backup file format");
        }

        uint8_t version = header[pos++];
        uint8_t flags = header[pos++];

        int64_t timestamp = 0;
        for (int i = 0; i < 8; ++i) {
            timestamp = (timestamp << 8) | header[pos++];
        }

        BackupInfo info;
        info.filename = backupPath.filename().string();
        info.path = backupPath;
        info.created = std::chrono::system_clock::time_point(
            std::chrono::seconds(timestamp));
        info.size = fileSize;
        info.version = std::to_string(version);
        info.encrypted = (flags & 0x01) != 0;

        return Result<BackupInfo>::success(std::move(info));

    } catch (const std::exception& e) {
        return Result<BackupInfo>::error(
            ErrorCode::StorageReadFailed,
            std::string("Failed to verify backup: ") + e.what());
    }
}

std::filesystem::path BackupManager::getDefaultBackupDir(
    std::string_view appName) {
    std::filesystem::path backupDir;

#if defined(_WIN32)
    PWSTR path = nullptr;
    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path))) {
        backupDir = path;
        backupDir /= appName;
        backupDir /= "Backups";
        CoTaskMemFree(path);
    }
#elif defined(__APPLE__) || defined(__linux__)
    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        if (pw)
            homeDir = pw->pw_dir;
    }
    if (homeDir) {
        backupDir = homeDir;
        backupDir /= "Documents";
        backupDir /= appName;
        backupDir /= "Backups";
    }
#endif

    if (backupDir.empty()) {
        backupDir = std::string(".") + std::string(appName) + "/Backups";
    }

    return backupDir;
}

std::string BackupManager::generateBackupFilename(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::ostringstream ss;
    ss << prefix << "_";
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    ss << ".atombak";

    return ss.str();
}

}  // namespace atom::secret
