#ifndef ATOM_SECRET_PASSWORD_HPP
#define ATOM_SECRET_PASSWORD_HPP

#include <openssl/err.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Optional Boost support
#ifdef ATOM_USE_BOOST
#include <boost/container/flat_map.hpp>
#include <boost/lockfree/queue.hpp>
#endif

namespace atom::secret {

/**
 * @brief Password strength levels.
 */
enum class PasswordStrength { VeryWeak, Weak, Medium, Strong, VeryStrong };

/**
 * @brief Password categories.
 */
enum class PasswordCategory {
    General,
    Finance,
    Work,
    Personal,
    Social,
    Entertainment,
    Other
};

/**
 * @brief Structure representing a password entry.
 */
struct PasswordEntry {
    std::string password;  ///< The stored password.
    std::string username;  ///< Associated username.
    std::string url;       ///< Associated URL.
    std::string notes;     ///< Additional notes.
    PasswordCategory category{
        PasswordCategory::General};                 ///< Password category.
    std::chrono::system_clock::time_point created;  ///< Creation timestamp.
    std::chrono::system_clock::time_point
        modified;  ///< Last modification timestamp.
    std::vector<std::string> previousPasswords;  ///< Password history.

    // Move constructor and assignment support
    PasswordEntry() = default;
    PasswordEntry(const PasswordEntry&) = default;
    PasswordEntry& operator=(const PasswordEntry&) = default;
    PasswordEntry(PasswordEntry&&) noexcept = default;
    PasswordEntry& operator=(PasswordEntry&&) noexcept = default;

    /**
     * @brief Checks if the entry is empty.
     * @return True if the entry is empty, false otherwise.
     */
    bool isEmpty() const noexcept {
        return password.empty() && username.empty() && url.empty() &&
               notes.empty() && previousPasswords.empty();
    }
};

/**
 * @brief Structure for encryption options.
 */
struct EncryptionOptions {
    bool useHardwareAcceleration =
        true;                    ///< Whether to use hardware acceleration.
    int keyIterations = 100000;  ///< PBKDF2 iteration count, increased default.

    /**
     * @brief Encryption method enumeration.
     */
    enum class Method : uint8_t {
        AES_GCM = 0,           ///< AES-GCM (Default, AEAD encryption).
        AES_CBC = 1,           ///< AES-CBC.
        CHACHA20_POLY1305 = 2  ///< ChaCha20-Poly1305.
    };

    Method encryptionMethod{
        Method::AES_GCM};  ///< The encryption method to use.
};

/**
 * @brief Settings for the Password Manager.
 */
struct PasswordManagerSettings {
    int autoLockTimeoutSeconds = 300;  ///< Auto-lock timeout in seconds.
    bool notifyOnPasswordExpiry =
        true;                     ///< Enable password expiry notifications.
    int passwordExpiryDays = 90;  ///< Password validity period in days.
    int minPasswordLength = 12;   ///< Minimum password length requirement.
    bool requireSpecialChars =
        true;                      ///< Require special characters in passwords.
    bool requireNumbers = true;    ///< Require numbers in passwords.
    bool requireMixedCase = true;  ///< Require mixed case letters in passwords.
    EncryptionOptions encryptionOptions;  ///< Encryption options.
};

/**
 * @brief Template for operation results, alternative to exceptions.
 * @tparam T The type of the successful result value.
 */
template <typename T>
class Result {
private:
    std::variant<T, std::string>
        data;  ///< Holds either the success value or an error string.

public:
    /**
     * @brief Constructs a Result with a success value (copy).
     * @param value The success value.
     */
    explicit Result(const T& value) : data(value) {}

    /**
     * @brief Constructs a Result with a success value (move).
     * @param value The success value (rvalue).
     */
    explicit Result(T&& value) noexcept : data(std::move(value)) {}

    /**
     * @brief Constructs a Result with an error message.
     * @param error The error message string.
     */
    explicit Result(const std::string& error) : data(error) {}

    /**
     * @brief Checks if the result represents success.
     * @return True if successful, false otherwise.
     */
    bool isSuccess() const noexcept { return std::holds_alternative<T>(data); }

