#ifndef ATOM_SECRET_STORAGE_STORAGE_HPP
#define ATOM_SECRET_STORAGE_STORAGE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Storage backend type.
 */
enum class StorageBackend {
    Auto,    ///< Automatically select best available backend
    System,  ///< System keychain (Windows Credential/macOS Keychain/Linux
             ///< Secret Service)
    File,    ///< Encrypted file storage
    Memory   ///< In-memory storage (for testing)
};

/**
 * @brief Storage options.
 */
struct StorageOptions {
    StorageBackend backend = StorageBackend::Auto;
    std::string appName = "AtomSecret";  ///< Application name for storage
    std::string storagePath;             ///< Custom path for file storage
    bool createIfNotExists = true;       ///< Create storage if it doesn't exist

    static StorageOptions defaults() { return StorageOptions{}; }
};

/**
 * @brief Interface for platform-specific secure storage.
 */
class SecureStorage {
public:
    virtual ~SecureStorage() = default;

    /**
     * @brief Stores data in the secure storage.
     * @param key The key/identifier for the data.
     * @param data The data to store.
     * @return Result indicating success or error.
     */
    virtual Result<void> store(std::string_view key, std::string_view data) = 0;

    /**
     * @brief Stores binary data in the secure storage.
     * @param key The key/identifier for the data.
     * @param data The binary data to store.
     * @return Result indicating success or error.
     */
    virtual Result<void> store(std::string_view key,
                               const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Retrieves data from the secure storage.
     * @param key The key/identifier for the data.
     * @return Result containing the data or error.
     */
    virtual Result<std::string> retrieve(std::string_view key) = 0;

    /**
     * @brief Retrieves binary data from the secure storage.
     * @param key The key/identifier for the data.
     * @return Result containing the binary data or error.
     */
    virtual Result<std::vector<uint8_t>> retrieveBytes(
        std::string_view key) = 0;

    /**
     * @brief Deletes data from the secure storage.
     * @param key The key/identifier for the data to delete.
     * @return Result indicating success or error.
     */
    virtual Result<void> remove(std::string_view key) = 0;

    /**
     * @brief Checks if a key exists in storage.
     * @param key The key to check.
     * @return True if key exists.
     */
    virtual bool exists(std::string_view key) = 0;

    /**
     * @brief Gets all keys in the storage.
     * @return Result containing vector of keys or error.
     */
    virtual Result<std::vector<std::string>> getAllKeys() = 0;

    /**
     * @brief Clears all data from storage.
     * @return Result indicating success or error.
     */
    virtual Result<void> clear() = 0;

    /**
     * @brief Gets the storage backend type.
     * @return Storage backend type.
     */
    virtual StorageBackend getBackend() const noexcept = 0;

    /**
     * @brief Gets the storage location/path.
     * @return Storage location description.
     */
    virtual std::string getLocation() const = 0;

    /**
     * @brief Creates a platform-appropriate storage instance.
     * @param options Storage options.
     * @return Unique pointer to storage instance.
     */
    static std::unique_ptr<SecureStorage> create(
        const StorageOptions& options = StorageOptions::defaults());

    /**
     * @brief Checks if a specific backend is available.
     * @param backend Backend to check.
     * @return True if available.
     */
    static bool isBackendAvailable(StorageBackend backend) noexcept;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_STORAGE_STORAGE_HPP
