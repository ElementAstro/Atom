#include "password.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>  // For std::istreambuf_iterator
#include <memory>    // For std::unique_ptr
#include <random>    // For password generation (alternative to OpenSSL RAND)
#include <regex>
#include <stdexcept>
#include <system_error>  // For filesystem errors

#include <openssl/err.h>  // For error reporting
#include <openssl/evp.h>
#include <openssl/kdf.h>  // For PBKDF2
#include <openssl/rand.h>

// Platform-specific includes
#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <wincred.h>
#include <shlobj.h> // For SHGetKnownFolderPath
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")  // Needed for SHGetKnownFolderPath
#endif
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <sys/stat.h>  // For mkdir
#elif defined(__linux__)
#include <pwd.h>       // For getpwuid, getuid
#include <sys/stat.h>  // For mkdir
#include <unistd.h>    // For getuid
#if __has_include(<libsecret/secret.h>)
#include <libsecret/secret.h>
#define USE_LIBSECRET 1
#else
// Fallback or other keyring options can be added here
#warning "libsecret not found, using file fallback for Linux keyring."
#define USE_FILE_FALLBACK 1  // Explicitly define for clarity
#endif
#else
#warning "Unsupported platform, using file fallback."
#define USE_FILE_FALLBACK 1  // Explicitly define for clarity
#endif

#include "atom/algorithm/base.hpp"  // Assuming this contains base64
#include "atom/error/exception.hpp"  // Assuming THROW_RUNTIME_ERROR is defined here
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

using json = nlohmann::json;

// --- Constants ---
namespace {
constexpr std::string_view ATOM_PM_VERSION = "2.1.0";  // Updated version
constexpr std::string_view ATOM_PM_SERVICE_NAME = "AtomPasswordManager";
constexpr std::string_view ATOM_PM_INIT_KEY =
    "ATOM_PM_INIT_DATA_V2";  // Use a versioned key
constexpr std::string_view ATOM_PM_INDEX_KEY = "ATOM_PM_INDEX_V2";
constexpr size_t ATOM_PM_SALT_SIZE = 16;
constexpr size_t ATOM_PM_IV_SIZE = 12;   // AES-GCM standard IV size is 12 bytes
constexpr size_t ATOM_PM_TAG_SIZE = 16;  // AES-GCM standard tag size
constexpr int DEFAULT_PBKDF2_ITERATIONS =
    100000;  // Default, overridden by settings
constexpr std::string_view VERIFICATION_PREFIX = "ATOM_PM_VERIFICATION_";
const std::string VERIFICATION_STRING =
    std::string(VERIFICATION_PREFIX).append(ATOM_PM_VERSION);

// Helper for OpenSSL errors
std::string getOpenSSLError() {
    unsigned long errCode = ERR_get_error();
    if (errCode == 0) {
        return "No OpenSSL error reported.";
    }
    char errBuf[256];
    ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
    return std::string(errBuf);
}

// Helper to get secure storage directory for file fallback
std::filesystem::path getSecureStorageDirectory() {
    std::filesystem::path storageDir;
#if defined(_WIN32)
    PWSTR path = nullptr;
    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        storageDir = path;
        storageDir /= "AtomPasswordManager";
        CoTaskMemFree(path);
    } else {
        // Fallback if SHGetKnownFolderPath fails
        char* appDataPath = nullptr;
        size_t pathLen;
        _dupenv_s(&appDataPath, &pathLen, "LOCALAPPDATA");
        if (appDataPath) {
            storageDir = appDataPath;
            storageDir /= "AtomPasswordManager";
            free(appDataPath);
        } else {
            storageDir =
                ".AtomPasswordManager";  // Current directory as last resort
            LOG_F(WARNING,
                  "Could not determine LocalAppData path, using current "
                  "directory.");
        }
    }
#elif defined(__APPLE__) || defined(__linux__)
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            homeDir = pw->pw_dir;
        }
    }
    if (homeDir) {
        storageDir = homeDir;
#if defined(__APPLE__)
        storageDir /= "Library/Application Support/AtomPasswordManager";
#else  // Linux
        storageDir /= ".local/share/AtomPasswordManager";
#endif
    } else {
        storageDir =
            ".AtomPasswordManager";  // Current directory as last resort
        LOG_F(WARNING,
              "Could not determine HOME directory, using current directory.");
    }
#else
    storageDir =
        ".AtomPasswordManager";  // Current directory for unknown platforms
    LOG_F(WARNING, "Unknown platform, using current directory for storage.");
#endif

    // Create directory if it doesn't exist
    std::error_code ec;
    if (!std::filesystem::exists(storageDir, ec) && !ec) {
        if (!std::filesystem::create_directories(storageDir, ec) && ec) {
            LOG_F(ERROR, "Failed to create storage directory '{}': {}",
                  storageDir.string().c_str(), ec.message().c_str());
            // Depending on requirements, could throw or return an invalid path
        }
#if !defined(_WIN32)  // Set permissions on Unix-like systems
        chmod(storageDir.c_str(), 0700);
#endif
    } else if (ec) {
        LOG_F(ERROR, "Failed to check existence of storage directory '{}': {}",
              storageDir.string().c_str(), ec.message().c_str());
    }

    return storageDir;
}

// Helper to sanitize identifier for use as filename
std::string sanitizeIdentifier(std::string_view identifier) {
    std::string sanitized;
    sanitized.reserve(identifier.length());
    for (char c : identifier) {
        if (std::isalnum(c) || c == '-' || c == '_') {
            sanitized += c;
        } else {
            sanitized += '_';  // Replace invalid characters
        }
    }
    // Prevent excessively long filenames
    if (sanitized.length() > 100) {
        sanitized.resize(100);
    }
    return sanitized;
}

}  // Anonymous namespace

namespace atom::secret {

// --- SslCipherContext Implementation ---
SslCipherContext::SslCipherContext() : ctx(EVP_CIPHER_CTX_new()) {
    if (!ctx) {
        throw std::runtime_error("Failed to create OpenSSL cipher context: " +
                                 getOpenSSLError());
    }
}

SslCipherContext::~SslCipherContext() {
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
        // ctx = nullptr; // No need, object is being destroyed
    }
}

SslCipherContext::SslCipherContext(SslCipherContext&& other) noexcept
    : ctx(other.ctx) {
    other.ctx = nullptr;  // Take ownership
}

SslCipherContext& SslCipherContext::operator=(
    SslCipherContext&& other) noexcept {
    if (this != &other) {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);  // Free existing context
        }
        ctx = other.ctx;
        other.ctx = nullptr;  // Take ownership
    }
    return *this;
}

// --- PasswordManager Implementation ---

PasswordManager::PasswordManager()
    : lastActivity(std::chrono::system_clock::now()) {
    // OpenSSL initialization is recommended to be done once per application
    // lifetime. Consider moving this to a global scope or ensuring it's called
    // safely. For simplicity here, we assume it's safe to call multiple times
    // or handled externally. OpenSSL_add_all_algorithms(); // Deprecated in
    // OpenSSL 1.1.0+ ERR_load_crypto_strings();    // Deprecated in
    // OpenSSL 1.1.0+ OpenSSL 1.1.0+ initializes itself automatically.
    LOG_F(INFO, "PasswordManager instance created (API version {})",
          ATOM_PM_VERSION.data());
}

PasswordManager::~PasswordManager() {
    try {
        lock();  // Ensure data is wiped on destruction
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during PasswordManager destruction: {}", e.what());
        // Avoid throwing from destructor
    } catch (...) {
        LOG_F(ERROR, "Unknown error during PasswordManager destruction");
        // Avoid throwing from destructor
    }

    // OpenSSL cleanup is also generally handled automatically in 1.1.0+
    // EVP_cleanup();       // Deprecated in OpenSSL 1.1.0+
    // ERR_free_strings();  // Deprecated in OpenSSL 1.1.0+
    LOG_F(INFO, "PasswordManager instance destroyed safely");
}

// Move constructor
PasswordManager::PasswordManager(PasswordManager&& other) noexcept
    : masterKey(std::move(other.masterKey)),
      isInitialized(other.isInitialized.load()),
      isUnlocked(other.isUnlocked.load()),
      lastActivity(other.lastActivity),
      settings(std::move(other.settings)),
      // Mutex cannot be moved, create a new one. Lock state is implicitly
      // transferred.
      cachedPasswords(
          std::move(other.cachedPasswords)),  // Requires map's move constructor
      activityCallback(std::move(other.activityCallback)) {
    // Reset moved-from state
    other.isInitialized.store(false);
    other.isUnlocked.store(false);
    // other.masterKey is already moved-from (likely empty)
    // other.cachedPasswords is already moved-from
}

// Move assignment
PasswordManager& PasswordManager::operator=(PasswordManager&& other) noexcept {
    if (this != &other) {
        // Lock both mutexes to prevent deadlocks (using std::lock or
        // scoped_lock) However, direct mutex move isn't possible. We need to
        // manage state transfer carefully. Simplest approach: lock self, clear
        // state, move data, unlock.
        std::unique_lock lock_this(mutex, std::defer_lock);
        std::unique_lock lock_other(other.mutex, std::defer_lock);
        std::lock(lock_this, lock_other);  // Lock both

        // Clear current state safely
        secureWipe(masterKey);
        cachedPasswords.clear();  // Assuming PasswordEntry wipe is handled
                                  // elsewhere or not needed here

        // Move data
        masterKey = std::move(other.masterKey);
        isInitialized.store(other.isInitialized.load());
        isUnlocked.store(other.isUnlocked.load());
        lastActivity = other.lastActivity;
        settings = std::move(other.settings);
        cachedPasswords = std::move(other.cachedPasswords);
        activityCallback = std::move(other.activityCallback);

        // Reset moved-from state
        other.isInitialized.store(false);
        other.isUnlocked.store(false);
        // other.masterKey is moved-from
        // other.cachedPasswords is moved-from
    }
    return *this;
}

bool PasswordManager::initialize(std::string_view masterPassword,
                                 const PasswordManagerSettings& newSettings) {
    std::unique_lock lock(mutex);  // Exclusive lock for initialization

    if (isInitialized.load(std::memory_order_acquire)) {
        LOG_F(WARNING, "PasswordManager already initialized.");
        return true;  // Or false, depending on desired behavior
    }

    if (masterPassword.empty()) {
        LOG_F(ERROR, "Cannot initialize with empty master password");
        return false;
    }

    this->settings = newSettings;  // Copy settings

    // Generate random salt
    std::vector<unsigned char> salt(ATOM_PM_SALT_SIZE);
    if (RAND_bytes(salt.data(), salt.size()) != 1) {
        LOG_F(ERROR, "Failed to generate random salt: {}",
              getOpenSSLError().c_str());
        return false;
    }

    // Derive key from master password
    try {
        masterKey = deriveKey(masterPassword, salt,
                              settings.encryptionOptions.keyIterations);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to derive key during initialization: {}",
              e.what());
        return false;
    }

    // Encrypt verification data
    std::string encryptedVerificationData;
    std::vector<unsigned char> iv(ATOM_PM_IV_SIZE);
    std::vector<unsigned char> tag(ATOM_PM_TAG_SIZE);

    try {
        if (RAND_bytes(iv.data(), iv.size()) != 1) {
            throw std::runtime_error(
                "Failed to generate random IV for verification data: " +
                getOpenSSLError());
        }

        SslCipherContext ctx;  // RAII

        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                               masterKey.data(), iv.data()) != 1) {
            throw std::runtime_error(
                "Failed to initialize encryption for verification data: " +
                getOpenSSLError());
        }

        std::vector<unsigned char> encryptedDataBuffer(
            VERIFICATION_STRING.size() +
            EVP_MAX_BLOCK_LENGTH);  // Max possible size
        int len = 0;
        if (EVP_EncryptUpdate(ctx.get(), encryptedDataBuffer.data(), &len,
                              reinterpret_cast<const unsigned char*>(
                                  VERIFICATION_STRING.data()),
                              VERIFICATION_STRING.size()) != 1) {
            throw std::runtime_error("Failed to encrypt verification data: " +
                                     getOpenSSLError());
        }
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedDataBuffer.data() + len,
                                &finalLen) != 1) {
            throw std::runtime_error(
                "Failed to finalize encryption for verification data: " +
                getOpenSSLError());
        }
        len += finalLen;
        encryptedDataBuffer.resize(len);  // Resize to actual encrypted size

        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(),
                                tag.data()) != 1) {
            throw std::runtime_error(
                "Failed to get authentication tag for verification data: " +
                getOpenSSLError());
        }

        // Base64 encode components for storage
        std::vector<unsigned char> dataB64 =
            algorithm::base64Encode(encryptedDataBuffer);
        encryptedVerificationData = std::string(dataB64.begin(), dataB64.end());

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Encryption error during initialization: {}", e.what());
        secureWipe(masterKey);  // Wipe key on failure
        return false;
    }

    // Build JSON structure for initialization data
    nlohmann::json initData;
    initData["version"] = ATOM_PM_VERSION;
    initData["iterations"] =
        settings.encryptionOptions.keyIterations;  // Store iterations used

    auto saltB64 = algorithm::base64Encode(std::string(salt.begin(), salt.end()));
    auto ivB64 = algorithm::base64Encode(std::string(iv.begin(), iv.end()));
    auto tagB64 = algorithm::base64Encode(std::string(tag.begin(), tag.end()));

    if (!saltB64 || !ivB64 || !tagB64) {
        throw std::runtime_error(
            "Failed to base64 encode initialization components.");
    }

    initData["salt"] = *saltB64;
    initData["iv"] = *ivB64;
    initData["tag"] = *tagB64;
    initData["data"] = encryptedVerificationData;  // Already base64 encoded

    std::string serializedData = initData.dump();

    // Store initialization data using platform mechanism or fallback
    bool stored = false;