    /**
     * @brief Checks if the result represents an error.
     * @return True if it's an error, false otherwise.
     */
    bool isError() const noexcept {
        return std::holds_alternative<std::string>(data);
    }

    /**
     * @brief Gets the success value (const lvalue ref).
     * @return A const reference to the success value.
     * @throws std::runtime_error if the result is an error.
     */
    const T& value() const& {
        if (isError())
            throw std::runtime_error(
                "Attempted to access value of an error Result: " +
                std::get<std::string>(data));
        return std::get<T>(data);
    }

    /**
     * @brief Gets the success value (rvalue ref).
     * @return An rvalue reference to the success value.
     * @throws std::runtime_error if the result is an error.
     */
    T&& value() && {
        if (isError())
            throw std::runtime_error(
                "Attempted to access value of an error Result: " +
                std::get<std::string>(data));
        return std::move(std::get<T>(data));
    }

    /**
     * @brief Gets the error message.
     * @return A const reference to the error message string.
     * @throws std::runtime_error if the result is successful.
     */
    const std::string& error() const {
        if (isSuccess())
            throw std::runtime_error(
                "Attempted to access error of a success Result.");
        return std::get<std::string>(data);
    }
};

// Forward declaration for OpenSSL context
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

/**
 * @brief RAII wrapper for OpenSSL EVP_CIPHER_CTX.
 * Ensures the context is properly freed.
 */
class SslCipherContext {
private:
    EVP_CIPHER_CTX* ctx;  ///< Pointer to the OpenSSL cipher context.

public:
    /**
     * @brief Constructs an SslCipherContext, creating a new EVP_CIPHER_CTX.
     * @throws std::runtime_error if context creation fails.
     */
    SslCipherContext();

    /**
     * @brief Destroys the SslCipherContext, freeing the EVP_CIPHER_CTX.
     */
    ~SslCipherContext();

    // Disable copy construction and assignment
    SslCipherContext(const SslCipherContext&) = delete;
    SslCipherContext& operator=(const SslCipherContext&) = delete;

    // Enable move construction and assignment
    SslCipherContext(SslCipherContext&& other) noexcept;
    SslCipherContext& operator=(SslCipherContext&& other) noexcept;

    /**
     * @brief Gets the raw pointer to the EVP_CIPHER_CTX.
     * @return The raw EVP_CIPHER_CTX pointer.
     */
    EVP_CIPHER_CTX* get() const noexcept { return ctx; }

    /**
     * @brief Implicit conversion to the raw EVP_CIPHER_CTX pointer.
     * @return The raw EVP_CIPHER_CTX pointer.
     */
    operator EVP_CIPHER_CTX*() const noexcept { return ctx; }
};

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

    /**
     * @brief Initializes the password manager with a master password.
     * Derives the master key and prepares the manager for use.
     * @param masterPassword The master password used to derive the encryption
     * key.
     * @param settings Optional settings for the password manager.
     * @return True if initialization was successful, false otherwise.
     */
    [[nodiscard]] bool initialize(
        std::string_view masterPassword,
        const PasswordManagerSettings& settings = PasswordManagerSettings());

    /**
     * @brief Unlocks the password manager using the master password.
     * Verifies the password and loads necessary data.
     * @param masterPassword The master password.
     * @return True if unlocking was successful, false otherwise.
     */
    [[nodiscard]] bool unlock(std::string_view masterPassword);

    /**
     * @brief Locks the password manager.
     * Clears sensitive data (like the master key and cache) from memory.
     */
    void lock() noexcept;

    /**
     * @brief Changes the master password.
     * Requires the current password for verification.
     * @param currentPassword The current master password.
     * @param newPassword The new master password.
     * @return True if the password change was successful, false otherwise.
     */
    [[nodiscard]] bool changeMasterPassword(std::string_view currentPassword,
                                            std::string_view newPassword);

    /**
     * @brief Loads all stored passwords into the cache.
     * Requires the manager to be unlocked.
     * @return True if loading was successful, false otherwise.
     */
    [[nodiscard]] bool loadAllPasswords();

