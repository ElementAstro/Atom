#ifndef ATOM_SECRET_STORAGE_BACKUP_HPP
#define ATOM_SECRET_STORAGE_BACKUP_HPP

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Backup metadata.
 */
struct BackupInfo {
    std::string filename;                           ///< Backup filename
    std::filesystem::path path;                     ///< Full path to backup
    std::chrono::system_clock::time_point created;  ///< Creation time
    size_t size;                                    ///< Backup size in bytes
    std::string version;                            ///< Backup format version
    int entryCount;                                 ///< Number of entries
    bool encrypted;  ///< Whether backup is encrypted
};

/**
 * @brief Backup options.
 */
struct BackupOptions {
    bool encrypt = true;   ///< Encrypt the backup
    bool compress = true;  ///< Compress the backup
    std::string password;  ///< Encryption password (if different from master)
    bool includeSettings = true;  ///< Include settings in backup
    bool includeHistory = true;   ///< Include password history
    int maxBackups = 10;          ///< Maximum backups to keep (0 = unlimited)

    static BackupOptions defaults() { return BackupOptions{}; }
};

/**
 * @brief Restore options.
 */
struct RestoreOptions {
    std::string password;            ///< Decryption password
    bool overwriteExisting = false;  ///< Overwrite existing entries
    bool restoreSettings = true;     ///< Restore settings
    bool dryRun = false;             ///< Preview without actual restore

    static RestoreOptions defaults() { return RestoreOptions{}; }
};

/**
 * @brief Backup and restore functionality for password data.
 */
class BackupManager {
public:
    /**
     * @brief Creates a backup of all password data.
     * @param data Serialized password data.
     * @param backupDir Directory to store backup.
     * @param options Backup options.
     * @return Result containing backup info or error.
     */
    static Result<BackupInfo> createBackup(
        const std::vector<uint8_t>& data,
        const std::filesystem::path& backupDir,
        const BackupOptions& options = BackupOptions::defaults());

    /**
     * @brief Restores password data from a backup.
     * @param backupPath Path to backup file.
     * @param options Restore options.
     * @return Result containing restored data or error.
     */
    static Result<std::vector<uint8_t>> restoreBackup(
        const std::filesystem::path& backupPath,
        const RestoreOptions& options = RestoreOptions::defaults());

    /**
     * @brief Lists all available backups.
     * @param backupDir Directory containing backups.
     * @return Result containing list of backup info or error.
     */
    static Result<std::vector<BackupInfo>> listBackups(
        const std::filesystem::path& backupDir);

    /**
     * @brief Deletes a backup.
     * @param backupPath Path to backup file.
     * @return Result indicating success or error.
     */
    static Result<void> deleteBackup(const std::filesystem::path& backupPath);

    /**
     * @brief Cleans up old backups, keeping only the most recent.
     * @param backupDir Directory containing backups.
     * @param keepCount Number of backups to keep.
     * @return Result containing number of deleted backups or error.
     */
    static Result<int> cleanupOldBackups(const std::filesystem::path& backupDir,
                                         int keepCount);

    /**
     * @brief Verifies a backup file integrity.
     * @param backupPath Path to backup file.
     * @param password Password if encrypted.
     * @return Result containing backup info if valid, or error.
     */
    static Result<BackupInfo> verifyBackup(
        const std::filesystem::path& backupPath,
        const std::string& password = "");

    /**
     * @brief Gets the default backup directory.
     * @param appName Application name.
     * @return Default backup directory path.
     */
    static std::filesystem::path getDefaultBackupDir(std::string_view appName);

    /**
     * @brief Generates a backup filename with timestamp.
     * @param prefix Filename prefix.
     * @return Generated filename.
     */
    static std::string generateBackupFilename(
        const std::string& prefix = "backup");

private:
    static constexpr uint32_t BACKUP_MAGIC = 0x41544F4D;  // "ATOM"
    static constexpr uint8_t BACKUP_VERSION = 1;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_STORAGE_BACKUP_HPP