#if defined(_WIN32)
    stored = storeToWindowsCredentialManager(ATOM_PM_INIT_KEY, serializedData);
#elif defined(__APPLE__)
    stored = storeToMacKeychain(ATOM_PM_SERVICE_NAME, ATOM_PM_INIT_KEY,
                                serializedData);
#elif defined(__linux__) && defined(USE_LIBSECRET)
    stored = storeToLinuxKeyring(ATOM_PM_SERVICE_NAME, ATOM_PM_INIT_KEY,
                                 serializedData);
#else  // File fallback
    stored = storeToEncryptedFile(ATOM_PM_INIT_KEY, serializedData);
#endif

    if (!stored) {
        LOG_F(ERROR, "Failed to store initialization data.");
        secureWipe(masterKey);
        return false;
    }

    isInitialized.store(true, std::memory_order_release);
    isUnlocked.store(true, std::memory_order_release);
    lastActivity = std::chrono::system_clock::now();  // Update activity time
    LOG_F(INFO, "PasswordManager successfully initialized");
    return true;
}

bool PasswordManager::unlock(std::string_view masterPassword) {
    // Use a read lock first to check if already unlocked
    {
        std::shared_lock read_lock(mutex);
        if (isUnlocked.load(std::memory_order_acquire)) {
            LOG_F(INFO, "PasswordManager is already unlocked");
            // Optionally update activity? Depends on requirements.
            // updateActivity(); // Requires unique_lock if it modifies state
            return true;
        }
    }  // Read lock released

    // Acquire unique lock for the unlock process
    std::unique_lock write_lock(mutex);

    // Double-check unlock status after acquiring write lock
    if (isUnlocked.load(std::memory_order_acquire)) {
        LOG_F(INFO, "PasswordManager was unlocked concurrently");
        return true;
    }

    if (masterPassword.empty()) {
        LOG_F(ERROR, "Empty master password provided for unlock");
        return false;
    }

    // Retrieve initialization data
    std::string serializedData;
#if defined(_WIN32)
    serializedData = retrieveFromWindowsCredentialManager(ATOM_PM_INIT_KEY);
#elif defined(__APPLE__)
    serializedData =
        retrieveFromMacKeychain(ATOM_PM_SERVICE_NAME, ATOM_PM_INIT_KEY);
#elif defined(__linux__) && defined(USE_LIBSECRET)
    serializedData =
        retrieveFromLinuxKeyring(ATOM_PM_SERVICE_NAME, ATOM_PM_INIT_KEY);
#else  // File fallback
    serializedData = retrieveFromEncryptedFile(ATOM_PM_INIT_KEY);
#endif

    if (serializedData.empty()) {
        LOG_F(ERROR,
              "No initialization data found. Manager not initialized or data "
              "inaccessible.");
        return false;
    }

    // Parse initialization data
    std::vector<unsigned char> salt, iv, tag, encryptedDataBytes;
    int iterations = DEFAULT_PBKDF2_ITERATIONS;  // Default if not found
    try {
        // Use fully qualified name for json
        nlohmann::json_abi_v3_11_3::json initData =
            nlohmann::json_abi_v3_11_3::json::parse(serializedData);

        // Check version compatibility if needed
        if (initData.contains("version")) {
            std::string storedVersion = initData["version"];
            // Add version comparison logic here if necessary
            LOG_F(INFO, "Stored data version: {}", storedVersion.c_str());
        }

        if (initData.contains("iterations")) {
            iterations = initData["iterations"].template get<int>();
        } else {
            LOG_F(WARNING,
                  "Iterations not found in init data, using default: {}",
                  DEFAULT_PBKDF2_ITERATIONS);
        }

        auto saltResult = algorithm::base64Decode(initData.at("salt").get<std::string>());
        auto ivResult = algorithm::base64Decode(initData.at("iv").get<std::string>());
        auto tagResult = algorithm::base64Decode(initData.at("tag").get<std::string>());
        auto dataResult = algorithm::base64Decode(initData.at("data").get<std::string>());

        if (!saltResult || !ivResult || !tagResult || !dataResult) {
            throw std::runtime_error(
                "Failed to decode base64 components from init data.");
        }

        salt = std::vector<unsigned char>(saltResult->begin(), saltResult->end());
        iv = std::vector<unsigned char>(ivResult->begin(), ivResult->end());
        tag = std::vector<unsigned char>(tagResult->begin(), tagResult->end());
        encryptedDataBytes = std::vector<unsigned char>(dataResult->begin(), dataResult->end());

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to parse initialization data: {}", e.what());
        return false;
    }

    // Derive key from provided master password
    std::vector<unsigned char> derivedKey;
    try {
        derivedKey = deriveKey(masterPassword, salt, iterations);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to derive key during unlock: {}", e.what());
        return false;  // Don't leak timing info by trying decryption
    }

    // Attempt to decrypt verification data
    std::vector<unsigned char> decryptedData(
        encryptedDataBytes.size());  // Initial size guess
    int len = 0;
    bool verificationSuccess = false;
    try {
        SslCipherContext ctx;  // RAII

        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                               derivedKey.data(), iv.data()) != 1) {
            throw std::runtime_error("Failed to initialize decryption: " +
                                     getOpenSSLError());
        }

        // Set the expected tag *before* finalization
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(),
                                tag.data()) != 1) {
            throw std::runtime_error("Failed to set authentication tag: " +
                                     getOpenSSLError());
        }

        if (EVP_DecryptUpdate(ctx.get(), decryptedData.data(), &len,
                              encryptedDataBytes.data(),
                              encryptedDataBytes.size()) != 1) {
            // This might fail legitimately if the key is wrong, but GCM often
            // detects errors at finalization
            throw std::runtime_error("Failed to decrypt verification data: " +
                                     getOpenSSLError());
        }

        int finalLen = 0;
        // EVP_DecryptFinal_ex returns 1 for success, 0 for verification failure
        // (tag mismatch), <0 for other errors
        int ret = EVP_DecryptFinal_ex(ctx.get(), decryptedData.data() + len,
                                      &finalLen);

        if (ret > 0) {  // Success
            len += finalLen;
            decryptedData.resize(len);
            std::string verificationStr(decryptedData.begin(),
                                        decryptedData.end());
            // Constant-time comparison might be slightly better, but less
            // critical here
            if (verificationStr == VERIFICATION_STRING) {
                verificationSuccess = true;
            } else {
                LOG_F(ERROR, "Verification data mismatch after decryption.");
            }
        } else if (ret == 0) {  // Verification failed (tag mismatch) -
                                // Indicates wrong password
            LOG_F(WARNING,
                  "Authentication failed - incorrect master password.");
            // No need to throw, verificationSuccess remains false
        } else {  // Other decryption error
            throw std::runtime_error("Decryption finalization failed: " +
                                     getOpenSSLError());
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Decryption error during unlock: {}", e.what());
        secureWipe(derivedKey);  // Wipe derived key on error
        return false;
    }

    if (!verificationSuccess) {
        secureWipe(derivedKey);  // Wipe derived key if verification failed
        return false;
    }

    // Unlock successful
    masterKey = std::move(derivedKey);  // Store the verified key
    isUnlocked.store(true, std::memory_order_release);
    isInitialized.store(
        true,
        std::memory_order_release);  // Mark as initialized if unlock succeeds
    lastActivity = std::chrono::system_clock::now();

    // Load passwords into cache (consider doing this lazily or in background)
    if (!loadAllPasswords()) {  // loadAllPasswords needs to handle locking
                                // internally or be called under lock
        LOG_F(WARNING, "Failed to load all passwords into cache after unlock.");
        // Decide if this is a critical failure
    }

    LOG_F(INFO, "PasswordManager successfully unlocked");
    return true;
}

void PasswordManager::lock() noexcept {
    std::unique_lock lock(mutex);  // Exclusive lock

    if (!isUnlocked.load(std::memory_order_relaxed)) {  // Relaxed is fine,
                                                        // protected by mutex
        return;
    }

    // Clear cache, securely wiping sensitive parts
    for (auto& [key, entry] : cachedPasswords) {
        secureWipe(entry.password);
        for (auto& prev : entry.previousPasswords) {
            secureWipe(prev);
        }
    }
    cachedPasswords.clear();

    // Clear master key
    secureWipe(masterKey);

    isUnlocked.store(
        false,
        std::memory_order_release);  // Release ensures writes are visible
    LOG_F(INFO, "PasswordManager locked");
}