    /**
     * @brief Stores a password entry.
     * Encrypts the entry and saves it using the platform storage or file
     * fallback.
     * @param platformKey A unique key identifying the password entry (e.g.,
     * URL, service name).
     * @param entry The PasswordEntry object to store (copied).
     * @return True if storage was successful, false otherwise.
     */
    [[nodiscard]] bool storePassword(std::string_view platformKey,
                                     const PasswordEntry& entry);

    /**
     * @brief Stores a password entry (move version).
     * Encrypts the entry and saves it using the platform storage or file
     * fallback.
     * @param platformKey A unique key identifying the password entry.
     * @param entry The PasswordEntry object to store (moved).
     * @return True if storage was successful, false otherwise.
     */
    [[nodiscard]] bool storePassword(std::string_view platformKey,
                                     PasswordEntry&& entry);

    /**
     * @brief Retrieves a password entry.
     * Fetches the encrypted data, decrypts it, and returns the entry. Checks
     * cache first.
     * @param platformKey The unique key identifying the password entry.
     * @return An std::optional containing the PasswordEntry if found, otherwise
     * std::nullopt.
     */
    [[nodiscard]] std::optional<PasswordEntry> retrievePassword(
        std::string_view platformKey);

    /**
     * @brief Deletes a password entry.
     * Removes the entry from the platform storage or file fallback and the
     * cache.
     * @param platformKey The unique key identifying the password entry.
     * @return True if deletion was successful, false otherwise.
     */
    [[nodiscard]] bool deletePassword(std::string_view platformKey);

    /**
     * @brief Gets a list of all stored platform keys.
     * @return A vector of strings containing all platform keys.
     */
    [[nodiscard]] std::vector<std::string> getAllPlatformKeys() const;

    /**
     * @brief Searches for password entries based on a query.
     * The search logic depends on the implementation (e.g., matching keys,
     * usernames, URLs).
     * @param query The search query string.
     * @return A vector of platform keys matching the query.
     */
    [[nodiscard]] std::vector<std::string> searchPasswords(
        std::string_view query);

    /**
     * @brief Filters password entries by category.
     * @param category The PasswordCategory to filter by.
     * @return A vector of platform keys belonging to the specified category.
     */
    [[nodiscard]] std::vector<std::string> filterByCategory(
        PasswordCategory category);

    /**
     * @brief Generates a strong random password.
     * @param length The desired length of the password (default 16).
     * @param includeSpecial Include special characters (default true).
     * @param includeNumbers Include numbers (default true).
     * @param includeMixedCase Include mixed-case letters (default true).
     * @return The generated password string.
     * @throws std::invalid_argument if length is too small.
     */
    [[nodiscard]] std::string generatePassword(int length = 16,
                                               bool includeSpecial = true,
                                               bool includeNumbers = true,
                                               bool includeMixedCase = true);

    /**
     * @brief Evaluates the strength of a given password.
     * @param password The password string to evaluate.
     * @return The evaluated PasswordStrength level.
     */
    [[nodiscard]] PasswordStrength evaluatePasswordStrength(
        std::string_view password) const;

    /**
     * @brief Exports all password data to an encrypted file.
     * The export format should be documented (e.g., encrypted JSON).
     * @param filePath The path to the file where the export will be saved.
     * @param password An additional password to encrypt the export file.
     * @return True if export was successful, false otherwise.
     */
    [[nodiscard]] bool exportPasswords(const std::filesystem::path& filePath,
                                       std::string_view password);

    /**
     * @brief Imports password data from an encrypted backup file.
     * Merges imported data with existing data (handling conflicts might be
     * needed).
     * @param filePath The path to the backup file to import.
     * @param password The password required to decrypt the backup file.
     * @return True if import was successful, false otherwise.
     */
    [[nodiscard]] bool importPasswords(const std::filesystem::path& filePath,
                                       std::string_view password);

    /**
     * @brief Updates the password manager settings.
     * @param newSettings The new PasswordManagerSettings object.
     */
    void updateSettings(const PasswordManagerSettings& newSettings);

