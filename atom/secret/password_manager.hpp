#ifndef ATOM_SECRET_PASSWORD_MANAGER_HPP
#define ATOM_SECRET_PASSWORD_MANAGER_HPP

#include "common.hpp"
#include "encryption.hpp"
#include "password_entry.hpp"
#include "storage.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Optional Boost support
#ifdef ATOM_USE_BOOST
#include <boost/container/flat_map.hpp>
#include <boost/lockfree/queue.hpp>
#endif

namespace atom::secret {

/**
 * @brief Class for securely managing passwords.
 *
 * The PasswordManager class provides methods to securely store, retrieve,
 * and delete passwords using platform-specific credential storage mechanisms
 * or an encrypted file fallback.
 */
class PasswordManager {
private:
    std::vector<unsigned char>
        masterKey;  ///< Derived master key for encryption operations.
    std::atomic<bool> isInitialized{
        false};  ///< Flag indicating if the manager is initialized.
    std::atomic<bool> isUnlocked{
        false};  ///< Flag indicating if the manager is unlocked.
    std::chrono::system_clock::time_point
        lastActivity;                  ///< Timestamp of the last user activity.
    PasswordManagerSettings settings;  ///< Manager configuration settings.
    std::unique_ptr<SecureStorage>
        storage;  ///< Platform-specific secure storage.

    // Thread safety protection
    mutable std::shared_mutex
        mutex;  ///< Read-write lock protecting cache access.

    // Cached password data, available when unlocked
#ifdef ATOM_USE_BOOST
    boost::container::flat_map<std::string, PasswordEntry>
        cachedPasswords;  ///< Password cache (Boost flat_map).
#else
    std::map<std::string, PasswordEntry>
        cachedPasswords;  ///< Password cache (std::map).
#endif

    // Activity callback function
    std::function<void()>
        activityCallback;  ///< Callback function invoked on activity.

public:
    /**
     * @brief Constructs a PasswordManager object.
     * Initializes OpenSSL.
     */
    PasswordManager();

    /**
     * @brief Destructor. Ensures secure cleanup of sensitive data and OpenSSL.
     */
    ~PasswordManager();

    // Disable copy construction and assignment
    PasswordManager(const PasswordManager&) = delete;
    PasswordManager& operator=(const PasswordManager&) = delete;

    // Enable move construction and assignment
    PasswordManager(PasswordManager&& other) noexcept;
    PasswordManager& operator=(PasswordManager&& other) noexcept;

    // Public methods from the original header...
    [[nodiscard]] bool initialize(
        std::string_view masterPassword,
        const PasswordManagerSettings& settings = PasswordManagerSettings());
    [[nodiscard]] bool unlock(std::string_view masterPassword);
    void lock() noexcept;
    [[nodiscard]] bool changeMasterPassword(std::string_view currentPassword,
                                            std::string_view newPassword);
    [[nodiscard]] bool loadAllPasswords();
    [[nodiscard]] bool storePassword(std::string_view platformKey,
                                     const PasswordEntry& entry);
    [[nodiscard]] bool storePassword(std::string_view platformKey,
                                     PasswordEntry&& entry);
    [[nodiscard]] std::optional<PasswordEntry> retrievePassword(
        std::string_view platformKey);
    [[nodiscard]] bool deletePassword(std::string_view platformKey);
    [[nodiscard]] std::vector<std::string> getAllPlatformKeys() const;
    [[nodiscard]] std::vector<std::string> searchPasswords(
        std::string_view query);
    [[nodiscard]] std::vector<std::string> filterByCategory(
        PasswordCategory category);
    [[nodiscard]] std::string generatePassword(int length = 16,
                                               bool includeSpecial = true,
                                               bool includeNumbers = true,
                                               bool includeMixedCase = true);
    [[nodiscard]] PasswordStrength evaluatePasswordStrength(
        std::string_view password) const;
    [[nodiscard]] bool exportPasswords(const std::filesystem::path& filePath,
                                       std::string_view password);
    [[nodiscard]] bool importPasswords(const std::filesystem::path& filePath,
                                       std::string_view password);
    void updateSettings(const PasswordManagerSettings& newSettings);
    [[nodiscard]] PasswordManagerSettings getSettings() const noexcept;
    [[nodiscard]] std::vector<std::string> checkExpiredPasswords();
    void setActivityCallback(std::function<void()> callback);
    [[nodiscard]] bool isLocked() const noexcept {
        return !isUnlocked.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool isReady() const noexcept {
        return isInitialized.load(std::memory_order_acquire);
    }

private:
    // Private methods from the original header...
    void updateActivity();
    [[nodiscard]] std::vector<unsigned char> deriveKey(
        std::string_view masterPassword, std::span<const unsigned char> salt,
        int iterations = 100000) const;
    template <typename T>
    static void secureWipe(T& data) noexcept;
    [[nodiscard]] std::string encryptEntry(
        const PasswordEntry& entry, std::span<const unsigned char> key) const;
    [[nodiscard]] PasswordEntry decryptEntry(
        std::string_view encryptedData,
        std::span<const unsigned char> key) const;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_MANAGER_HPP