bool PasswordManager::changeMasterPassword(std::string_view currentPassword,
                                           std::string_view newPassword) {
    // 1. Verify current password and unlock (acquires unique lock)
    // We need the lock for the entire operation. Unlock will acquire it.
    if (!unlock(currentPassword)) {
        LOG_F(ERROR,
              "Failed to change master password: Current password verification "
              "failed.");
        return false;
    }
    // Now we hold the unique lock from unlock()

    if (newPassword.empty()) {
        LOG_F(ERROR, "New master password cannot be empty.");
        // No need to re-lock, just return
        return false;
    }
    if (newPassword == currentPassword) {
        LOG_F(WARNING, "New master password is the same as the current one.");
        // No need to re-lock, just return
        return true;  // Or false, depending on desired behavior
    }

    LOG_F(INFO, "Starting master password change process...");

    // 2. Retrieve all current entries (already holding lock, cache should be
    // populated by unlock) Create a temporary copy of the cache
    auto currentEntries = cachedPasswords;  // Copy constructor

    // 3. Generate new salt
    std::vector<unsigned char> newSalt(ATOM_PM_SALT_SIZE);
    if (RAND_bytes(newSalt.data(), newSalt.size()) != 1) {
        LOG_F(ERROR, "Failed to generate new salt for password change: {}",
              getOpenSSLError().c_str());
        // Don't leave in unlocked state with old key if we can't proceed
        // Consider how to handle partial failure - maybe lock() and return
        // false? For now, log and return false. State remains unlocked with old
        // key.
        return false;
    }

    // 4. Derive new master key
    std::vector<unsigned char> newMasterKey;
    try {
        newMasterKey = deriveKey(newPassword, newSalt,
                                 settings.encryptionOptions.keyIterations);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to derive new master key: {}", e.what());
        // State remains unlocked with old key.
        return false;
    }

    auto encodeAndConvert =
        [](const std::vector<unsigned char>& data) -> std::string {
        auto result = algorithm::base64Encode(
            std::span<const unsigned char>(data.data(), data.size()));
        if (!result) {
            throw std::runtime_error("Failed to base64 encode: " +
                                     result.error());
        }
        return std::string(result.value().begin(), result.value().end());
    };

    // 5. Re-encrypt verification data with the new key/salt
    std::string newEncryptedVerificationData;
    std::vector<unsigned char> newIv(ATOM_PM_IV_SIZE);
    std::vector<unsigned char> newTag(ATOM_PM_TAG_SIZE);
    try {
        if (RAND_bytes(newIv.data(), newIv.size()) != 1) {
            throw std::runtime_error(
                "Failed to generate new IV for verification data: " +
                getOpenSSLError());
        }
        SslCipherContext ctx;
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                               newMasterKey.data(), newIv.data()) != 1) {
            throw std::runtime_error("Failed to initialize new encryption: " +
                                     getOpenSSLError());
        }
        std::vector<unsigned char> encryptedBuffer(VERIFICATION_STRING.size() +
                                                   EVP_MAX_BLOCK_LENGTH);
        int len = 0;
        if (EVP_EncryptUpdate(ctx.get(), encryptedBuffer.data(), &len,
                              reinterpret_cast<const unsigned char*>(
                                  VERIFICATION_STRING.data()),
                              VERIFICATION_STRING.size()) != 1) {
            throw std::runtime_error(
                "Failed to encrypt new verification data: " +
                getOpenSSLError());
        }
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedBuffer.data() + len,
                                &finalLen) != 1) {
            throw std::runtime_error("Failed to finalize new encryption: " +
                                     getOpenSSLError());
        }
        len += finalLen;
        encryptedBuffer.resize(len);

        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, newTag.size(),
                                newTag.data()) != 1) {
            throw std::runtime_error("Failed to get new authentication tag: " +
                                     getOpenSSLError());
        }

        std::string dataB64 = encodeAndConvert(encryptedBuffer);
        newEncryptedVerificationData =
            std::string(dataB64.begin(), dataB64.end());

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to re-encrypt verification data: {}", e.what());
        secureWipe(newMasterKey);
        // State remains unlocked with old key.
        return false;
    }

    // 6. Build new init data JSON
    nlohmann::json newInitData;
    newInitData["version"] = ATOM_PM_VERSION;
    newInitData["iterations"] = settings.encryptionOptions.keyIterations;

    std::string saltB64 = encodeAndConvert(newSalt);
    std::string ivB64 = encodeAndConvert(newIv);
    std::string tagB64 = encodeAndConvert(newTag);

    newInitData["salt"] = std::string(saltB64.begin(), saltB64.end());
    newInitData["iv"] = std::string(ivB64.begin(), ivB64.end());
    newInitData["tag"] = std::string(tagB64.begin(), tagB64.end());
    newInitData["data"] = newEncryptedVerificationData;

    std::string newSerializedInitData = newInitData.dump();

    // 7. Store new init data (overwrite old one)
    bool initStored = false;
#if defined(_WIN32)
    initStored = storeToWindowsCredentialManager(ATOM_PM_INIT_KEY,
                                                 newSerializedInitData);
#elif defined(__APPLE__)
    initStored = storeToMacKeychain(ATOM_PM_SERVICE_NAME, ATOM_PM_INIT_KEY,
                                    newSerializedInitData);
#elif defined(__linux__) && defined(USE_LIBSECRET)
    initStored = storeToLinuxKeyring(ATOM_PM_SERVICE_NAME, ATOM_PM_INIT_KEY,
                                     newSerializedInitData);
#else  // File fallback
    initStored = storeToEncryptedFile(ATOM_PM_INIT_KEY, newSerializedInitData);
#endif

    if (!initStored) {
        LOG_F(FATAL,
              "CRITICAL: Failed to store new initialization data after "
              "deriving new key. Password change incomplete. Manual recovery "
              "might be needed.");
        secureWipe(newMasterKey);
        // What state should we be in? Maybe lock and return false?
        // Locking might prevent user from trying again with old password.
        // Leaving unlocked with old key seems problematic too.
        // This is a critical failure point.
        return false;  // Indicate failure, state is unlocked with OLD key but
                       // init data might be inconsistent.
    }

    // 8. Update the master key in memory
    secureWipe(masterKey);                // Wipe old key
    masterKey = std::move(newMasterKey);  // Use new key

    // 9. Re-encrypt all entries with the new master key and store them
    LOG_F(INFO, "Re-encrypting {} entries with new master key...",
          currentEntries.size());
    bool allEntriesMigrated = true;
    for (const auto& [key, entry] : currentEntries) {
        try {
            std::string newEncryptedEntryData =
                encryptEntry(entry, masterKey);  // Use the new masterKey

            bool entryStored = false;
#if defined(_WIN32)
            entryStored =
                storeToWindowsCredentialManager(key, newEncryptedEntryData);
#elif defined(__APPLE__)
            entryStored = storeToMacKeychain(ATOM_PM_SERVICE_NAME, key,
                                             newEncryptedEntryData);
#elif defined(__linux__) && defined(USE_LIBSECRET)
            entryStored = storeToLinuxKeyring(ATOM_PM_SERVICE_NAME, key,
                                              newEncryptedEntryData);
#else  // File fallback
            entryStored = storeToEncryptedFile(key, newEncryptedEntryData);
#endif
            if (!entryStored) {
                LOG_F(ERROR, "Failed to re-store migrated password for key: {}",
                      key.c_str());
                allEntriesMigrated = false;
                // Decide how to handle: continue, stop, attempt rollback?
                // Continuing might leave some entries unmigrated.
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to re-encrypt password for key {}: {}",
                  key.c_str(), e.what());
            allEntriesMigrated = false;
        }
    }

    if (!allEntriesMigrated) {
        LOG_F(WARNING,
              "Master password changed, but failed to migrate one or more "
              "entries.");
        // The change is successful, but data might be inconsistent.
        // The new master password works, but old entries might be inaccessible
        // until manually fixed or re-saved.
    }

    // 10. Update cache (already holds the copied entries, which are fine)
    // No, the cache should reflect the current state. It's already correct.

    updateActivity();  // Update activity time
    LOG_F(INFO, "Master password changed successfully.");
    return true;  // Even if some entries failed to migrate
}

bool PasswordManager::loadAllPasswords() {
    // This function should be called under a unique_lock
    // Assumes caller holds the unique_lock

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot load passwords: PasswordManager is locked");
        return false;
    }

    std::vector<std::string> keys;
    try {
        keys = getAllPlatformKeys();  // Use internal version that doesn't
                                      // lock/update activity
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get platform keys during loadAllPasswords: {}",
              e.what());
        return false;
    }

    bool allLoaded = true;
    for (const auto& key : keys) {
        // Check if already in cache (might have been loaded by
        // retrievePassword)
        if (cachedPasswords.find(key) == cachedPasswords.end()) {
            std::string encryptedData;
            try {
#if defined(_WIN32)
                encryptedData = retrieveFromWindowsCredentialManager(key);
#elif defined(__APPLE__)
                encryptedData =
                    retrieveFromMacKeychain(ATOM_PM_SERVICE_NAME, key);
#elif defined(__linux__) && defined(USE_LIBSECRET)
                encryptedData =
                    retrieveFromLinuxKeyring(ATOM_PM_SERVICE_NAME, key);
#else  // File fallback
                encryptedData = retrieveFromEncryptedFile(key);
#endif
                if (!encryptedData.empty()) {
                    // Decrypt and add to cache
                    // Use decryptEntry directly, assumes masterKey is valid
                    PasswordEntry entry =
                        decryptEntry(encryptedData, masterKey);
                    // Securely wipe password from entry before inserting if
                    // needed? No, cache needs it.
                    cachedPasswords.emplace(
                        key, std::move(entry));  // Use emplace for efficiency
                } else {
                    LOG_F(WARNING,
                          "No data found for key '{}' during loadAllPasswords.",
                          key.c_str());
                    // This might be normal if a key exists but data is
                    // missing/corrupt
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to load/decrypt entry for key '{}': {}",
                      key.c_str(), e.what());
                allLoaded = false;  // Mark as partial success
            }
        }
    }
    LOG_F(INFO, "Finished loading passwords into cache. Success: {}",
          allLoaded);
    return allLoaded;
}

bool PasswordManager::storePassword(std::string_view platformKey,
                                    const PasswordEntry& entry) {
    // Create a copy and call the move version
    PasswordEntry entryCopy = entry;
    return storePassword(platformKey, std::move(entryCopy));
}

bool PasswordManager::storePassword(std::string_view platformKey,
                                    PasswordEntry&& entry) {
    std::unique_lock lock(mutex);  // Exclusive lock

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot store password: PasswordManager is locked");
        return false;
    }
    if (platformKey.empty()) {
        LOG_F(ERROR, "Platform key cannot be empty");
        return false;
    }
    // Ensure internal keys are not overwritten
    if (platformKey == ATOM_PM_INIT_KEY || platformKey == ATOM_PM_INDEX_KEY) {
        LOG_F(ERROR, "Attempted to overwrite internal key: {}",
              platformKey.data());
        return false;
    }

    try {
        // Update modification time before encryption
        entry.modified = std::chrono::system_clock::now();
        if (entry.created ==
            std::chrono::system_clock::time_point{}) {  // Set creation time if
                                                        // not set
            entry.created = entry.modified;
        }

        // Encrypt the entry
        std::string encryptedData =
            encryptEntry(entry, masterKey);  // Assumes masterKey is valid

        // Store encrypted data
        bool stored = false;
        std::string keyStr(
            platformKey);  // Need std::string for some platform APIs

#if defined(_WIN32)
        stored = storeToWindowsCredentialManager(keyStr, encryptedData);
#elif defined(__APPLE__)
        stored =
            storeToMacKeychain(ATOM_PM_SERVICE_NAME, keyStr, encryptedData);
#elif defined(__linux__) && defined(USE_LIBSECRET)
        stored =
            storeToLinuxKeyring(ATOM_PM_SERVICE_NAME, keyStr, encryptedData);
#else  // File fallback
        stored = storeToEncryptedFile(keyStr, encryptedData);
#endif

        if (!stored) {
            LOG_F(ERROR, "Failed to store encrypted password data for key: {}",
                  keyStr.c_str());
            return false;
        }

        // Update cache (move the entry into the cache)
        // Use operator[] or insert_or_assign for potential overwrite
        cachedPasswords[keyStr] = std::move(entry);

        updateActivity();  // Update activity time
        LOG_F(INFO, "Password stored successfully for platform key: {}",
              keyStr.c_str());
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Store password error for key '{}': {}",
              platformKey.data(), e.what());
        return false;
    }
}