    /**
     * @brief Gets the current password manager settings.
     * @return A copy of the current PasswordManagerSettings.
     */
    [[nodiscard]] PasswordManagerSettings getSettings() const noexcept;

    /**
     * @brief Checks for passwords that have expired based on settings.
     * Requires the manager to be unlocked.
     * @return A vector of platform keys for expired passwords. Returns empty if
     * notifications are disabled.
     */
    [[nodiscard]] std::vector<std::string> checkExpiredPasswords();

    /**
     * @brief Sets a callback function to be invoked on user activity.
     * Useful for resetting auto-lock timers in a GUI application.
     * @param callback The function to call on activity.
     */
    void setActivityCallback(std::function<void()> callback);

    /**
     * @brief Checks if the password manager is currently locked.
     * @return True if locked, false otherwise.
     */
    [[nodiscard]] bool isLocked() const noexcept {
        return !isUnlocked.load(std::memory_order_acquire);
    }

    /**
     * @brief Checks if the password manager has been initialized.
     * @return True if initialized, false otherwise.
     */
    [[nodiscard]] bool isReady() const noexcept {
        return isInitialized.load(std::memory_order_acquire);
    }

private:
    /**
     * @brief Updates the last activity timestamp and potentially triggers the
     * callback and auto-lock check.
     */
    void updateActivity();

    /**
     * @brief Internal implementation to get all platform keys without locking.
     * @return Vector of platform key strings.
     */
    [[nodiscard]] std::vector<std::string> getAllPlatformKeysInternal() const;

    /**
     * @brief Derives an encryption key from the master password using PBKDF2.
     * @param masterPassword The master password.
     * @param salt The salt value to use.
     * @param iterations The number of PBKDF2 iterations (default from settings
     * or constant).
     * @return The derived key as a vector of bytes.
     * @throws std::runtime_error on PBKDF2 failure.
     */
    [[nodiscard]] std::vector<unsigned char> deriveKey(
        std::string_view masterPassword, std::span<const unsigned char> salt,
        int iterations = 100000) const;  // Default iterations match constant

    /**
     * @brief Securely wipes sensitive data from memory.
     * Overwrites the memory region before deallocation/clearing.
     * @tparam T The type of the data container (e.g., std::vector,
     * std::string).
     * @param data The data container to wipe.
     */
    template <typename T>
    static void secureWipe(T& data) noexcept;

    /**
     * @brief Encrypts a PasswordEntry using the derived master key.
     * Serializes the entry (e.g., to JSON), then encrypts using AES-GCM (or
     * configured method).
     * @param entry The PasswordEntry to encrypt.
     * @param key The encryption key (derived from masterKey).
     * @return The encrypted data as a string (likely Base64 encoded JSON
     * containing IV, tag, ciphertext).
     * @throws std::runtime_error on serialization or encryption failure.
     */
    [[nodiscard]] std::string encryptEntry(
        const PasswordEntry& entry, std::span<const unsigned char> key) const;

    /**
     * @brief Decrypts data back into a PasswordEntry using the derived master
     * key. Parses the encrypted string, decrypts using AES-GCM (or configured
     * method), and deserializes.
     * @param encryptedData The encrypted data string.
     * @param key The decryption key (derived from masterKey).
     * @return The decrypted PasswordEntry.
     * @throws std::runtime_error on parsing, decryption, or deserialization
     * failure.
     */
    [[nodiscard]] PasswordEntry decryptEntry(
        std::string_view encryptedData,
        std::span<const unsigned char> key) const;

// Platform-specific storage methods
#if defined(_WIN32)
    /**
     * @brief Stores encrypted data in the Windows Credential Manager.
     * @param target The target name for the credential.
     * @param encryptedData The data to store.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool storeToWindowsCredentialManager(
        std::string_view target, std::string_view encryptedData) const;

    /**
     * @brief Retrieves encrypted data from the Windows Credential Manager.
     * @param target The target name of the credential.
     * @return The retrieved data string, or an empty string on failure/not
     * found.
     */
    [[nodiscard]] std::string retrieveFromWindowsCredentialManager(
        std::string_view target) const;

