#ifndef ATOM_SECRET_STORAGE_FILE_STORAGE_HPP
#define ATOM_SECRET_STORAGE_FILE_STORAGE_HPP

#include <filesystem>
#include <string>

#include "storage.hpp"

namespace atom::secret {

/**
 * @brief File-based secure storage implementation.
 *
 * Stores data in encrypted files on the filesystem.
 */
class FileStorage : public SecureStorage {
public:
    /**
     * @brief Constructs a FileStorage instance.
     * @param appName Application name for storage directory.
     * @param customPath Optional custom storage path.
     */
    explicit FileStorage(std::string_view appName,
                         const std::string& customPath = "");

    ~FileStorage() override = default;

    Result<void> store(std::string_view key, std::string_view data) override;
    Result<void> store(std::string_view key,
                       const std::vector<uint8_t>& data) override;
    Result<std::string> retrieve(std::string_view key) override;
    Result<std::vector<uint8_t>> retrieveBytes(std::string_view key) override;
    Result<void> remove(std::string_view key) override;
    bool exists(std::string_view key) override;
    Result<std::vector<std::string>> getAllKeys() override;
    Result<void> clear() override;
    StorageBackend getBackend() const noexcept override {
        return StorageBackend::File;
    }
    std::string getLocation() const override { return storageDir_.string(); }

private:
    std::string appName_;
    std::filesystem::path storageDir_;

    /**
     * @brief Gets the file path for a key.
     * @param key Storage key.
     * @return File path.
     */
    std::filesystem::path getFilePath(std::string_view key) const;

    /**
     * @brief Sanitizes a key for use as filename.
     * @param key Key to sanitize.
     * @return Sanitized filename.
     */
    static std::string sanitizeKey(std::string_view key);

    /**
     * @brief Updates the index file.
     * @param key Key to add or remove.
     * @param add True to add, false to remove.
     */
    void updateIndex(const std::string& key, bool add);

    /**
     * @brief Gets the storage directory path.
     * @param appName Application name.
     * @param customPath Custom path (optional).
     * @return Storage directory path.
     */
    static std::filesystem::path getStorageDirectory(
        std::string_view appName, const std::string& customPath);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_STORAGE_FILE_STORAGE_HPP