std::optional<PasswordEntry> PasswordManager::retrievePassword(
    std::string_view platformKey) {
    std::string keyStr(
        platformKey);  // Make a copy for potential cache insertion

    // Use shared lock for cache check
    {
        std::shared_lock lock(mutex);
        if (!isUnlocked.load(std::memory_order_acquire)) {
            LOG_F(ERROR, "Cannot retrieve password: PasswordManager is locked");
            return std::nullopt;
        }
        if (platformKey.empty()) {
            LOG_F(ERROR, "Platform key cannot be empty for retrieval");
            return std::nullopt;
        }

        auto it = cachedPasswords.find(keyStr);
        if (it != cachedPasswords.end()) {
            LOG_F(INFO, "Password retrieved from cache for platform key: {}",
                  keyStr.c_str());
            updateActivity();   // Update activity time (requires promoting lock
                                // or separate logic) For simplicity, update
                                // activity only on storage access below.
            return it->second;  // Return a copy
        }
    }  // Shared lock released

    // If not in cache, acquire unique lock to retrieve from storage and
    // potentially update cache
    std::unique_lock lock(mutex);

    // Double-check unlock status and cache after acquiring unique lock
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR,
              "Cannot retrieve password: PasswordManager locked during "
              "operation");
        return std::nullopt;
    }
    auto it = cachedPasswords.find(keyStr);
    if (it != cachedPasswords.end()) {
        LOG_F(INFO,
              "Password retrieved from cache (after lock promotion) for "
              "platform key: {}",
              keyStr.c_str());
        // updateActivity(); // Update here if needed
        return it->second;  // Return a copy
    }

    try {
        // Retrieve encrypted data from storage
        std::string encryptedData;
#if defined(_WIN32)
        encryptedData = retrieveFromWindowsCredentialManager(keyStr);
#elif defined(__APPLE__)
        encryptedData = retrieveFromMacKeychain(ATOM_PM_SERVICE_NAME, keyStr);
#elif defined(__linux__) && defined(USE_LIBSECRET)
        encryptedData = retrieveFromLinuxKeyring(ATOM_PM_SERVICE_NAME, keyStr);
#else  // File fallback
        encryptedData = retrieveFromEncryptedFile(keyStr);
#endif

        if (encryptedData.empty()) {
            LOG_F(WARNING, "No password data found for platform key: {}",
                  keyStr.c_str());
            return std::nullopt;  // Not found
        }

        // Decrypt the entry
        PasswordEntry entry = decryptEntry(
            encryptedData, masterKey);  // Assumes masterKey is valid

        // Add to cache
        auto [iter, inserted] =
            cachedPasswords.emplace(keyStr, entry);  // Use emplace

        updateActivity();  // Update activity time
        LOG_F(INFO, "Password retrieved from storage for platform key: {}",
              keyStr.c_str());
        return iter
            ->second;  // Return reference to cached entry (as optional copy)

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Retrieve password error for key '{}': {}", keyStr.c_str(),
              e.what());
        return std::nullopt;
    }
}

bool PasswordManager::deletePassword(std::string_view platformKey) {
    std::unique_lock lock(mutex);  // Exclusive lock

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot delete password: PasswordManager is locked");
        return false;
    }
    if (platformKey.empty()) {
        LOG_F(ERROR, "Platform key cannot be empty for deletion");
        return false;
    }
    // Prevent deletion of internal keys
    if (platformKey == ATOM_PM_INIT_KEY || platformKey == ATOM_PM_INDEX_KEY) {
        LOG_F(ERROR, "Attempted to delete internal key: {}",
              platformKey.data());
        return false;
    }

    std::string keyStr(platformKey);  // Need std::string

    try {
        // Delete from platform storage first
        bool deletedFromStorage = false;
#if defined(_WIN32)
        deletedFromStorage = deleteFromWindowsCredentialManager(keyStr);
#elif defined(__APPLE__)
        deletedFromStorage =
            deleteFromMacKeychain(ATOM_PM_SERVICE_NAME, keyStr);
#elif defined(__linux__) && defined(USE_LIBSECRET)
        deletedFromStorage =
            deleteFromLinuxKeyring(ATOM_PM_SERVICE_NAME, keyStr);
#else  // File fallback
        deletedFromStorage = deleteFromEncryptedFile(keyStr);
#endif

        if (!deletedFromStorage) {
            // Log warning but proceed to remove from cache if it exists
            LOG_F(WARNING,
                  "Failed to delete password from underlying storage for key: "
                  "{}. Might be already deleted.",
                  keyStr.c_str());
        }

        // Remove from cache
        auto numErased = cachedPasswords.erase(keyStr);
        if (numErased > 0) {
            LOG_F(INFO, "Password removed from cache for key: {}",
                  keyStr.c_str());
        }

        updateActivity();  // Update activity time
        LOG_F(INFO, "Password deletion processed for platform key: {}",
              keyStr.c_str());
        // Return true if it was successfully removed from storage or cache,
        // or if it wasn't found in storage (idempotent delete)
        return deletedFromStorage || numErased > 0;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Delete password error for key '{}': {}", keyStr.c_str(),
              e.what());
        return false;
    }
}

// Public wrapper for getAllPlatformKeys
std::vector<std::string> PasswordManager::getAllPlatformKeys() const {
    std::shared_lock lock(mutex);  // Shared lock is sufficient if
                                   // getAllPlatformKeysInternal is const

    if (!isUnlocked.load(std::memory_order_acquire)) {
        LOG_F(ERROR, "Cannot get platform keys: PasswordManager is locked");
        return {};
    }
    // updateActivity(); // Reading keys might not count as activity needing
    // lock update

    try {
        return getAllPlatformKeysInternal();  // This is correct, issue was in the error line
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Get all platform keys error: {}", e.what());
        return {};
    }
}

// Internal version without locking or activity update
std::vector<std::string> PasswordManager::getAllPlatformKeysInternal() const {
    std::vector<std::string> keys;

#if defined(_WIN32)
    keys = getAllWindowsCredentials();
#elif defined(__APPLE__)
    keys = getAllMacKeychainItems(ATOM_PM_SERVICE_NAME);
#elif defined(__linux__) && defined(USE_LIBSECRET)
    keys = getAllLinuxKeyringItems(ATOM_PM_SERVICE_NAME);
#else  // File fallback
    keys = getAllEncryptedFileItems();
#endif

    // Filter out internal keys
    keys.erase(std::remove_if(keys.begin(), keys.end(),
                              [](const std::string& key) {
                                  return key == ATOM_PM_INIT_KEY ||
                                         key == ATOM_PM_INDEX_KEY;
                              }),
               keys.end());

    // LOG_F(INFO, "Retrieved {} platform keys (internal)", keys.size()); //
    // Maybe too verbose
    return keys;
}

std::vector<std::string> PasswordManager::searchPasswords(
    std::string_view query) {
    std::unique_lock lock(mutex);  // Need unique lock to potentially load cache

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot search passwords: PasswordManager is locked");
        return {};
    }
    if (query.empty()) {
        LOG_F(WARNING, "Empty search query, returning all keys.");
        // Return keys from cache directly if possible, avoid
        // getAllPlatformKeysInternal if cache is full
        std::vector<std::string> allKeys;
        allKeys.reserve(cachedPasswords.size());
        for (const auto& pair : cachedPasswords) {
            allKeys.push_back(pair.first);
        }
        // If cache isn't fully loaded, we might need to load it first
        // loadAllPasswords(); // Ensure cache is full - uncomment if needed
        // Then collect keys from cache
        return allKeys;  // Or call the public getAllPlatformKeys() which
                         // handles locking
    }

    updateActivity();

    try {
        // Ensure cache is loaded (consider lazy loading strategy)
        loadAllPasswords();  // Assumes this is safe to call multiple times

        std::vector<std::string> results;
        std::string lowerQuery(query);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        for (const auto& [key, entry] : cachedPasswords) {
            // Create lowercase versions for comparison only if needed
            auto checkMatch = [&](const std::string& text) {
                if (text.empty())
                    return false;
                std::string lowerText = text;
                std::transform(lowerText.begin(), lowerText.end(),
                               lowerText.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                return lowerText.find(lowerQuery) != std::string::npos;
            };

            if (checkMatch(key) || checkMatch(entry.username) ||
                checkMatch(entry.url) || checkMatch(entry.notes)) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Search for '{}' returned {} results", query.data(),
              results.size());
        return results;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Search passwords error: {}", e.what());
        return {};
    }
}

std::vector<std::string> PasswordManager::filterByCategory(
    PasswordCategory category) {
    std::unique_lock lock(mutex);  // Need unique lock to potentially load cache

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot filter passwords: PasswordManager is locked");
        return {};
    }

    updateActivity();

    try {
        // Ensure cache is loaded
        loadAllPasswords();

        std::vector<std::string> results;
        for (const auto& [key, entry] : cachedPasswords) {
            if (entry.category == category) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Filter by category {} returned {} results",
              static_cast<int>(category), results.size());
        return results;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Filter by category error: {}", e.what());
        return {};
    }
}

std::string PasswordManager::generatePassword(int length, bool includeSpecial,
                                              bool includeNumbers,
                                              bool includeMixedCase) {
    // No lock needed for generation itself, but updateActivity needs it
    // Call updateActivity first
    {
        std::unique_lock lock(mutex);
        if (!isUnlocked.load(std::memory_order_relaxed)) {
            LOG_F(ERROR, "Cannot generate password: PasswordManager is locked");
            return "";  // Or throw? Returning empty seems safer.
        }
        updateActivity();
    }

    if (length < settings.minPasswordLength) {
        LOG_F(WARNING,
              "Requested password length {} is less than minimum {}, using "
              "minimum.",
              length, settings.minPasswordLength);
        length = settings.minPasswordLength;
    }
    if (length <= 0) {  // Ensure length is positive
        length = 16;    // Default fallback
    }

    // Character sets
    const std::string lower = "abcdefghijklmnopqrstuvwxyz";
    const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string digits = "0123456789";
    const std::string special =
        "!@#$%^&*()-_=+[]{}\\|;:'\",.<>/?`~";  // Common special chars

    std::string charPool;
    std::vector<char> requiredChars;

    charPool += lower;
    requiredChars.push_back(lower[0]);  // Placeholder, will be randomized

    if (includeMixedCase || settings.requireMixedCase) {
        charPool += upper;
        requiredChars.push_back(upper[0]);
    }
    if (includeNumbers || settings.requireNumbers) {
        charPool += digits;
        requiredChars.push_back(digits[0]);
    }
    if (includeSpecial || settings.requireSpecialChars) {
        charPool += special;
        requiredChars.push_back(special[0]);
    }

    if (charPool.empty()) {
        LOG_F(ERROR, "Character pool for password generation is empty.");
        return "";  // Cannot generate
    }

    // Use C++ random engine for better portability and control than RAND_bytes
    // for this task
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> pool_dist(0, charPool.length() - 1);
    std::uniform_int_distribution<size_t> lower_dist(0, lower.length() - 1);
    std::uniform_int_distribution<size_t> upper_dist(0, upper.length() - 1);
    std::uniform_int_distribution<size_t> digit_dist(0, digits.length() - 1);
    std::uniform_int_distribution<size_t> special_dist(0, special.length() - 1);

    std::string password(length, ' ');
    size_t requiredCount = requiredChars.size();

    // Fill required characters first
    requiredChars[0] = lower[lower_dist(generator)];
    size_t reqIdx = 1;
    if (includeMixedCase || settings.requireMixedCase)
        requiredChars[reqIdx++] = upper[upper_dist(generator)];
    if (includeNumbers || settings.requireNumbers)
        requiredChars[reqIdx++] = digits[digit_dist(generator)];
    if (includeSpecial || settings.requireSpecialChars)
        requiredChars[reqIdx++] = special[special_dist(generator)];

    // Fill the rest of the password length
    for (size_t i = 0; i < length; ++i) {
        password[i] = charPool[pool_dist(generator)];
    }

    // Shuffle required characters into random positions
    std::vector<size_t> positions(length);
    std::iota(positions.begin(), positions.end(), 0);
    std::shuffle(positions.begin(), positions.end(), generator);

    for (size_t i = 0; i < requiredCount && i < length; ++i) {
        password[positions[i]] = requiredChars[i];
    }

    // Shuffle the whole password for good measure
    std::shuffle(password.begin(), password.end(), generator);

    LOG_F(INFO, "Generated password of length {}", length);
    return password;
}