    /**
     * @brief Deletes a credential from the Windows Credential Manager.
     * @param target The target name of the credential.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool deleteFromWindowsCredentialManager(
        std::string_view target) const;

    /**
     * @brief Gets all credential target names associated with this application
     * from Windows Credential Manager.
     * @return A vector of target name strings.
     */
    [[nodiscard]] std::vector<std::string> getAllWindowsCredentials() const;

#elif defined(__APPLE__)
    /**
     * @brief Stores encrypted data in the macOS Keychain.
     * @param service The service name for the keychain item.
     * @param account The account name for the keychain item.
     * @param encryptedData The data to store.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool storeToMacKeychain(std::string_view service,
                                          std::string_view account,
                                          std::string_view encryptedData) const;

    /**
     * @brief Retrieves encrypted data from the macOS Keychain.
     * @param service The service name of the keychain item.
     * @param account The account name of the keychain item.
     * @return The retrieved data string, or an empty string on failure/not
     * found.
     */
    [[nodiscard]] std::string retrieveFromMacKeychain(
        std::string_view service, std::string_view account) const;

    /**
     * @brief Deletes an item from the macOS Keychain.
     * @param service The service name of the keychain item.
     * @param account The account name of the keychain item.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool deleteFromMacKeychain(std::string_view service,
                                             std::string_view account) const;

    /**
     * @brief Gets all account names for a given service from the macOS
     * Keychain.
     * @param service The service name to query.
     * @return A vector of account name strings.
     */
    [[nodiscard]] std::vector<std::string> getAllMacKeychainItems(
        std::string_view service) const;

#elif defined(__linux__)
    /**
     * @brief Stores encrypted data in the Linux Keyring (using libsecret or
     * fallback).
     * @param schema_name The schema name (used by libsecret).
     * @param attribute_name The attribute name (used as identifier).
     * @param encryptedData The data to store.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool storeToLinuxKeyring(
        std::string_view schema_name, std::string_view attribute_name,
        std::string_view encryptedData) const;

    /**
     * @brief Retrieves encrypted data from the Linux Keyring.
     * @param schema_name The schema name.
     * @param attribute_name The attribute name.
     * @return The retrieved data string, or an empty string on failure/not
     * found.
     */
    [[nodiscard]] std::string retrieveFromLinuxKeyring(
        std::string_view schema_name, std::string_view attribute_name) const;

    /**
     * @brief Deletes an item from the Linux Keyring.
     * @param schema_name The schema name.
     * @param attribute_name The attribute name.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool deleteFromLinuxKeyring(
        std::string_view schema_name, std::string_view attribute_name) const;

    /**
     * @brief Gets all attribute names for a given schema from the Linux
     * Keyring. Note: Enumeration might be limited depending on the backend
     * (e.g., libsecret).
     * @param schema_name The schema name to query.
     * @return A vector of attribute name strings.
     */
    [[nodiscard]] std::vector<std::string> getAllLinuxKeyringItems(
        std::string_view schema_name) const;
#endif

    /**
     * @brief Fallback: Stores encrypted data in a file within a secure
     * directory. Used when platform-specific storage is unavailable or fails.
     * @param identifier A unique identifier used as the filename (sanitized).
     * @param encryptedData The data to store.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool storeToEncryptedFile(
        std::string_view identifier, std::string_view encryptedData) const;

    /**
     * @brief Fallback: Retrieves encrypted data from a file.
     * @param identifier The identifier used as the filename.
     * @return The retrieved data string, or an empty string on failure/not
     * found.
     */
    [[nodiscard]] std::string retrieveFromEncryptedFile(
        std::string_view identifier) const;

    /**
     * @brief Fallback: Deletes an encrypted data file.
     * @param identifier The identifier used as the filename.
     * @return True on success, false on failure.
     */
    [[nodiscard]] bool deleteFromEncryptedFile(
        std::string_view identifier) const;

    /**
     * @brief Fallback: Gets all identifiers (filenames) from the encrypted file
     * storage directory.
     * @return A vector of identifier strings.
     */
    [[nodiscard]] std::vector<std::string> getAllEncryptedFileItems() const;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_HPP