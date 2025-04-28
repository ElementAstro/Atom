#ifndef ATOM_SECRET_STORAGE_HPP
#define ATOM_SECRET_STORAGE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace atom::secret {

/**
 * @brief Interface for platform-specific secure storage.
 */
class SecureStorage {
public:
    virtual ~SecureStorage() = default;

    /**
     * @brief Stores encrypted data in the platform's secure storage.
     * @param key The key or identifier for the data.
     * @param data The encrypted data to store.
     * @return True on success, false otherwise.
     */
    virtual bool store(std::string_view key, std::string_view data) const = 0;

    /**
     * @brief Retrieves encrypted data from the platform's secure storage.
     * @param key The key or identifier for the data.
     * @return The retrieved data or an empty string if not found/error.
     */
    virtual std::string retrieve(std::string_view key) const = 0;

    /**
     * @brief Deletes data from the platform's secure storage.
     * @param key The key or identifier for the data to delete.
     * @return True on success, false otherwise.
     */
    virtual bool remove(std::string_view key) const = 0;

    /**
     * @brief Gets all keys/identifiers stored in the platform's secure storage.
     * @return A vector of key strings.
     */
    virtual std::vector<std::string> getAllKeys() const = 0;

    /**
     * @brief Creates and returns a platform-appropriate instance of
     * SecureStorage.
     * @param appName The application name for storage categorization.
     * @return A unique_ptr to a SecureStorage instance.
     */
    static std::unique_ptr<SecureStorage> create(std::string_view appName);
};

#if defined(_WIN32)
class WindowsSecureStorage;
#elif defined(__APPLE__)
class MacSecureStorage;
#elif defined(__linux__)
class LinuxSecureStorage;
#endif
class FileSecureStorage;

}  // namespace atom::secret

#endif  // ATOM_SECRET_STORAGE_HPP