PasswordStrength PasswordManager::evaluatePasswordStrength(
    std::string_view password) const {
    // No lock needed for evaluation, it's const
    // updateActivity(); // Reading strength might not count as activity

    const size_t len = password.length();
    if (len == 0)
        return PasswordStrength::VeryWeak;

    int score = 0;
    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    // Entropy approximation points (very rough)
    if (len >= 8)
        score += 1;
    if (len >= 12)
        score += 1;
    if (len >= 16)
        score += 1;

    for (char c : password) {
        if (std::islower(c))
            hasLower = true;
        else if (std::isupper(c))
            hasUpper = true;
        else if (std::isdigit(c))
            hasDigit = true;
        // Consider a more robust check for special characters
        else if (std::ispunct(c) || std::isgraph(c))
            hasSpecial = true;  // Broader check
    }

    int charTypes = 0;
    if (hasLower)
        charTypes++;
    if (hasUpper)
        charTypes++;
    if (hasDigit)
        charTypes++;
    if (hasSpecial)
        charTypes++;

    if (charTypes >= 2)
        score += 1;
    if (charTypes >= 3)
        score += 1;
    if (charTypes >= 4)
        score += 1;

    // Penalties for common patterns (simple checks)
    try {
    if (hasSpecial)
        charTypes++;

    if (charTypes >= 2)
        score += 1;
    if (charTypes >= 3)
        score += 1;
    if (charTypes >= 4)
        score += 1;

    // Penalties for common patterns (simple checks)
    try {
        if (std::regex_search(
                password.begin(), password.end(),
                std::regex("(\\w)\\1{2,}"))) {  // 3+ repeated chars
            score -= 1;
        }
        if (std::regex_search(
                password.begin(), password.end(),
                std::regex("(abc|bcd|cde|def|efg|fgh|pqr|qrs|rst|123|234|345|"
                           "456|567|678|789|qwerty|asdfgh|zxcvbn)",
                           std::regex::icase))) {  // Common sequences
            score -= 1;
        }
        // Add more checks: dictionary words, common substitutions etc. (can
        // become complex)
    } catch (const std::regex_error& e) {
        LOG_F(ERROR, "Regex error during password strength evaluation: {}",
              e.what());
        // Proceed without regex checks if they fail
    }

    // Map score to strength level
    if (score <= 1)
        return PasswordStrength::VeryWeak;
    if (score == 2)
        return PasswordStrength::Weak;
    if (score == 3)
        return PasswordStrength::Medium;
    if (score == 4)
        return PasswordStrength::Strong;
    // score >= 5
    return PasswordStrength::VeryStrong;
}

bool PasswordManager::exportPasswords(const std::filesystem::path& filePath,
                                      std::string_view password) {
    std::unique_lock lock(mutex);  // Need unique lock to load cache

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot export passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Export requires a non-empty password.");
        return false;
    }

    updateActivity();

    try {
        // Ensure cache is loaded
        loadAllPasswords();

        // Prepare export data JSON
        json exportJson;
        exportJson["version"] = ATOM_PM_VERSION;
        exportJson["timestamp"] =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        json entriesArray = json::array();

        for (const auto& [key, entry] : cachedPasswords) {
            json entryJson;
            entryJson["key"] = key;
            entryJson["username"] = entry.username;
            entryJson["password"] =
                entry.password;  // Exporting plaintext password within
                                 // encrypted blob
            entryJson["url"] = entry.url;
            entryJson["notes"] = entry.notes;
            entryJson["category"] = static_cast<int>(entry.category);
            entryJson["created"] =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    entry.created.time_since_epoch())
                    .count();
            entryJson["modified"] =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    entry.modified.time_since_epoch())
                    .count();
            entryJson["previousPasswords"] =
                entry.previousPasswords;  // Array of strings
            entriesArray.push_back(entryJson);
        }
        exportJson["entries"] = entriesArray;

        std::string serializedEntries = exportJson.dump();

        // Encrypt the serialized JSON with the provided password
        std::vector<unsigned char> exportSalt(ATOM_PM_SALT_SIZE);
        if (RAND_bytes(exportSalt.data(), exportSalt.size()) != 1) {
            throw std::runtime_error(
                "Failed to generate random salt for export: " +
                getOpenSSLError());
        }

        // Use a fixed high iteration count for export/import for compatibility
        std::vector<unsigned char> exportKey =
            deriveKey(password, exportSalt, DEFAULT_PBKDF2_ITERATIONS);

        std::vector<unsigned char> exportIv(ATOM_PM_IV_SIZE);
        if (RAND_bytes(exportIv.data(), exportIv.size()) != 1) {
            secureWipe(exportKey);
            throw std::runtime_error(
                "Failed to generate random IV for export: " +
                getOpenSSLError());
        }

        std::vector<unsigned char> encryptedExportData;
        std::vector<unsigned char> exportTag(ATOM_PM_TAG_SIZE);

        try {
            SslCipherContext ctx;
            if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                                   exportKey.data(), exportIv.data()) != 1) {
                throw std::runtime_error(
                    "Failed to initialize export encryption: " +
                    getOpenSSLError());
            }
            encryptedExportData.resize(serializedEntries.size() +
                                       EVP_MAX_BLOCK_LENGTH);
            int len = 0;
            if (EVP_EncryptUpdate(ctx.get(), encryptedExportData.data(), &len,
                                  reinterpret_cast<const unsigned char*>(
                                      serializedEntries.data()),
                                  serializedEntries.size()) != 1) {
                throw std::runtime_error("Failed to encrypt export data: " +
                                         getOpenSSLError());
            }
            int finalLen = 0;
            if (EVP_EncryptFinal_ex(ctx.get(), encryptedExportData.data() + len,
                                    &finalLen) != 1) {
                throw std::runtime_error(
                    "Failed to finalize export encryption: " +
                    getOpenSSLError());
            }
            len += finalLen;
            encryptedExportData.resize(len);

            if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
                                    exportTag.size(), exportTag.data()) != 1) {
                throw std::runtime_error(
                    "Failed to get export authentication tag: " +
                    getOpenSSLError());
            }
        } catch (...) {
            secureWipe(exportKey);  // Ensure key is wiped on encryption error
            throw;                  // Re-throw
        }
        secureWipe(exportKey);  // Wipe key after use

        // Build final export file structure (JSON containing base64 parts)
        json finalExportFile;
        finalExportFile["format"] =
            "ATOM_PM_EXPORT_V2";  // Version the export format
        finalExportFile["version"] = ATOM_PM_VERSION;

        auto saltBase64 = algorithm::base64Encode(exportSalt);
        auto ivBase64 = algorithm::base64Encode(exportIv);
        auto tagBase64 = algorithm::base64Encode(exportTag);
        auto dataBase64 = algorithm::base64Encode(encryptedExportData);

        if (!saltBase64 || !ivBase64 || !tagBase64 || !dataBase64) {
            throw std::runtime_error(
                "Failed to base64 encode export components.");
        }

        finalExportFile["salt"] =
            std::string(saltBase64->begin(), saltBase64->end());
        finalExportFile["iv"] = std::string(ivBase64->begin(), ivBase64->end());
        finalExportFile["tag"] =
            std::string(tagBase64->begin(), tagBase64->end());
        finalExportFile["data"] =
            std::string(dataBase64->begin(), dataBase64->end());

        // Write to file
        std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
        if (!outFile) {
            throw std::runtime_error(
                "Failed to open export file for writing: " + filePath.string());
        }
        outFile << finalExportFile.dump(4);  // Pretty print JSON
        outFile.close();
        if (!outFile) {  // Check for write errors after closing
            throw std::runtime_error("Failed to write data to export file: " +
                                     filePath.string());
        }

        LOG_F(INFO, "Successfully exported {} password entries to {}",
              cachedPasswords.size(), filePath.string().c_str());
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Export passwords error: {}", e.what());
        // Attempt to delete partially created file?
        std::error_code ec;
        std::filesystem::remove(filePath,
                                ec);  // Ignore error if file doesn't exist
        return false;
    }
}

bool PasswordManager::importPasswords(const std::filesystem::path& filePath,
                                      std::string_view password) {
    std::unique_lock lock(mutex);  // Exclusive lock for import

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot import passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Import requires a non-empty password.");
        return false;
    }

    updateActivity();

    try {
        // Read the entire file
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            throw std::runtime_error(
                "Failed to open import file for reading: " + filePath.string());
        }
        std::string fileContent((std::istreambuf_iterator<char>(inFile)),
                                std::istreambuf_iterator<char>());
        inFile.close();

        if (fileContent.empty()) {
            throw std::runtime_error("Import file is empty: " +
                                     filePath.string());
        }

        // Parse the outer JSON structure
        json importFileJson = json::parse(fileContent);

        // Verify format
        std::string format = importFileJson.at("format").get<std::string>();
        if (format != "ATOM_PM_EXPORT_V2") {  // Check correct format version
            throw std::runtime_error(
                "Invalid or unsupported import file format: " + format);
        }
        // Optionally check importFileJson["version"] against ATOM_PM_VERSION

        // Decode base64 components
        auto saltResult = algorithm::base64Decode(
            importFileJson.at("salt").get<std::string>());
        auto ivResult =
            algorithm::base64Decode(importFileJson.at("iv").get<std::string>());
        auto tagResult = algorithm::base64Decode(
            importFileJson.at("tag").get<std::string>());
        auto dataResult = algorithm::base64Decode(
            importFileJson.at("data").get<std::string>());

        if (!saltResult || !ivResult || !tagResult || !dataResult) {
            throw std::runtime_error(
                "Failed to decode base64 components from import file.");
        }
        std::vector<unsigned char> importSalt = std::move(*saltResult);
        std::vector<unsigned char> importIv = std::move(*ivResult);
        std::vector<unsigned char> importTag = std::move(*tagResult);
        std::vector<unsigned char> encryptedImportData = std::move(*dataResult);

        // Derive key from import password
        std::vector<unsigned char> importKey;
        try {
            importKey =
                deriveKey(password, importSalt,
                          DEFAULT_PBKDF2_ITERATIONS);  // Use fixed iterations
        } catch (...) {
            // Don't log error here, could be wrong password
            LOG_F(WARNING, "Failed to derive key from import password.");
            return false;  // Treat as wrong password
        }

        // Decrypt the inner JSON data
        std::vector<unsigned char> decryptedDataBytes;
        bool importDecryptionSuccess = false;
        try {
            SslCipherContext ctx;
            if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                                   importKey.data(), importIv.data()) != 1) {
                throw std::runtime_error(
                    "Failed to initialize import decryption: " +
                    getOpenSSLError());
            }
            if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG,
                                    importTag.size(), importTag.data()) != 1) {
                throw std::runtime_error(
                    "Failed to set import authentication tag: " +
                    getOpenSSLError());
            }
            decryptedDataBytes.resize(
                encryptedImportData.size());  // Initial guess
            int len = 0;
            if (EVP_DecryptUpdate(ctx.get(), decryptedDataBytes.data(), &len,
                                  encryptedImportData.data(),
                                  encryptedImportData.size()) != 1) {
                // Error during update
                throw std::runtime_error(
                    "Failed to decrypt import data update: " +
                    getOpenSSLError());
            }
            int finalLen = 0;
            int ret = EVP_DecryptFinal_ex(
                ctx.get(), decryptedDataBytes.data() + len, &finalLen);

            if (ret > 0) {  // Success
                len += finalLen;
                decryptedDataBytes.resize(len);
                importDecryptionSuccess = true;
            } else if (ret == 0) {  // Verification failed (tag mismatch)
                LOG_F(WARNING,
                      "Authentication failed for import file - incorrect "
                      "password?");
                // importDecryptionSuccess remains false
            } else {  // Other error
                throw std::runtime_error(
                    "Import decryption finalization failed: " +
                    getOpenSSLError());
            }
        } catch (...) {
            secureWipe(importKey);  // Wipe key on error
            throw;                  // Re-throw
        }
        secureWipe(importKey);  // Wipe key after use

        if (!importDecryptionSuccess) {
            return false;  // Authentication failed
        }

        // Parse the decrypted inner JSON
        std::string decryptedJsonStr(decryptedDataBytes.begin(),
                                     decryptedDataBytes.end());
        json importedEntriesJson = json::parse(decryptedJsonStr);

        // Import entries
        int importedCount = 0;
        int skippedCount = 0;
        if (!importedEntriesJson.contains("entries") ||
            !importedEntriesJson["entries"].is_array()) {
            throw std::runtime_error("Import data missing 'entries' array.");
        }

        for (const auto& entryJson : importedEntriesJson["entries"]) {
            try {
                PasswordEntry entry;
                std::string key = entryJson.at("key").get<std::string>();

                // Basic validation
                if (key.empty()) {
                    LOG_F(WARNING, "Skipping import for entry with empty key.");
                    skippedCount++;
                    continue;
                }
                // Skip internal keys
                if (key == ATOM_PM_INIT_KEY || key == ATOM_PM_INDEX_KEY) {
                    LOG_F(WARNING, "Skipping import for internal key: {}",
                          key.c_str());
                    skippedCount++;
                    continue;
                }

                entry.username = entryJson.value("username", "");
                entry.password =
                    entryJson.value("password", "");  // Password should exist
                entry.url = entryJson.value("url", "");
                entry.notes = entryJson.value("notes", "");
                entry.category = static_cast<PasswordCategory>(entryJson.value(
                    "category", static_cast<int>(PasswordCategory::General)));
                entry.created = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(entryJson.value("created", 0LL)));
                entry.modified = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(
                        entryJson.value("modified", 0LL)));

                if (entryJson.contains("previousPasswords") &&
                    entryJson["previousPasswords"].is_array()) {
                    for (const auto& prevPwd : entryJson["previousPasswords"]) {
                        if (prevPwd.is_string()) {
                            entry.previousPasswords.push_back(
                                prevPwd.get<std::string>());
                        }
                    }
                }

                // Handle conflicts: Overwrite existing entry with the same key
                if (storePassword(
                        key, std::move(entry))) {  // storePassword handles
                                                   // locking and cache update
                    importedCount++;
                } else {
                    LOG_F(ERROR, "Failed to store imported entry for key: {}",
                          key.c_str());
                    skippedCount++;
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to parse or store an imported entry: {}",
                      e.what());
                skippedCount++;
            }
        }

        LOG_F(INFO, "Import finished. Imported: {}, Skipped/Failed: {}",
              importedCount, skippedCount);
        return importedCount > 0 ||
               skippedCount ==
                   0;  // Success if at least one imported or none failed

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Import passwords error: {}", e.what());
        return false;
    }
}

void PasswordManager::updateSettings(
    const PasswordManagerSettings& newSettings) {
    std::unique_lock lock(mutex);  // Exclusive lock
    settings = newSettings;
    updateActivity();  // Update activity time
    LOG_F(INFO, "PasswordManager settings updated");
    // Note: Changing keyIterations might require re-encrypting init data,
    // but that's complex. Current implementation reads iterations on unlock.
}

PasswordManagerSettings PasswordManager::getSettings() const noexcept {
    std::shared_lock lock(
        mutex);       // Shared lock sufficient for reading settings
    return settings;  // Return a copy
}

std::vector<std::string> PasswordManager::checkExpiredPasswords() {
    std::unique_lock lock(mutex);  // Need unique lock to potentially load cache

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR,
              "Cannot check expired passwords: PasswordManager is locked");
        return {};
    }

    if (!settings.notifyOnPasswordExpiry || settings.passwordExpiryDays <= 0) {
        return {};  // Feature disabled
    }

    updateActivity();

    try {
        // Ensure cache is loaded
        loadAllPasswords();

        std::vector<std::string> expiredKeys;
        const auto now = std::chrono::system_clock::now();
        const auto expiryDuration =
            std::chrono::hours(24) * settings.passwordExpiryDays;

        for (const auto& [key, entry] : cachedPasswords) {
            // Check if modification time is valid and older than expiry
            // duration
            if (entry.modified != std::chrono::system_clock::time_point{} &&
                (now - entry.modified) > expiryDuration) {
                expiredKeys.push_back(key);
            }
        }

        if (!expiredKeys.empty()) {
            LOG_F(INFO, "Found {} expired passwords (older than {} days)",
                  expiredKeys.size(), settings.passwordExpiryDays);
        }
        return expiredKeys;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Check expired passwords error: {}", e.what());
        return {};
    }
}

void PasswordManager::setActivityCallback(std::function<void()> callback) {
    std::unique_lock lock(mutex);  // Lock to safely update the callback
    activityCallback = std::move(callback);
}

// --- Private Methods ---

void PasswordManager::updateActivity() {
    // Assumes caller holds a unique_lock
    lastActivity = std::chrono::system_clock::now();

    // Trigger callback if set
    if (activityCallback) {
        try {
            // Execute callback outside of lock? Depends if callback needs
            // manager state. If callback might call back into PasswordManager,
            // deadlock is possible. Execute it here under lock for simplicity,
            // but document potential issues.
            activityCallback();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in activity callback: {}", e.what());
        } catch (...) {
            LOG_F(ERROR, "Unknown exception in activity callback.");
        }
    }

    // Auto-lock check is handled lazily by isLocked() or explicitly by
    // user/timer Checking it here on every activity might be too aggressive. If
    // auto-lock timer needs active checking, a separate thread might be better.
}

std::vector<unsigned char> PasswordManager::deriveKey(
    std::string_view masterPassword, std::span<const unsigned char> salt,
    int iterations) const {
    // No lock needed, this is a const computation based on inputs

    if (iterations <= 0) {
        iterations = DEFAULT_PBKDF2_ITERATIONS;  // Use default if invalid
        LOG_F(WARNING, "Invalid PBKDF2 iterations ({}), using default ({}).",
              iterations, DEFAULT_PBKDF2_ITERATIONS);
    }

    std::vector<unsigned char> derivedKey(32);  // AES-256 key size

    // Use EVP_KDF functions for PBKDF2 (recommended over PKCS5_PBKDF2_HMAC)
    std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)> kdf(
        EVP_KDF_fetch(nullptr, "PBKDF2", nullptr), EVP_KDF_free);
    if (!kdf) {
        throw std::runtime_error("Failed to fetch PBKDF2 KDF: " +
                                 getOpenSSLError());
    }

    std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)> kctx(
        EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free);
    if (!kctx) {
        throw std::runtime_error("Failed to create KDF context: " +
                                 getOpenSSLError());
    }

    OSSL_PARAM params[5];
    params[0] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_PASSWORD,
        const_cast<char*>(masterPassword.data()),  // OpenSSL API annoyance
        masterPassword.length());
    params[1] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_SALT,
        const_cast<unsigned char*>(salt.data()),  // OpenSSL API annoyance
        salt.size());
    params[2] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_ITER, &iterations);
    params[3] = OSSL_PARAM_construct_utf8_string(
        OSSL_KDF_PARAM_DIGEST, const_cast<char*>(SN_sha256), 0);  // Use SHA256
    params[4] = OSSL_PARAM_construct_end();

    if (EVP_KDF_derive(kctx.get(), derivedKey.data(), derivedKey.size(),
                       params) != 1) {
        throw std::runtime_error("Failed to derive key using PBKDF2: " +
                                 getOpenSSLError());
    }

    return derivedKey;
}

template <typename T>
void PasswordManager::secureWipe(T& data) noexcept {
    // Use standard library functions if available (C++23 adds
    // std::memset_explicit) Otherwise, use platform specifics or volatile loop
    // as fallback.
#if defined(_WIN32)
    SecureZeroMemory(data.data(), data.size() * sizeof(typename T::value_type));
#elif defined(__linux__) || defined(__APPLE__)
    // Use explicit_bzero if available (glibc, BSDs)
    // Or rely on compiler intrinsics / volatile loop
    // For simplicity, using volatile loop here. Add #ifdef for explicit_bzero
    // if needed.
    volatile typename T::value_type* p = data.data();
    size_t cnt = data.size();
    while (cnt--) {
        *p++ = 0;
    }
    // Add memory barrier if needed, though volatile *might* suffice for simple
    // cases asm volatile("" : : "r"(data.data()) : "memory"); // Example memory
    // barrier
#else
    // Generic volatile loop fallback
    volatile typename T::value_type* p = data.data();
    size_t cnt = data.size();
    while (cnt--) {
        *p++ = 0;
    }
#endif
    // Ensure the container itself is cleared if it holds pointers or other
    // resources For std::vector<unsigned char> or std::string, clearing is
    // usually sufficient after wiping data.
    data.clear();
    data.shrink_to_fit();  // Reduce capacity
}

// Specialization for std::string
template <>
void PasswordManager::secureWipe<std::string>(std::string& data) noexcept {
#if defined(_WIN32)
    if (!data.empty()) {
        SecureZeroMemory(&data[0], data.size());
    }
#elif defined(__linux__) || defined(__APPLE__)
    if (!data.empty()) {
        volatile char* p = &data[0];
        size_t cnt = data.size();
        while (cnt--) {
            *p++ = 0;
        }
        // asm volatile("" : : "r"(data.data()) : "memory");
    }
#else
    if (!data.empty()) {
        volatile char* p = &data[0];
        size_t cnt = data.size();
        while (cnt--) {
            *p++ = 0;
        }
    }
#endif
    data.clear();
    data.shrink_to_fit();
}

std::string PasswordManager::encryptEntry(
    const PasswordEntry& entry, std::span<const unsigned char> key) const {
    // No lock needed, const method using provided key

    // Serialize entry to JSON
    json entryJson;
    try {
        entryJson["username"] = entry.username;
        entryJson["password"] = entry.password;  // Plaintext password in JSON
        entryJson["url"] = entry.url;
        entryJson["notes"] = entry.notes;
        entryJson["category"] = static_cast<int>(entry.category);
        entryJson["created"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                entry.created.time_since_epoch())
                .count();
        entryJson["modified"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                entry.modified.time_since_epoch())
                .count();
        entryJson["previousPasswords"] = entry.previousPasswords;
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("JSON serialization failed during encryption: ") +
            e.what());
    }

    std::string serializedEntry = entryJson.dump();

    // Generate random IV
    std::vector<unsigned char> iv(ATOM_PM_IV_SIZE);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        throw std::runtime_error(
            "Failed to generate random IV for entry encryption: " +
            getOpenSSLError());
    }

    // Encrypt using AES-GCM
    std::vector<unsigned char> encryptedData;
    std::vector<unsigned char> tag(ATOM_PM_TAG_SIZE);
    try {
        SslCipherContext ctx;
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                               key.data(), iv.data()) != 1) {
            throw std::runtime_error("Failed to initialize entry encryption: " +
                                     getOpenSSLError());
        }

        encryptedData.resize(serializedEntry.size() +
                             EVP_MAX_BLOCK_LENGTH);  // Max possible size
        int len = 0;
        if (EVP_EncryptUpdate(
                ctx.get(), encryptedData.data(), &len,
                reinterpret_cast<const unsigned char*>(serializedEntry.data()),
                serializedEntry.size()) != 1) {
            throw std::runtime_error("Failed to encrypt entry data: " +
                                     getOpenSSLError());
        }
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedData.data() + len,
                                &finalLen) != 1) {
            throw std::runtime_error("Failed to finalize entry encryption: " +
                                     getOpenSSLError());
        }
        len += finalLen;
        encryptedData.resize(len);  // Resize to actual encrypted size

        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(),
                                tag.data()) != 1) {
            throw std::runtime_error(
                "Failed to get entry authentication tag: " + getOpenSSLError());
        }
    } catch (const std::exception& e) {
        // No key wipe needed here as key is passed in
        throw;  // Re-throw
    }

    // Build encrypted package JSON
    json encJson;
    auto ivBase64 = algorithm::base64Encode(iv);
    auto tagBase64 = algorithm::base64Encode(tag);
    auto dataBase64 = algorithm::base64Encode(encryptedData);

    if (!ivBase64 || !tagBase64 || !dataBase64) {
        throw std::runtime_error(
            "Failed to base64 encode encrypted entry components.");
    }

    encJson["iv"] = std::string(ivBase64->begin(), ivBase64->end());
    encJson["tag"] = std::string(tagBase64->begin(), tagBase64->end());
    encJson["data"] = std::string(dataBase64->begin(), dataBase64->end());
    // Optionally add encryption method identifier if supporting multiple
    // methods encJson["method"] =
    // static_cast<int>(settings.encryptionOptions.encryptionMethod);

    return encJson.dump();
}

PasswordEntry PasswordManager::decryptEntry(
    std::string_view encryptedData, std::span<const unsigned char> key) const {
    // No lock needed, const method using provided key

    // Parse the encrypted package JSON
    json encJson;
    std::vector<unsigned char> iv, tag, dataBytes;
    try {
        encJson = json::parse(encryptedData);

        auto ivResult =
            algorithm::base64Decode(encJson.at("iv").get<std::string>());
        auto tagResult =
            algorithm::base64Decode(encJson.at("tag").get<std::string>());
        auto dataResult =
            algorithm::base64Decode(encJson.at("data").get<std::string>());

        if (!ivResult || !tagResult || !dataResult) {
            throw std::runtime_error(
                "Failed to decode base64 components from encrypted data.");
        }
        iv = std::move(*ivResult);
        tag = std::move(*tagResult);
        dataBytes = std::move(*dataResult);

        // Check IV and Tag sizes
        if (iv.size() != ATOM_PM_IV_SIZE)
            throw std::runtime_error("Invalid IV size in encrypted data.");
        if (tag.size() != ATOM_PM_TAG_SIZE)
            throw std::runtime_error("Invalid tag size in encrypted data.");

    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to parse encrypted data package: ") + e.what());
    }

    // Decrypt using AES-GCM
    std::vector<unsigned char> decryptedDataBytes;
    bool decryptionSuccess = false;
    try {
        SslCipherContext ctx;
        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                               key.data(), iv.data()) != 1) {
            throw std::runtime_error("Failed to initialize entry decryption: " +
                                     getOpenSSLError());
        }
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(),
                                tag.data()) != 1) {
            throw std::runtime_error(
                "Failed to set entry authentication tag: " + getOpenSSLError());
        }

        decryptedDataBytes.resize(dataBytes.size());  // Initial guess
        int len = 0;
        if (EVP_DecryptUpdate(ctx.get(), decryptedDataBytes.data(), &len,
                              dataBytes.data(), dataBytes.size()) != 1) {
            // Error during update
            throw std::runtime_error("Failed to decrypt entry data update: " +
                                     getOpenSSLError());
        }
        int finalLen = 0;
        int ret = EVP_DecryptFinal_ex(
            ctx.get(), decryptedDataBytes.data() + len, &finalLen);

        if (ret > 0) {  // Success
            len += finalLen;
            decryptedDataBytes.resize(len);
            decryptionSuccess = true;
        } else if (ret == 0) {  // Verification failed (tag mismatch)
            throw std::runtime_error(
                "Authentication failed - entry data may be corrupted or key is "
                "wrong.");
        } else {  // Other error
            throw std::runtime_error("Entry decryption finalization failed: " +
                                     getOpenSSLError());
        }
    } catch (const std::exception& e) {
        // No key wipe needed
        throw;  // Re-throw
    }

    // Parse the decrypted JSON
    PasswordEntry entry;
    try {
        std::string decryptedJsonStr(decryptedDataBytes.begin(),
                                     decryptedDataBytes.end());
        json entryJson = json::parse(decryptedJsonStr);

        entry.username = entryJson.value("username", "");
        entry.password = entryJson.value("password", "");
        entry.url = entryJson.value("url", "");
        entry.notes = entryJson.value("notes", "");
        entry.category = static_cast<PasswordCategory>(entryJson.value(
            "category", static_cast<int>(PasswordCategory::General)));
        entry.created = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(entryJson.value("created", 0LL)));
        entry.modified = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(entryJson.value("modified", 0LL)));

        if (entryJson.contains("previousPasswords") &&
            entryJson["previousPasswords"].is_array()) {
            for (const auto& prevPwd : entryJson["previousPasswords"]) {
                if (prevPwd.is_string()) {
                    entry.previousPasswords.push_back(
                        prevPwd.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to parse decrypted entry JSON: ") + e.what());
    }

    return entry;
}

// --- Platform Specific Implementations ---

#if defined(_WIN32)
bool PasswordManager::storeToWindowsCredentialManager(
    std::string_view target, std::string_view encryptedData) const {
    // No lock needed, const method accessing external system

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    // Convert target to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      target.length(), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR,
              "Failed to convert target to wide string for CredWriteW. Error: "
              "%lu",
              GetLastError());
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), target.length(),
                        &wideTarget[0], wideLen);

    cred.TargetName = &wideTarget[0];
    cred.CredentialBlobSize = static_cast<DWORD>(encryptedData.length());
    // CredentialBlob needs non-const pointer
    cred.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(encryptedData.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;  // Or CRED_PERSIST_ENTERPRISE if
                                                // appropriate

    // Use a fixed, non-sensitive username or derive one? Using fixed for
    // simplicity.
    static const std::wstring pmUser = L"AtomPasswordManagerUser";
    cred.UserName = const_cast<LPWSTR>(pmUser.c_str());

    if (CredWriteW(&cred, 0)) {
        // LOG_F(INFO, "Data stored successfully in Windows Credential Manager
        // for target: {}", target.data()); // Verbose
        return true;
    } else {
        DWORD error = GetLastError();
        LOG_F(ERROR,
              "Failed to store data in Windows Credential Manager for target: "
              "{}. Error: {}",
              target.data(), error);
        return false;
    }
}

std::string PasswordManager::retrieveFromWindowsCredentialManager(
    std::string_view target) const {
    // No lock needed, const method accessing external system

    PCREDENTIALW pCred = nullptr;
    std::string result = "";

    // Convert target to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      target.length(), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(
            ERROR,
            "Failed to convert target to wide string for CredReadW. Error: %lu",
            GetLastError());
        return "";
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), target.length(),
                        &wideTarget[0], wideLen);

    if (CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        if (pCred->CredentialBlobSize > 0 && pCred->CredentialBlob != nullptr) {
            result.assign(reinterpret_cast<char*>(pCred->CredentialBlob),
                          pCred->CredentialBlobSize);
        }
        CredFree(pCred);
        // LOG_F(INFO, "Data retrieved successfully from Windows Credential
        // Manager for target: {}", target.data()); // Verbose
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            LOG_F(ERROR,
                  "Failed to retrieve data from Windows Credential Manager for "
                  "target: {}. Error: {}",
                  target.data(), error);
        }
        // Return empty string if not found or error
    }
    return result;
}

bool PasswordManager::deleteFromWindowsCredentialManager(
    std::string_view target) const {
    // No lock needed, const method accessing external system

    // Convert target to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      target.length(), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR,
              "Failed to convert target to wide string for CredDeleteW. Error: "
              "%lu",
              GetLastError());
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), target.length(),
                        &wideTarget[0], wideLen);

    if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        // LOG_F(INFO, "Data deleted successfully from Windows Credential
        // Manager for target: {}", target.data()); // Verbose
        return true;
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            LOG_F(ERROR,
                  "Failed to delete data from Windows Credential Manager for "
                  "target: {}. Error: {}",
                  target.data(), error);
        }
        // Return true if not found (idempotent delete)
        return error == ERROR_NOT_FOUND;
    }
}

std::vector<std::string> PasswordManager::getAllWindowsCredentials() const {
    // No lock needed, const method accessing external system

    std::vector<std::string> results;
    DWORD count = 0;
    PCREDENTIALW* pCredentials = nullptr;

    // Enumerate credentials matching a pattern (e.g., "AtomPasswordManager*")
    // Using a wildcard might require specific permissions or configuration.
    // A safer approach might be to store an index credential.
    // For simplicity, let's assume enumeration works or use the index approach
    // like file fallback. Using a fixed prefix for enumeration:
    std::wstring filter =
        std::wstring(ATOM_PM_SERVICE_NAME.begin(), ATOM_PM_SERVICE_NAME.end()) +
        L"*";

    if (CredEnumerateW(filter.c_str(), 0, &count, &pCredentials)) {
        results.reserve(count);
        for (DWORD i = 0; i < count; ++i) {
            if (pCredentials[i]->TargetName) {
                // Convert wide string target back to UTF-8 std::string
                int utf8Len =
                    WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName,
                                        -1, nullptr, 0, nullptr, nullptr);
                if (utf8Len >
                    1) {  // Greater than 1 to account for null terminator
                    std::string target(utf8Len - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName,
                                        -1, &target[0], utf8Len, nullptr,
                                        nullptr);
                    results.push_back(std::move(target));
                }
            }
        }
        CredFree(pCredentials);
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            LOG_F(ERROR,
                  "Failed to enumerate Windows credentials with filter '{}'. "
                  "Error: {}",
                  std::string(filter.begin(), filter.end()).c_str(), error);
        }
    }
    return results;
}

#elif defined(__APPLE__)
// macOS Keychain implementation requires careful handling of CF types and
// memory management. The previous implementation provides a basic structure.
// Error handling and details need review.

// Helper function for macOS status codes
std::string GetMacOSStatusString(OSStatus status) {
    // Consider using SecCopyErrorMessageString for more descriptive errors if
    // available
    return "macOS Error: " + std::to_string(status);
}

bool PasswordManager::storeToMacKeychain(std::string_view service,
                                         std::string_view account,
                                         std::string_view encryptedData) const {
    // No lock needed

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);
    CFDataRef cfData =
        CFDataCreate(kCFAllocatorDefault,
                     reinterpret_cast<const UInt8*>(encryptedData.data()),
                     encryptedData.length());

    if (!cfService || !cfAccount || !cfData) {
        if (cfService)
            CFRelease(cfService);
        if (cfAccount)
            CFRelease(cfAccount);
        if (cfData)
            CFRelease(cfData);
        LOG_F(ERROR, "Failed to create CF types for Keychain storage.");
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    OSStatus status =
        SecItemCopyMatching(query, nullptr);  // Check if item exists

    if (status == errSecSuccess) {  // Item exists, update it
        CFMutableDictionaryRef update = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(update, kSecValueData, cfData);
        status = SecItemUpdate(query, update);
        CFRelease(update);
        if (status != errSecSuccess) {
            LOG_F(ERROR,
                  "Failed to update item in macOS Keychain (Service: {}, "
                  "Account: {}): {}",
                  service.data(), account.data(),
                  GetMacOSStatusString(status).c_str());
        }
    } else if (status == errSecItemNotFound) {  // Item doesn't exist, add it
        CFDictionarySetValue(query, kSecValueData, cfData);
        // Set accessibility - kSecAttrAccessibleWhenUnlockedThisDeviceOnly is a
        // reasonable default
        CFDictionarySetValue(query, kSecAttrAccessible,
                             kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
        status = SecItemAdd(query, nullptr);
        if (status != errSecSuccess) {
            LOG_F(ERROR,
                  "Failed to add item to macOS Keychain (Service: {}, Account: "
                  "{}): {}",
                  service.data(), account.data(),
                  GetMacOSStatusString(status).c_str());
        }
    } else {  // Other error during lookup
        LOG_F(ERROR,
              "Failed to query macOS Keychain (Service: {}, Account: {}): {}",
              service.data(), account.data(),
              GetMacOSStatusString(status).c_str());
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);
    CFRelease(cfData);

    return status == errSecSuccess;
}

std::string PasswordManager::retrieveFromMacKeychain(
    std::string_view service, std::string_view account) const {
    // No lock needed

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        if (cfService)
            CFRelease(cfService);
        if (cfAccount)
            CFRelease(cfAccount);
        LOG_F(ERROR, "Failed to create CF types for Keychain retrieval.");
        return "";
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
    CFDictionarySetValue(query, kSecReturnData,
                         kCFBooleanTrue);  // Request data back
    CFDictionarySetValue(query, kSecMatchLimit,
                         kSecMatchLimitOne);  // Expect only one match

    CFDataRef cfData = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfData);

    std::string result = "";
    if (status == errSecSuccess && cfData) {
        result.assign(reinterpret_cast<const char*>(CFDataGetBytePtr(cfData)),
                      CFDataGetLength(cfData));
        CFRelease(cfData);
    } else if (status != errSecItemNotFound) {
        LOG_F(ERROR,
              "Failed to retrieve item from macOS Keychain (Service: {}, "
              "Account: {}): {}",
              service.data(), account.data(),
              GetMacOSStatusString(status).c_str());
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    return result;
}

bool PasswordManager::deleteFromMacKeychain(std::string_view service,
                                            std::string_view account) const {
    // No lock needed

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        if (cfService)
            CFRelease(cfService);
        if (cfAccount)
            CFRelease(cfAccount);
        LOG_F(ERROR, "Failed to create CF types for Keychain deletion.");
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    OSStatus status = SecItemDelete(query);

    if (status != errSecSuccess && status != errSecItemNotFound) {
        LOG_F(ERROR,
              "Failed to delete item from macOS Keychain (Service: {}, "
              "Account: {}): {}",
              service.data(), account.data(),
              GetMacOSStatusString(status).c_str());
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    // Return true if deleted or not found (idempotent)
    return status == errSecSuccess || status == errSecItemNotFound;
}

std::vector<std::string> PasswordManager::getAllMacKeychainItems(
    std::string_view service) const {
    // No lock needed

    std::vector<std::string> results;
    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    if (!cfService) {
        LOG_F(ERROR, "Failed to create CFString for Keychain service name.");
        return results;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecMatchLimit,
                         kSecMatchLimitAll);  // Get all matches
    CFDictionarySetValue(
        query, kSecReturnAttributes,
        kCFBooleanTrue);  // We only need attributes (account name)

    CFArrayRef cfResults = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfResults);

    if (status == errSecSuccess && cfResults) {
        CFIndex count = CFArrayGetCount(cfResults);
        results.reserve(count);
        for (CFIndex i = 0; i < count; ++i) {
            CFDictionaryRef item =
                (CFDictionaryRef)CFArrayGetValueAtIndex(cfResults, i);
            CFStringRef cfAccount =
                (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            if (cfAccount) {
                CFIndex length = CFStringGetLength(cfAccount);
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(
                                      length, kCFStringEncodingUTF8) +
                                  1;
                std::string accountStr(maxSize, '\0');
                if (CFStringGetCString(cfAccount, &accountStr[0], maxSize,
                                       kCFStringEncodingUTF8)) {
                    accountStr.resize(strlen(
                        accountStr
                            .c_str()));  // Resize to actual C string length
                    results.push_back(std::move(accountStr));
                }
            }
        }
        CFRelease(cfResults);
    } else if (status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to list macOS Keychain items (Service: {}): {}",
              service.data(), GetMacOSStatusString(status).c_str());
    }

    CFRelease(query);
    CFRelease(cfService);

    return results;
}

#elif defined(__linux__) && defined(USE_LIBSECRET)
// Libsecret implementation requires GLib main loop integration for async
// operations, or careful use of sync functions. The previous sync
// implementation is used here for simplicity. Ensure GLib is initialized if
// using async or certain sync functions.

// Mutex for libsecret calls if needed (assuming library calls might not be
// thread-safe internally) std::mutex g_libsecretMutex; // Consider making this
// a member if multiple PasswordManager instances exist

bool PasswordManager::storeToLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name,
    std::string_view encryptedData) const {
    // std::lock_guard<std::mutex> lock(g_libsecretMutex); // Lock if needed

    const SecretSchema schema = {
        schema_name.data(),  // Use .data() for C API
        SECRET_SCHEMA_NONE,
        {
            // Define attributes used for lookup
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            // Add other attributes if needed, e.g., "service", "username"
            {nullptr, SecretSchemaAttributeType(0)}  // Null terminate
        }};

    GError* error = nullptr;
    // Store password with label and attributes
    gboolean success = secret_password_store_sync(
        &schema,
        SECRET_COLLECTION_DEFAULT,  // Or specific collection
        attribute_name.data(),      // Use attribute_name as the display label
        encryptedData.data(),
        nullptr,                                       // Cancellable
        &error, "atom_pm_key", attribute_name.data(),  // Attribute for lookup
        // Add more attributes here if defined in schema
        nullptr);  // Null terminate attributes

    if (!success) {
        if (error) {
            LOG_F(ERROR,
                  "Failed to store data in Linux keyring (Schema: {}, Key: "
                  "{}): {}",
                  schema_name.data(), attribute_name.data(), error->message);
            g_error_free(error);
        } else {
            LOG_F(ERROR,
                  "Failed to store data in Linux keyring (Schema: {}, Key: {}) "
                  "(Unknown error)",
                  schema_name.data(), attribute_name.data());
        }
        return false;
    }
    // LOG_F(INFO, "Data stored successfully in Linux keyring (Schema: {}, Key:
    // {})", schema_name.data(), attribute_name.data()); // Verbose
    return true;
}

std::string PasswordManager::retrieveFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // std::lock_guard<std::mutex> lock(g_libsecretMutex); // Lock if needed

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {{"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
         {nullptr, SecretSchemaAttributeType(0)}}};

    GError* error = nullptr;
    // Lookup password based on attributes
    gchar* secret = secret_password_lookup_sync(
        &schema,
        nullptr,  // Cancellable
        &error, "atom_pm_key",
        attribute_name.data(),  // Attribute value to match
        nullptr);               // Null terminate attributes

    std::string result = "";
    if (secret) {
        result.assign(secret);
        secret_password_free(secret);
    } else if (error) {
        LOG_F(ERROR,
              "Failed to retrieve data from Linux keyring (Schema: {}, Key: "
              "{}): {}",
              schema_name.data(), attribute_name.data(), error->message);
        g_error_free(error);
    }
    // Return empty string if not found or error
    return result;
}

bool PasswordManager::deleteFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // std::lock_guard<std::mutex> lock(g_libsecretMutex); // Lock if needed

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {{"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
         {nullptr, SecretSchemaAttributeType(0)}}};

    GError* error = nullptr;
    gboolean success = secret_password_clear_sync(
        &schema,
        nullptr,  // Cancellable
        &error, "atom_pm_key",
        attribute_name.data(),  // Attribute value to match
        nullptr);               // Null terminate attributes

    if (!success && error) {
        LOG_F(ERROR,
              "Failed to delete data from Linux keyring (Schema: {}, Key: {}): "
              "{}",
              schema_name.data(), attribute_name.data(), error->message);
        g_error_free(error);
    }
    // Return true if deleted or not found (idempotent)
    return success || !error;
}

std::vector<std::string> PasswordManager::getAllLinuxKeyringItems(
    std::string_view schema_name) const {
    // std::lock_guard<std::mutex> lock(g_libsecretMutex); // Lock if needed

    std::vector<std::string> results;
    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {{"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
         {nullptr, SecretSchemaAttributeType(0)}}};

    // Libsecret doesn't provide a direct way to enumerate all items.
    // Use a known index key to store a list of keys.
    std::string indexData =
        retrieveFromLinuxKeyring(schema_name, ATOM_PM_INDEX_KEY);
    if (!indexData.empty()) {
        try {
            json indexJson = json::parse(indexData);
            if (indexJson.is_array()) {
                for (const auto& item : indexJson) {
                    if (item.is_string()) {
                        results.push_back(item.get<std::string>());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR,
                  "Failed to parse index data from Linux keyring (Schema: {}): "
                  "{}",
                  schema_name.data(), e.what());
        }
    }
    return results;
}

#else  // Fallback to file-based storage

bool PasswordManager::storeToEncryptedFile(
    std::string_view identifier, std::string_view encryptedData) const {
    // No lock needed, const method

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to determine secure storage directory.");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath =
        storageDir / (sanitizedIdentifier + ".dat");

    try {
        std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
        if (!outFile) {
            throw std::runtime_error("Failed to open file for writing: " +
                                     filePath.string());
        }
        outFile.write(encryptedData.data(), encryptedData.size());
        outFile.close();
        if (!outFile) {  // Check for write errors after closing
            throw std::runtime_error("Failed to write data to file: " +
                                     filePath.string());
        }

        // Update index
        std::filesystem::path indexPath = storageDir / "index.json";
        std::vector<std::string> existingKeys;

        // Read existing index
        std::ifstream indexFile(indexPath);
        if (indexFile) {
            try {
                json indexJson;
                indexFile >> indexJson;
                if (indexJson.is_array()) {
                    for (const auto& item : indexJson) {
                        if (item.is_string()) {
                            existingKeys.push_back(item.get<std::string>());
                        }
                    }
                }
            } catch (...) {
                // Ignore parse errors
            }
        }

        // Add new key if not already present
        if (std::find(existingKeys.begin(), existingKeys.end(),
                      sanitizedIdentifier) == existingKeys.end()) {
            existingKeys.push_back(sanitizedIdentifier);
        }

        // Write updated index
        std::ofstream newIndexFile(indexPath,
                                   std::ios::binary | std::ios::trunc);
        if (newIndexFile) {
            json newIndexJson = existingKeys;
            newIndexFile << newIndexJson.dump(4);  // Pretty print JSON
        }

        // LOG_F(INFO, "Data stored successfully in file (Identifier: {})",
        // identifier.data()); // Verbose
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to store data in file (Identifier: {}): {}",
              identifier.data(), e.what());
        return false;
    }
}

std::string PasswordManager::retrieveFromEncryptedFile(
    std::string_view identifier) const {
    // No lock needed, const method

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to determine secure storage directory.");
        return "";
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath =
        storageDir / (sanitizedIdentifier + ".dat");

    try {
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            return "";  // Not found
        }
        std::string encryptedData((std::istreambuf_iterator<char>(inFile)),
                                  std::istreambuf_iterator<char>());
        return encryptedData;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to retrieve data from file (Identifier: {}): {}",
              identifier.data(), e.what());
        return "";
    }
}

bool PasswordManager::deleteFromEncryptedFile(
    std::string_view identifier) const {
    // No lock needed, const method

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to determine secure storage directory.");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath =
        storageDir / (sanitizedIdentifier + ".dat");

    try {
        std::error_code ec;
        if (std::filesystem::remove(filePath, ec) || ec.value() == ENOENT) {
            // Update index
            std::filesystem::path indexPath = storageDir / "index.json";
            std::vector<std::string> existingKeys;

            // Read existing index
            std::ifstream indexFile(indexPath);
            if (indexFile) {
                try {
                    json indexJson;
                    indexFile >> indexJson;
                    if (indexJson.is_array()) {
                        for (const auto& item : indexJson) {
                            if (item.is_string() && item.get<std::string>() !=
                                                        sanitizedIdentifier) {
                                existingKeys.push_back(item.get<std::string>());
                            }
                        }
                    }
                } catch (...) {
                    // Ignore parse errors
                }
            }

            // Write updated index
            std::ofstream newIndexFile(indexPath,
                                       std::ios::binary | std::ios::trunc);
            if (newIndexFile) {
                json newIndexJson = existingKeys;
                newIndexFile << newIndexJson.dump(4);  // Pretty print JSON
            }

            // LOG_F(INFO, "Data deleted successfully from file (Identifier:
            // {})", identifier.data()); // Verbose
            return true;
        } else {
            throw std::runtime_error(
                "Failed to delete file: " + filePath.string() + " (" +
                ec.message() + ")");
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to delete data from file (Identifier: {}): {}",
              identifier.data(), e.what());
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllEncryptedFileItems() const {
    // No lock needed, const method

    std::vector<std::string> keys;
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to determine secure storage directory.");
        return keys;
    }

    std::filesystem::path indexPath = storageDir / "index.json";

    try {
        std::ifstream indexFile(indexPath);
        if (indexFile) {
            json indexJson;
            indexFile >> indexJson;
            if (indexJson.is_array()) {
                for (const auto& item : indexJson) {
                    if (item.is_string()) {
                        keys.push_back(item.get<std::string>());
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to read index file: {}", e.what());
    }

    return keys;
}

#endif  // Platform-specific implementations

}  // namespace atom::secret