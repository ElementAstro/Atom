#include "password.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <thread>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sys/stat.h>

// 平台特定头文件
#if defined(_WIN32)
#include <wincred.h>
#include <windows.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(__linux__)
#include <mutex>
#if __has_include(<libsecret/secret.h>)
#include <libsecret/secret.h>
#define USE_LIBSECRET 1
#elif __has_include(<libgnome-keyring/gnome-keyring.h>)
#include <libgnome-keyring/gnome-keyring.h>
#define USE_GNOME_KEYRING 1
#else
#define USE_FILE_FALLBACK 1
#endif
#else
#define USE_FILE_FALLBACK 1
#endif

#include "atom/algorithm/base.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

// 常量定义
#define ATOM_PM_VERSION "2.0.0"
#define ATOM_PM_SERVICE_NAME "AtomPasswordManager"
#define ATOM_PM_SALT_SIZE 16
#define ATOM_PM_IV_SIZE 16
#define ATOM_PM_TAG_SIZE 16
#define ATOM_PM_PBKDF2_ITERATIONS 100000

using json = nlohmann::json;

namespace atom::secret {

// 线程安全的单例锁
#ifdef __linux__
std::mutex g_keyringMutex;
#endif

// 密码管理器实现
PasswordManager::PasswordManager()
    : lastActivity(std::chrono::system_clock::now()) {
    // 初始化OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    LOG_F(INFO, "PasswordManager initialized (version %s)", ATOM_PM_VERSION);
}

PasswordManager::~PasswordManager() {
    // 安全清除敏感数据
    lock();

    // 清理OpenSSL
    EVP_cleanup();
    ERR_free_strings();

    LOG_F(INFO, "PasswordManager destroyed safely");
}

bool PasswordManager::initialize(const std::string& masterPassword,
                                 const PasswordManagerSettings& settings) {
    if (masterPassword.empty()) {
        LOG_F(ERROR, "Cannot initialize with empty master password");
        return false;
    }

    this->settings = settings;

    // 生成随机盐值
    std::vector<unsigned char> salt(ATOM_PM_SALT_SIZE);
    if (RAND_bytes(salt.data(), salt.size()) != 1) {
        LOG_F(ERROR, "Failed to generate random salt");
        return false;
    }

    // 从主密码派生密钥
    try {
        masterKey = deriveKey(masterPassword, salt,
                              settings.encryptionOptions.keyIterations);
        isInitialized = true;
        isUnlocked = true;
        updateActivity();

        // 存储初始化数据（包含盐值和验证数据）
        std::string verificationData =
            "ATOM_PM_VERIFICATION_" + std::string(ATOM_PM_VERSION);
        std::vector<unsigned char> iv(ATOM_PM_IV_SIZE);
        if (RAND_bytes(iv.data(), iv.size()) != 1) {
            LOG_F(ERROR, "Failed to generate random IV");
            return false;
        }

        // 使用AES-GCM加密验证数据
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            LOG_F(ERROR, "Failed to create OpenSSL cipher context");
            return false;
        }

        // 初始化加密
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                               masterKey.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to initialize encryption");
            return false;
        }

        // 加密数据
        std::vector<unsigned char> encryptedData(verificationData.size() +
                                                 EVP_MAX_BLOCK_LENGTH);
        int len = 0;
        if (EVP_EncryptUpdate(ctx, encryptedData.data(), &len,
                              (const unsigned char*)verificationData.c_str(),
                              verificationData.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to encrypt verification data");
            return false;
        }

        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx, encryptedData.data() + len, &finalLen) !=
            1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to finalize encryption");
            return false;
        }

        len += finalLen;
        encryptedData.resize(len);

        // 获取认证标签
        std::vector<unsigned char> tag(ATOM_PM_TAG_SIZE);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(),
                                tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to get authentication tag");
            return false;
        }

        EVP_CIPHER_CTX_free(ctx);

        // 构建存储数据
        json initData;
        initData["version"] = ATOM_PM_VERSION;
        auto saltBase64 =
            algorithm::base64Encode(reinterpret_cast<const char*>(salt.data()),
                                    salt.size())
                .value();
        auto ivBase64 = algorithm::base64Encode(
                            reinterpret_cast<const char*>(iv.data()), iv.size())
                            .value();
        auto tagBase64 =
            algorithm::base64Encode(reinterpret_cast<const char*>(tag.data()),
                                    tag.size())
                .value();
        auto dataBase64 =
            algorithm::base64Encode(
                reinterpret_cast<const char*>(encryptedData.data()),
                encryptedData.size())
                .value();

        initData["salt"] = std::string(saltBase64.begin(), saltBase64.end());
        initData["iv"] = std::string(ivBase64.begin(), ivBase64.end());
        initData["tag"] = std::string(tagBase64.begin(), tagBase64.end());
        initData["data"] = std::string(dataBase64.begin(), dataBase64.end());

        std::string serializedData = initData.dump();

#if defined(_WIN32)
        if (!storeToWindowsCredentialManager("ATOM_PM_INIT", serializedData)) {
            LOG_F(ERROR,
                  "Failed to store initialization data to Windows Credential "
                  "Manager");
            return false;
        }
#elif defined(__APPLE__)
        if (!storeToMacKeychain(ATOM_PM_SERVICE_NAME, "ATOM_PM_INIT",
                                serializedData)) {
            LOG_F(ERROR,
                  "Failed to store initialization data to macOS Keychain");
            return false;
        }
#elif defined(__linux__) && defined(USE_LIBSECRET)
        if (!storeToLinuxKeyring(ATOM_PM_SERVICE_NAME, "ATOM_PM_INIT",
                                 serializedData)) {
            LOG_F(ERROR,
                  "Failed to store initialization data to Linux keyring");
            return false;
        }
#else
        if (!storeToEncryptedFile("ATOM_PM_INIT", serializedData)) {
            LOG_F(ERROR,
                  "Failed to store initialization data to encrypted file");
            return false;
        }
#endif

        LOG_F(INFO, "PasswordManager successfully initialized");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Initialization error: %s", e.what());
        isInitialized = false;
        return false;
    }
}

bool PasswordManager::unlock(const std::string& masterPassword) {
    if (isUnlocked) {
        LOG_F(INFO, "PasswordManager is already unlocked");
        return true;
    }

    if (masterPassword.empty()) {
        LOG_F(ERROR, "Empty master password provided");
        return false;
    }

    try {
        // 获取初始化数据
        std::string initDataStr;

#if defined(_WIN32)
        initDataStr = retrieveFromWindowsCredentialManager("ATOM_PM_INIT");
#elif defined(__APPLE__)
        initDataStr =
            retrieveFromMacKeychain(ATOM_PM_SERVICE_NAME, "ATOM_PM_INIT");
#elif defined(__linux__) && defined(USE_LIBSECRET)
        initDataStr =
            retrieveFromLinuxKeyring(ATOM_PM_SERVICE_NAME, "ATOM_PM_INIT");
#else
        initDataStr = retrieveFromEncryptedFile("ATOM_PM_INIT");
#endif

        if (initDataStr.empty()) {
            LOG_F(ERROR,
                  "No initialization data found. Manager not initialized.");
            return false;
        }

        // 解析初始化数据
        json initData = json::parse(initDataStr);
        auto saltStr =
            algorithm::base64Decode(initData["salt"].get<std::string>())
                .value();
        std::vector<unsigned char> salt(saltStr.begin(), saltStr.end());

        auto ivStr =
            algorithm::base64Decode(initData["iv"].get<std::string>()).value();
        std::vector<unsigned char> iv(ivStr.begin(), ivStr.end());

        auto tagStr =
            algorithm::base64Decode(initData["tag"].get<std::string>()).value();
        std::vector<unsigned char> tag(tagStr.begin(), tagStr.end());

        auto encryptedStr =
            algorithm::base64Decode(initData["data"].get<std::string>())
                .value();
        std::vector<unsigned char> encryptedData(encryptedStr.begin(),
                                                 encryptedStr.end());

        // 从主密码派生密钥
        masterKey = deriveKey(masterPassword, salt,
                              settings.encryptionOptions.keyIterations);

        // 验证密钥是否正确（通过解密验证数据）
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            LOG_F(ERROR, "Failed to create OpenSSL cipher context");
            return false;
        }

        // 初始化解密
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                               masterKey.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to initialize decryption");
            return false;
        }

        // 设置认证标签
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(),
                                tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to set authentication tag");
            return false;
        }

        // 解密数据
        std::vector<unsigned char> decryptedData(encryptedData.size());
        int len = 0;
        if (EVP_DecryptUpdate(ctx, decryptedData.data(), &len,
                              encryptedData.data(),
                              encryptedData.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR,
                  "Failed to decrypt verification data - incorrect master "
                  "password?");
            return false;
        }

        int finalLen = 0;
        int ret =
            EVP_DecryptFinal_ex(ctx, decryptedData.data() + len, &finalLen);
        EVP_CIPHER_CTX_free(ctx);

        if (ret != 1) {
            LOG_F(ERROR, "Authentication failed - incorrect master password");
            secureWipe(masterKey);
            return false;
        }

        len += finalLen;
        decryptedData.resize(len);

        std::string verificationStr(decryptedData.begin(), decryptedData.end());
        if (verificationStr.find("ATOM_PM_VERIFICATION_") != 0) {
            LOG_F(ERROR, "Verification data is invalid");
            secureWipe(masterKey);
            return false;
        }

        isUnlocked = true;
        updateActivity();

        // 加载缓存
        loadAllPasswords();

        LOG_F(INFO, "PasswordManager successfully unlocked");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unlock error: %s", e.what());
        secureWipe(masterKey);
        return false;
    }
}

void PasswordManager::lock() {
    if (!isUnlocked) {
        return;
    }

    // 清除缓存
    for (auto& [key, entry] : cachedPasswords) {
        secureWipe(entry.password);
        for (auto& prev : entry.previousPasswords) {
            secureWipe(prev);
        }
    }
    cachedPasswords.clear();

    // 清除主密钥
    secureWipe(masterKey);

    isUnlocked = false;
    LOG_F(INFO, "PasswordManager locked");
}

bool PasswordManager::changeMasterPassword(const std::string& currentPassword,
                                           const std::string& newPassword) {
    // 验证当前密码
    if (!unlock(currentPassword)) {
        LOG_F(ERROR, "Current password is incorrect");
        return false;
    }

    if (newPassword.empty()) {
        LOG_F(ERROR, "New password cannot be empty");
        return false;
    }

    try {
        // 导出所有当前密码
        std::vector<std::string> allKeys = getAllPlatformKeys();
        std::map<std::string, PasswordEntry> allEntries;

        for (const auto& key : allKeys) {
            if (key != "ATOM_PM_INIT") {  // 跳过初始化数据
                allEntries[key] = retrievePassword(key);
            }
        }

        // 锁定并重新初始化
        lock();

        if (!initialize(newPassword, settings)) {
            LOG_F(ERROR, "Failed to reinitialize with new master password");
            return false;
        }

        // 使用新密钥重新存储所有密码
        for (const auto& [key, entry] : allEntries) {
            if (!storePassword(key, entry)) {
                LOG_F(ERROR, "Failed to migrate password for key: %s",
                      key.c_str());
            }
        }

        LOG_F(INFO, "Master password changed successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Change master password error: %s", e.what());
        return false;
    }
}

bool PasswordManager::storePassword(const std::string& platformKey,
                                    const PasswordEntry& entry) {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot store password: PasswordManager is locked");
        return false;
    }

    if (platformKey.empty()) {
        LOG_F(ERROR, "Platform key cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 加密密码条目
        std::string encryptedData = encryptEntry(entry, masterKey);

        // 更新缓存
        cachedPasswords[platformKey] = entry;

        // 存储到平台特定存储
#if defined(_WIN32)
        if (!storeToWindowsCredentialManager(platformKey, encryptedData)) {
            LOG_F(ERROR,
                  "Failed to store password to Windows Credential Manager");
            return false;
        }
#elif defined(__APPLE__)
        if (!storeToMacKeychain(ATOM_PM_SERVICE_NAME, platformKey,
                                encryptedData)) {
            LOG_F(ERROR, "Failed to store password to macOS Keychain");
            return false;
        }
#elif defined(__linux__) && defined(USE_LIBSECRET)
        if (!storeToLinuxKeyring(ATOM_PM_SERVICE_NAME, platformKey,
                                 encryptedData)) {
            LOG_F(ERROR, "Failed to store password to Linux keyring");
            return false;
        }
#else
        if (!storeToEncryptedFile(platformKey, encryptedData)) {
            LOG_F(ERROR, "Failed to store password to encrypted file");
            return false;
        }
#endif

        LOG_F(INFO, "Password stored successfully for platform key: %s",
              platformKey.c_str());
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Store password error: %s", e.what());
        return false;
    }
}

PasswordEntry PasswordManager::retrievePassword(
    const std::string& platformKey) {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot retrieve password: PasswordManager is locked");
        return PasswordEntry{};
    }

    if (platformKey.empty()) {
        LOG_F(ERROR, "Platform key cannot be empty");
        return PasswordEntry{};
    }

    updateActivity();

    // 先检查缓存
    auto it = cachedPasswords.find(platformKey);
    if (it != cachedPasswords.end()) {
        LOG_F(INFO, "Password retrieved from cache for platform key: %s",
              platformKey.c_str());
        return it->second;
    }

    try {
        // 从平台特定存储获取加密数据
        std::string encryptedData;

#if defined(_WIN32)
        encryptedData = retrieveFromWindowsCredentialManager(platformKey);
#elif defined(__APPLE__)
        encryptedData =
            retrieveFromMacKeychain(ATOM_PM_SERVICE_NAME, platformKey);
#elif defined(__linux__) && defined(USE_LIBSECRET)
        encryptedData =
            retrieveFromLinuxKeyring(ATOM_PM_SERVICE_NAME, platformKey);
#else
        encryptedData = retrieveFromEncryptedFile(platformKey);
#endif

        if (encryptedData.empty()) {
            LOG_F(ERROR, "No password found for platform key: %s",
                  platformKey.c_str());
            return PasswordEntry{};
        }

        // 解密密码条目
        PasswordEntry entry = decryptEntry(encryptedData, masterKey);

        // 更新缓存
        cachedPasswords[platformKey] = entry;

        LOG_F(INFO, "Password retrieved successfully for platform key: %s",
              platformKey.c_str());
        return entry;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Retrieve password error: %s", e.what());
        return PasswordEntry{};
    }
}

bool PasswordManager::deletePassword(const std::string& platformKey) {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot delete password: PasswordManager is locked");
        return false;
    }

    if (platformKey.empty()) {
        LOG_F(ERROR, "Platform key cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 从缓存中删除
        cachedPasswords.erase(platformKey);

        // 从平台特定存储中删除
        bool success = false;

#if defined(_WIN32)
        success = deleteFromWindowsCredentialManager(platformKey);
#elif defined(__APPLE__)
        success = deleteFromMacKeychain(ATOM_PM_SERVICE_NAME, platformKey);
#elif defined(__linux__) && defined(USE_LIBSECRET)
        success = deleteFromLinuxKeyring(ATOM_PM_SERVICE_NAME, platformKey);
#else
        success = deleteFromEncryptedFile(platformKey);
#endif

        if (success) {
            LOG_F(INFO, "Password deleted successfully for platform key: %s",
                  platformKey.c_str());
            return true;
        } else {
            LOG_F(ERROR, "Failed to delete password for platform key: %s",
                  platformKey.c_str());
            return false;
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Delete password error: %s", e.what());
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllPlatformKeys() {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot get platform keys: PasswordManager is locked");
        return {};
    }

    updateActivity();

    try {
        std::vector<std::string> keys;

#if defined(_WIN32)
        keys = getAllWindowsCredentials();
#elif defined(__APPLE__)
        keys = getAllMacKeychainItems(ATOM_PM_SERVICE_NAME);
#elif defined(__linux__) && defined(USE_LIBSECRET)
        keys = getAllLinuxKeyringItems(ATOM_PM_SERVICE_NAME);
#else
        keys = getAllEncryptedFileItems();
#endif

        // 过滤掉内部使用的键
        keys.erase(std::remove_if(keys.begin(), keys.end(),
                                  [](const std::string& key) {
                                      return key == "ATOM_PM_INIT";
                                  }),
                   keys.end());

        LOG_F(INFO, "Retrieved %zu platform keys", keys.size());
        return keys;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Get all platform keys error: %s", e.what());
        return {};
    }
}

std::vector<std::string> PasswordManager::searchPasswords(
    const std::string& query) {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot search passwords: PasswordManager is locked");
        return {};
    }

    if (query.empty()) {
        LOG_F(ERROR, "Search query cannot be empty");
        return getAllPlatformKeys();
    }

    updateActivity();

    try {
        // 确保所有密码都在缓存中
        loadAllPasswords();

        std::vector<std::string> results;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        for (const auto& [key, entry] : cachedPasswords) {
            std::string lowerKey = key;
            std::string lowerUsername = entry.username;
            std::string lowerUrl = entry.url;
            std::string lowerNotes = entry.notes;

            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerUsername.begin(), lowerUsername.end(),
                           lowerUsername.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerNotes.begin(), lowerNotes.end(),
                           lowerNotes.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lowerKey.find(lowerQuery) != std::string::npos ||
                lowerUsername.find(lowerQuery) != std::string::npos ||
                lowerUrl.find(lowerQuery) != std::string::npos ||
                lowerNotes.find(lowerQuery) != std::string::npos) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Search for '%s' returned %zu results", query.c_str(),
              results.size());
        return results;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Search passwords error: %s", e.what());
        return {};
    }
}

std::vector<std::string> PasswordManager::filterByCategory(
    PasswordCategory category) {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot filter passwords: PasswordManager is locked");
        return {};
    }

    updateActivity();

    try {
        // 确保所有密码都在缓存中
        loadAllPasswords();

        std::vector<std::string> results;

        for (const auto& [key, entry] : cachedPasswords) {
            if (entry.category == category) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Filter by category %d returned %zu results",
              static_cast<int>(category), results.size());
        return results;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Filter by category error: %s", e.what());
        return {};
    }
}

std::string PasswordManager::generatePassword(int length, bool includeSpecial,
                                              bool includeNumbers,
                                              bool includeMixedCase) {
    if (length < 4) {
        length = 12;  // 最小长度为12
    }

    updateActivity();

    try {
        std::string chars = "abcdefghijklmnopqrstuvwxyz";

        if (includeMixedCase) {
            chars += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        }

        if (includeNumbers) {
            chars += "0123456789";
        }

        if (includeSpecial) {
            chars += "!@#$%^&*()-_=+[]{}|;:,.<>?/";
        }

        std::vector<unsigned char> randomData(length);
        if (RAND_bytes(randomData.data(), length) != 1) {
            LOG_F(ERROR, "Failed to generate random bytes");
            throw std::runtime_error("Failed to generate random bytes");
        }

        std::string password;
        password.reserve(length);

        for (int i = 0; i < length; ++i) {
            password += chars[randomData[i] % chars.length()];
        }

        // 确保包含必需的字符类型
        bool hasLower = false;
        bool hasUpper = false;
        bool hasDigit = false;
        bool hasSpecial = false;

        for (char c : password) {
            if (std::islower(c))
                hasLower = true;
            else if (std::isupper(c))
                hasUpper = true;
            else if (std::isdigit(c))
                hasDigit = true;
            else
                hasSpecial = true;
        }

        // 如果缺少必需的字符类型，替换一些字符
        if (includeMixedCase && !hasUpper) {
            password[0] = std::toupper(password[0]);
        }

        if (includeNumbers && !hasDigit) {
            password[1] = '0' + (randomData[0] % 10);
        }

        if (includeSpecial && !hasSpecial) {
            const char specials[] = "!@#$%^&*";
            password[2] = specials[randomData[1] % 8];
        }

        LOG_F(INFO, "Generated password of length %d", length);
        return password;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Generate password error: %s", e.what());
        return "";
    }
}

PasswordStrength PasswordManager::evaluatePasswordStrength(
    const std::string& password) {
    updateActivity();

    if (password.length() < 8) {
        return PasswordStrength::VeryWeak;
    }

    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (char c : password) {
        if (std::islower(c))
            hasLower = true;
        else if (std::isupper(c))
            hasUpper = true;
        else if (std::isdigit(c))
            hasDigit = true;
        else
            hasSpecial = true;
    }

    int strength = 0;
    if (hasLower)
        strength++;
    if (hasUpper)
        strength++;
    if (hasDigit)
        strength++;
    if (hasSpecial)
        strength++;

    if (password.length() >= 12)
        strength++;
    if (password.length() >= 16)
        strength++;

    // 检查常见模式
    if (std::regex_search(password, std::regex("(\\w)\\1\\1"))) {  // 重复字符
        strength--;
    }

    if (std::regex_search(password, std::regex("(123|abc|qwe)"))) {  // 常见序列
        strength--;
    }

    switch (strength) {
        case 0:
        case 1:
            return PasswordStrength::VeryWeak;
        case 2:
            return PasswordStrength::Weak;
        case 3:
            return PasswordStrength::Medium;
        case 4:
            return PasswordStrength::Strong;
        default:
            return PasswordStrength::VeryStrong;
    }
}

bool PasswordManager::exportPasswords(const std::string& filePath,
                                      const std::string& password) {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot export passwords: PasswordManager is locked");
        return false;
    }

    updateActivity();

    try {
        // 确保所有密码都在缓存中
        loadAllPasswords();

        // 准备导出数据
        json exportData;
        exportData["version"] = ATOM_PM_VERSION;
        exportData["timestamp"] =
            std::chrono::system_clock::now().time_since_epoch().count();
        exportData["entries"] = json::array();

        for (const auto& [key, entry] : cachedPasswords) {
            json entryJson;
            entryJson["key"] = key;
            entryJson["username"] = entry.username;
            entryJson["password"] = entry.password;
            entryJson["url"] = entry.url;
            entryJson["notes"] = entry.notes;
            entryJson["category"] = static_cast<int>(entry.category);
            entryJson["created"] = entry.created.time_since_epoch().count();
            entryJson["modified"] = entry.modified.time_since_epoch().count();

            json prevPasswordsJson = json::array();
            for (const auto& prevPwd : entry.previousPasswords) {
                prevPasswordsJson.push_back(prevPwd);
            }
            entryJson["previousPasswords"] = prevPasswordsJson;

            exportData["entries"].push_back(entryJson);
        }

        // 将导出数据序列化
        std::string serializedData = exportData.dump();

        // 使用提供的密码加密数据
        std::vector<unsigned char> salt(ATOM_PM_SALT_SIZE);
        if (RAND_bytes(salt.data(), salt.size()) != 1) {
            LOG_F(ERROR, "Failed to generate random salt for export");
            return false;
        }

        std::vector<unsigned char> exportKey =
            deriveKey(password, salt, ATOM_PM_PBKDF2_ITERATIONS);

        std::vector<unsigned char> iv(ATOM_PM_IV_SIZE);
        if (RAND_bytes(iv.data(), iv.size()) != 1) {
            LOG_F(ERROR, "Failed to generate random IV for export");
            return false;
        }

        // 使用AES-GCM加密数据
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            LOG_F(ERROR, "Failed to create OpenSSL cipher context");
            return false;
        }

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                               exportKey.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to initialize encryption for export");
            return false;
        }

        std::vector<unsigned char> encryptedData(serializedData.size() +
                                                 EVP_MAX_BLOCK_LENGTH);
        int len = 0;
        if (EVP_EncryptUpdate(ctx, encryptedData.data(), &len,
                              (const unsigned char*)serializedData.c_str(),
                              serializedData.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to encrypt export data");
            return false;
        }

        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx, encryptedData.data() + len, &finalLen) !=
            1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to finalize encryption for export");
            return false;
        }

        len += finalLen;
        encryptedData.resize(len);

        // 获取认证标签
        std::vector<unsigned char> tag(ATOM_PM_TAG_SIZE);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(),
                                tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to get authentication tag for export");
            return false;
        }

        EVP_CIPHER_CTX_free(ctx);

        // 构建最终导出文件
        json finalExport;
        finalExport["format"] = "ATOM_PM_EXPORT";
        finalExport["version"] = ATOM_PM_VERSION;
        finalExport["salt"] =
            algorithm::base64Encode(reinterpret_cast<const char*>(salt.data()),
                                    ATOM_PM_SALT_SIZE)
                .value();
        finalExport["iv"] =
            algorithm::base64Encode(reinterpret_cast<const char*>(iv.data()),
                                    ATOM_PM_IV_SIZE)
                .value();
        finalExport["tag"] =
            algorithm::base64Encode(reinterpret_cast<const char*>(tag.data()),
                                    ATOM_PM_TAG_SIZE)
                .value();
        finalExport["data"] =
            algorithm::base64Encode(
                reinterpret_cast<const char*>(encryptedData.data()),
                encryptedData.size())
                .value();

        // 写入文件
        std::ofstream outFile(filePath);
        if (!outFile.is_open()) {
            LOG_F(ERROR, "Failed to open export file for writing: %s",
                  filePath.c_str());
            return false;
        }

        outFile << finalExport.dump(4);  // Pretty print with 4-space indent
        outFile.close();

        // 安全清除临时敏感数据
        secureWipe(exportKey);

        LOG_F(INFO, "Successfully exported %zu password entries to %s",
              cachedPasswords.size(), filePath.c_str());
        return true;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Export passwords error: %s", e.what());
        return false;
    }
}

bool PasswordManager::importPasswords(const std::string& filePath,
                                      const std::string& password) {
    if (!isUnlocked) {
        LOG_F(ERROR, "Cannot import passwords: PasswordManager is locked");
        return false;
    }

    updateActivity();

    try {
        // 读取导出文件
        std::ifstream inFile(filePath);
        if (!inFile.is_open()) {
            LOG_F(ERROR, "Failed to open import file for reading: %s",
                  filePath.c_str());
            return false;
        }

        std::string fileContent((std::istreambuf_iterator<char>(inFile)),
                                std::istreambuf_iterator<char>());
        inFile.close();

        json importData = json::parse(fileContent);

        // 验证导出格式
        if (importData["format"] != "ATOM_PM_EXPORT") {
            LOG_F(ERROR, "Invalid export file format");
            return false;
        }

        std::string version = importData["version"];
        auto saltStr =
            algorithm::base64Decode(importData["salt"].get<std::string>())
                .value();
        std::vector<unsigned char> salt(saltStr.begin(), saltStr.end());

        auto ivStr =
            algorithm::base64Decode(importData["iv"].get<std::string>())
                .value();
        std::vector<unsigned char> iv(ivStr.begin(), ivStr.end());

        auto tagStr =
            algorithm::base64Decode(importData["tag"].get<std::string>())
                .value();
        std::vector<unsigned char> tag(tagStr.begin(), tagStr.end());

        auto encryptedStr =
            algorithm::base64Decode(importData["data"].get<std::string>())
                .value();
        std::vector<unsigned char> encryptedData(encryptedStr.begin(),
                                                 encryptedStr.end());

        // 从导入密码派生密钥
        std::vector<unsigned char> importKey =
            deriveKey(password, salt, ATOM_PM_PBKDF2_ITERATIONS);

        // 解密数据
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            LOG_F(ERROR, "Failed to create OpenSSL cipher context for import");
            return false;
        }

        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                               importKey.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to initialize decryption for import");
            return false;
        }

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(),
                                tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to set authentication tag for import");
            return false;
        }

        std::vector<unsigned char> decryptedData(encryptedData.size());
        int len = 0;
        if (EVP_DecryptUpdate(ctx, decryptedData.data(), &len,
                              encryptedData.data(),
                              encryptedData.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            LOG_F(ERROR, "Failed to decrypt import data - wrong password?");
            return false;
        }

        int finalLen = 0;
        int ret =
            EVP_DecryptFinal_ex(ctx, decryptedData.data() + len, &finalLen);
        EVP_CIPHER_CTX_free(ctx);

        if (ret != 1) {
            LOG_F(ERROR, "Authentication failed for import - wrong password");
            secureWipe(importKey);
            return false;
        }

        len += finalLen;
        decryptedData.resize(len);

        // 解析解密后的数据
        std::string decryptedStr(decryptedData.begin(), decryptedData.end());
        json entriesData = json::parse(decryptedStr);

        // 导入密码条目
        int importedCount = 0;
        for (const auto& entryJson : entriesData["entries"]) {
            PasswordEntry entry;
            std::string key = entryJson["key"];

            entry.username = entryJson["username"];
            entry.password = entryJson["password"];
            entry.url = entryJson["url"];
            entry.notes = entryJson["notes"];
            entry.category =
                static_cast<PasswordCategory>(entryJson["category"].get<int>());
            entry.created = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(entryJson["created"].get<int64_t>()));
            entry.modified =
                std::chrono::system_clock::time_point(std::chrono::milliseconds(
                    entryJson["modified"].get<int64_t>()));

            for (const auto& prevPwd : entryJson["previousPasswords"]) {
                entry.previousPasswords.push_back(prevPwd);
            }

            // 存储密码条目
            if (storePassword(key, entry)) {
                importedCount++;
            }
        }

        // 安全清除临时敏感数据
        secureWipe(importKey);

        LOG_F(INFO, "Successfully imported %d password entries from %s",
              importedCount, filePath.c_str());
        return importedCount > 0;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Import passwords error: %s", e.what());
        return false;
    }
}

void PasswordManager::updateSettings(
    const PasswordManagerSettings& newSettings) {
    settings = newSettings;
    updateActivity();
    LOG_F(INFO, "Updated PasswordManager settings");
}

PasswordManagerSettings PasswordManager::getSettings() const {
    return settings;
}

std::vector<std::string> PasswordManager::checkExpiredPasswords() {
    if (!isUnlocked) {
        LOG_F(ERROR,
              "Cannot check expired passwords: PasswordManager is locked");
        return {};
    }

    if (!settings.notifyOnPasswordExpiry) {
        return {};
    }

    updateActivity();

    try {
        // 确保所有密码都在缓存中
        loadAllPasswords();

        std::vector<std::string> expiredKeys;
        auto now = std::chrono::system_clock::now();
        auto expiryDuration =
            std::chrono::hours(24 * settings.passwordExpiryDays);

        for (const auto& [key, entry] : cachedPasswords) {
            if ((now - entry.modified) > expiryDuration) {
                expiredKeys.push_back(key);
            }
        }

        LOG_F(INFO, "Found %zu expired passwords", expiredKeys.size());
        return expiredKeys;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Check expired passwords error: %s", e.what());
        return {};
    }
}

void PasswordManager::setActivityCallback(std::function<void()> callback) {
    activityCallback = callback;
}

// Private methods
void PasswordManager::updateActivity() {
    lastActivity = std::chrono::system_clock::now();

    // 如果设置了回调，则调用
    if (activityCallback) {
        activityCallback();
    }

    // 检查自动锁定
    if (settings.autoLockTimeoutSeconds > 0) {
        auto timeSinceLastActivity =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - lastActivity)
                .count();

        if (timeSinceLastActivity > settings.autoLockTimeoutSeconds) {
            lock();
        }
    }
}

std::vector<unsigned char> PasswordManager::deriveKey(
    const std::string& masterPassword, const std::vector<unsigned char>& salt,
    int iterations) {
    std::vector<unsigned char> derivedKey(32);  // AES-256需要32字节密钥

    if (PKCS5_PBKDF2_HMAC(masterPassword.c_str(), masterPassword.length(),
                          salt.data(), salt.size(), iterations, EVP_sha256(),
                          derivedKey.size(), derivedKey.data()) != 1) {
        THROW_RUNTIME_ERROR("Failed to derive key from master password");
    }

    return derivedKey;
}

template <typename T>
void PasswordManager::secureWipe(T& data) {
    volatile auto* p = data.data();
    for (size_t i = 0; i < data.size(); i++) {
        p[i] = 0;
    }
}

void PasswordManager::loadAllPasswords() {
    // 如果已经解锁，加载所有密码到缓存
    if (!isUnlocked) {
        return;
    }

    std::vector<std::string> keys = getAllPlatformKeys();
    for (const auto& key : keys) {
        if (cachedPasswords.find(key) == cachedPasswords.end()) {
            // 从存储加载，但不通过retrievePassword从而避免递归
            std::string encryptedData;

#if defined(_WIN32)
            encryptedData = retrieveFromWindowsCredentialManager(key);
#elif defined(__APPLE__)
            encryptedData = retrieveFromMacKeychain(ATOM_PM_SERVICE_NAME, key);
#elif defined(__linux__) && defined(USE_LIBSECRET)
            encryptedData = retrieveFromLinuxKeyring(ATOM_PM_SERVICE_NAME, key);
#else
            encryptedData = retrieveFromEncryptedFile(key);
#endif

            if (!encryptedData.empty()) {
                try {
                    PasswordEntry entry =
                        decryptEntry(encryptedData, masterKey);
                    cachedPasswords[key] = entry;
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to decrypt entry for key %s: %s",
                          key.c_str(), e.what());
                }
            }
        }
    }
}

std::string PasswordManager::encryptEntry(
    const PasswordEntry& entry, const std::vector<unsigned char>& key) {
    // 序列化条目到JSON
    json entryJson;
    entryJson["username"] = entry.username;
    entryJson["password"] = entry.password;
    entryJson["url"] = entry.url;
    entryJson["notes"] = entry.notes;
    entryJson["category"] = static_cast<int>(entry.category);
    entryJson["created"] = entry.created.time_since_epoch().count();
    entryJson["modified"] = entry.modified.time_since_epoch().count();

    json prevPasswordsJson = json::array();
    for (const auto& prevPwd : entry.previousPasswords) {
        prevPasswordsJson.push_back(prevPwd);
    }
    entryJson["previousPasswords"] = prevPasswordsJson;

    std::string serializedEntry = entryJson.dump();

    // 生成随机IV
    std::vector<unsigned char> iv(ATOM_PM_IV_SIZE);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        THROW_RUNTIME_ERROR("Failed to generate random IV");
    }

    // 使用AES-GCM加密
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        THROW_RUNTIME_ERROR("Failed to create OpenSSL cipher context");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to initialize encryption");
    }

    std::vector<unsigned char> encryptedData(serializedEntry.size() +
                                             EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    if (EVP_EncryptUpdate(ctx, encryptedData.data(), &len,
                          (const unsigned char*)serializedEntry.c_str(),
                          serializedEntry.length()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to encrypt data");
    }

    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx, encryptedData.data() + len, &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to finalize encryption");
    }

    len += finalLen;
    encryptedData.resize(len);

    // 获取认证标签
    std::vector<unsigned char> tag(ATOM_PM_TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(),
                            tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to get authentication tag");
    }

    EVP_CIPHER_CTX_free(ctx);

    // 构建加密包
    json encJson;
    auto ivBase64 = algorithm::base64Encode(
                        reinterpret_cast<const char*>(iv.data()), iv.size())
                        .value();
    auto tagBase64 = algorithm::base64Encode(
                         reinterpret_cast<const char*>(tag.data()), tag.size())
                         .value();
    auto dataBase64 = algorithm::base64Encode(
                          reinterpret_cast<const char*>(encryptedData.data()),
                          encryptedData.size())
                          .value();

    encJson["iv"] = std::string(ivBase64.begin(), ivBase64.end());
    encJson["tag"] = std::string(tagBase64.begin(), tagBase64.end());
    encJson["data"] = std::string(dataBase64.begin(), dataBase64.end());

    return encJson.dump();
}

PasswordEntry PasswordManager::decryptEntry(
    const std::string& encryptedData, const std::vector<unsigned char>& key) {
    // 解析加密包
    json encJson = json::parse(encryptedData);
    auto ivResult = algorithm::base64Decode(encJson["iv"].get<std::string>());
    auto tagResult = algorithm::base64Decode(encJson["tag"].get<std::string>());
    auto dataResult =
        algorithm::base64Decode(encJson["data"].get<std::string>());

    if (!ivResult || !tagResult || !dataResult) {
        THROW_RUNTIME_ERROR("Failed to decode base64 data");
    }

    std::vector<unsigned char> iv(ivResult.value().begin(),
                                  ivResult.value().end());
    std::vector<unsigned char> tag(tagResult.value().begin(),
                                   tagResult.value().end());
    std::vector<unsigned char> data(dataResult.value().begin(),
                                    dataResult.value().end());

    // 使用AES-GCM解密
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        THROW_RUNTIME_ERROR("Failed to create OpenSSL cipher context");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to initialize decryption");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(),
                            tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to set authentication tag");
    }

    std::vector<unsigned char> decryptedData(data.size());
    int len = 0;
    if (EVP_DecryptUpdate(ctx, decryptedData.data(), &len, data.data(),
                          data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to decrypt data");
    }

    int finalLen = 0;
    int ret = EVP_DecryptFinal_ex(ctx, decryptedData.data() + len, &finalLen);
    EVP_CIPHER_CTX_free(ctx);

    if (ret != 1) {
        THROW_RUNTIME_ERROR("Authentication failed - data may be corrupted");
    }

    len += finalLen;
    decryptedData.resize(len);

    // 解析JSON
    std::string decryptedStr(decryptedData.begin(), decryptedData.end());
    json entryJson = json::parse(decryptedStr);

    PasswordEntry entry;
    entry.username = entryJson["username"];
    entry.password = entryJson["password"];
    entry.url = entryJson["url"];
    entry.notes = entryJson["notes"];
    entry.category =
        static_cast<PasswordCategory>(entryJson["category"].get<int>());

    int64_t createdTime = entryJson["created"].get<int64_t>();
    int64_t modifiedTime = entryJson["modified"].get<int64_t>();

    entry.created = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(createdTime));
    entry.modified = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(modifiedTime));

    for (const auto& prevPwd : entryJson["previousPasswords"]) {
        entry.previousPasswords.push_back(prevPwd);
    }

    return entry;
}

// 平台特定实现
#if defined(_WIN32)
bool PasswordManager::storeToWindowsCredentialManager(
    const std::string& target, const std::string& encryptedData) {
    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    std::wstring wideTarget(target.begin(), target.end());
    cred.TargetName = const_cast<LPWSTR>(wideTarget.c_str());
    cred.CredentialBlobSize = encryptedData.length();
    cred.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(encryptedData.c_str()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = const_cast<LPWSTR>(L"ATOM_PM_USER");

    if (CredWriteW(&cred, 0)) {
        LOG_F(INFO,
              "Data stored successfully in Windows Credential Manager for "
              "target: %s",
              target.c_str());
        return true;
    } else {
        DWORD error = GetLastError();
        LOG_F(ERROR,
              "Failed to store data in Windows Credential Manager for target: "
              "%s. Error: %lu",
              target.c_str(), error);
        return false;
    }
}

std::string PasswordManager::retrieveFromWindowsCredentialManager(
    const std::string& target) {
    PCREDENTIALW cred;
    std::wstring wideTarget(target.begin(), target.end());
    if (CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        std::string encryptedData(reinterpret_cast<char*>(cred->CredentialBlob),
                                  cred->CredentialBlobSize);
        CredFree(cred);
        LOG_F(INFO,
              "Data retrieved successfully from Windows Credential Manager for "
              "target: %s",
              target.c_str());
        return encryptedData;
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            LOG_F(ERROR,
                  "Failed to retrieve data from Windows Credential Manager for "
                  "target: %s. Error: %lu",
                  target.c_str(), error);
        }
        return "";
    }
}

bool PasswordManager::deleteFromWindowsCredentialManager(
    const std::string& target) {
    std::wstring wideTarget(target.begin(), target.end());
    if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        LOG_F(INFO,
              "Data deleted successfully from Windows Credential Manager for "
              "target: %s",
              target.c_str());
        return true;
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            LOG_F(ERROR,
                  "Failed to delete data from Windows Credential Manager for "
                  "target: %s. Error: %lu",
                  target.c_str(), error);
        }
        return error == ERROR_NOT_FOUND;  // 如果条目不存在，也视为成功删除
    }
}

std::vector<std::string> PasswordManager::getAllWindowsCredentials() {
    std::vector<std::string> results;
    DWORD count;
    PCREDENTIALW* credentials;

    if (CredEnumerateW(L"ATOM_PM_*", 0, &count, &credentials)) {
        for (DWORD i = 0; i < count; i++) {
            std::wstring wideTarget(credentials[i]->TargetName);
            std::string target(wideTarget.begin(), wideTarget.end());
            results.push_back(target);
        }
        CredFree(credentials);
    } else {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            LOG_F(ERROR, "Failed to enumerate Windows credentials. Error: %lu",
                  error);
        }
    }

    return results;
}

#elif defined(__APPLE__)
bool PasswordManager::storeToMacKeychain(const std::string& service,
                                         const std::string& account,
                                         const std::string& encryptedData) {
    // 检查是否已存在
    SecKeychainItemRef itemRef = nullptr;
    OSStatus status = SecKeychainFindGenericPassword(
        nullptr, service.length(), service.c_str(), account.length(),
        account.c_str(), nullptr, nullptr, &itemRef);

    if (status == errSecSuccess && itemRef) {
        // 更新现有项
        status = SecKeychainItemModifyAttributesAndData(
            itemRef, nullptr, encryptedData.length(), encryptedData.c_str());
        CFRelease(itemRef);

        if (status == errSecSuccess) {
            LOG_F(INFO,
                  "Data updated successfully in macOS Keychain for service: "
                  "%s, account: %s",
                  service.c_str(), account.c_str());
            return true;
        } else {
            LOG_F(ERROR,
                  "Failed to update data in macOS Keychain for service: %s, "
                  "account: %s. Error: %d",
                  service.c_str(), account.c_str(), status);
            return false;
        }
    } else {
        // 添加新项
        status = SecKeychainAddGenericPassword(
            nullptr, service.length(), service.c_str(), account.length(),
            account.c_str(), encryptedData.length(), encryptedData.c_str(),
            nullptr);

        if (status == errSecSuccess) {
            LOG_F(INFO,
                  "Data stored successfully in macOS Keychain for service: %s, "
                  "account: %s",
                  service.c_str(), account.c_str());
            return true;
        } else {
            LOG_F(ERROR,
                  "Failed to store data in macOS Keychain for service: %s, "
                  "account: %s. Error: %d",
                  service.c_str(), account.c_str(), status);
            return false;
        }
    }
}

std::string PasswordManager::retrieveFromMacKeychain(
    const std::string& service, const std::string& account) {
    void* dataOut;
    UInt32 dataOutLength;

    OSStatus status = SecKeychainFindGenericPassword(
        nullptr, service.length(), service.c_str(), account.length(),
        account.c_str(), &dataOutLength, &dataOut, nullptr);

    if (status == errSecSuccess) {
        std::string encryptedData(static_cast<char*>(dataOut), dataOutLength);
        SecKeychainItemFreeContent(nullptr, dataOut);
        LOG_F(INFO,
              "Data retrieved successfully from macOS Keychain for service: "
              "%s, account: %s",
              service.c_str(), account.c_str());
        return encryptedData;
    } else {
        if (status != errSecItemNotFound) {
            LOG_F(ERROR,
                  "Failed to retrieve data from macOS Keychain for service: "
                  "%s, account: %s. Error: %d",
                  service.c_str(), account.c_str(), status);
        }
        return "";
    }
}

bool PasswordManager::deleteFromMacKeychain(const std::string& service,
                                            const std::string& account) {
    SecKeychainItemRef itemRef;
    OSStatus status = SecKeychainFindGenericPassword(
        nullptr, service.length(), service.c_str(), account.length(),
        account.c_str(), nullptr, nullptr, &itemRef);

    if (status == errSecSuccess) {
        status = SecKeychainItemDelete(itemRef);
        CFRelease(itemRef);

        if (status == errSecSuccess) {
            LOG_F(INFO,
                  "Data deleted successfully from macOS Keychain for service: "
                  "%s, account: %s",
                  service.c_str(), account.c_str());
            return true;
        } else {
            LOG_F(ERROR,
                  "Failed to delete data from macOS Keychain for service: %s, "
                  "account: %s. Error: %d",
                  service.c_str(), account.c_str(), status);
            return false;
        }
    } else if (status == errSecItemNotFound) {
        // 如果条目不存在，也视为成功删除
        return true;
    } else {
        LOG_F(ERROR,
              "Failed to find data for deletion in macOS Keychain for service: "
              "%s, account: %s. Error: %d",
              service.c_str(), account.c_str(), status);
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllMacKeychainItems(
    const std::string& service) {
    std::vector<std::string> results;

    // 使用SecItemCopyMatching查找匹配的钥匙串项
    CFMutableDictionaryRef query =
        CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);

    CFStringRef serviceKey = CFStringCreateWithCString(nullptr, service.c_str(),
                                                       kCFStringEncodingUTF8);

    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, serviceKey);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);

    CFArrayRef items = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&items);

    if (status == errSecSuccess && items) {
        CFIndex count = CFArrayGetCount(items);
        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef item =
                (CFDictionaryRef)CFArrayGetValueAtIndex(items, i);
            CFStringRef accountName =
                (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            if (accountName) {
                char buffer[1024];
                if (CFStringGetCString(accountName, buffer, sizeof(buffer),
                                       kCFStringEncodingUTF8)) {
                    results.push_back(buffer);
                }
            }
        }
        CFRelease(items);
    } else if (status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to list macOS Keychain items. Error: %d", status);
    }

    CFRelease(serviceKey);
    CFRelease(query);

    return results;
}

#elif defined(__linux__) && defined(USE_LIBSECRET)
bool PasswordManager::storeToLinuxKeyring(const std::string& schema_name,
                                          const std::string& attribute_name,
                                          const std::string& encryptedData) {
    std::unique_lock<std::mutex> lock(g_keyringMutex);

    const SecretSchema schema = {
        schema_name.c_str(),
        SECRET_SCHEMA_NONE,
        {{"attribute", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, 0}}};

    GError* error = nullptr;
    bool success = secret_password_store_sync(
        &schema, SECRET_COLLECTION_DEFAULT, "AtomPasswordManager",
        encryptedData.c_str(), nullptr, &error, "attribute",
        attribute_name.c_str(), nullptr);

    if (!success) {
        if (error) {
            LOG_F(ERROR,
                  "Failed to store data in Linux keyring for schema: %s, "
                  "attribute: %s. Error: %s",
                  schema_name.c_str(), attribute_name.c_str(), error->message);
            g_error_free(error);
        } else {
            LOG_F(ERROR,
                  "Failed to store data in Linux keyring for schema: %s, "
                  "attribute: %s",
                  schema_name.c_str(), attribute_name.c_str());
        }
        return false;
    }

    LOG_F(INFO,
          "Data stored successfully in Linux keyring for schema: %s, "
          "attribute: %s",
          schema_name.c_str(), attribute_name.c_str());
    return true;
}

std::string PasswordManager::retrieveFromLinuxKeyring(
    const std::string& schema_name, const std::string& attribute_name) {
    std::unique_lock<std::mutex> lock(g_keyringMutex);

    const SecretSchema schema = {
        schema_name.c_str(),
        SECRET_SCHEMA_NONE,
        {{"attribute", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, 0}}};

    GError* error = nullptr;
    gchar* secret = secret_password_lookup_sync(
        &schema, nullptr, &error, "attribute", attribute_name.c_str(), nullptr);

    if (error) {
        LOG_F(ERROR,
              "Failed to retrieve data from Linux keyring for schema: %s, "
              "attribute: %s. Error: %s",
              schema_name.c_str(), attribute_name.c_str(), error->message);
        g_error_free(error);
        return "";
    }

    if (!secret) {
        return "";
    }

    std::string result(secret);
    secret_password_free(secret);

    LOG_F(INFO,
          "Data retrieved successfully from Linux keyring for schema: %s, "
          "attribute: %s",
          schema_name.c_str(), attribute_name.c_str());
    return result;
}

bool PasswordManager::deleteFromLinuxKeyring(
    const std::string& schema_name, const std::string& attribute_name) {
    std::unique_lock<std::mutex> lock(g_keyringMutex);

    const SecretSchema schema = {
        schema_name.c_str(),
        SECRET_SCHEMA_NONE,
        {{"attribute", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, 0}}};

    GError* error = nullptr;
    gboolean result = secret_password_clear_sync(
        &schema, nullptr, &error, "attribute", attribute_name.c_str(), nullptr);

    if (error) {
        LOG_F(ERROR,
              "Failed to delete data from Linux keyring for schema: %s, "
              "attribute: %s. Error: %s",
              schema_name.c_str(), attribute_name.c_str(), error->message);
        g_error_free(error);
        return false;
    }

    if (result) {
        LOG_F(INFO,
              "Data deleted successfully from Linux keyring for schema: %s, "
              "attribute: %s",
              schema_name.c_str(), attribute_name.c_str());
    } else {
        LOG_F(INFO,
              "No data found to delete in Linux keyring for schema: %s, "
              "attribute: %s",
              schema_name.c_str(), attribute_name.c_str());
    }

    return true;
}

std::vector<std::string> PasswordManager::getAllLinuxKeyringItems(
    const std::string& schema_name) {
    std::unique_lock<std::mutex> lock(g_keyringMutex);
    std::vector<std::string> results;

    const SecretSchema schema = {
        schema_name.c_str(),
        SECRET_SCHEMA_NONE,
        {{"attribute", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, 0}}};

    // 由于libsecret API限制，无法直接枚举所有项
    // 这里使用一个约定的方式存储密钥索引
    std::string indexStr =
        retrieveFromLinuxKeyring(schema_name, "ATOM_PM_INDEX");

    if (!indexStr.empty()) {
        try {
            json indexJson = json::parse(indexStr);
            if (indexJson.is_array()) {
                for (const auto& item : indexJson) {
                    results.push_back(item.get<std::string>());
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse Linux keyring index: %s", e.what());
        }
    }

    return results;
}

#else
// 文件存储后备方案的实现
bool PasswordManager::storeToEncryptedFile(const std::string& identifier,
                                           const std::string& encryptedData) {
    // 获取安全的存储目录
    std::string storageDir;

#if defined(_WIN32)
    char* appDataPath;
    size_t pathLen;
    _dupenv_s(&appDataPath, &pathLen, "APPDATA");
    if (appDataPath) {
        storageDir = std::string(appDataPath) + "\\AtomPasswordManager";
        free(appDataPath);
    } else {
        storageDir = ".\\AtomPasswordManager";
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        storageDir = std::string(homeDir) + "/.atom_pm";
    } else {
        storageDir = "./atom_pm";
    }
#endif

    // 创建目录（如果不存在）
#if defined(_WIN32)
    CreateDirectoryA(storageDir.c_str(), nullptr);
#else
    mkdir(storageDir.c_str(), 0700);
#endif

    // 构建文件路径
    std::string filePath = storageDir + "/" + identifier + ".dat";

    // 写入文件
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        LOG_F(ERROR, "Failed to open file for writing: %s", filePath.c_str());
        return false;
    }

    outFile.write(encryptedData.c_str(), encryptedData.length());
    outFile.close();

    if (outFile.fail()) {
        LOG_F(ERROR, "Failed to write to file: %s", filePath.c_str());
        return false;
    }

    // 更新索引
    std::string indexPath = storageDir + "/index.json";
    std::vector<std::string> existingKeys;

    // 读取现有索引
    std::ifstream indexFile(indexPath);
    if (indexFile.is_open()) {
        try {
            json indexJson;
            indexFile >> indexJson;
            indexFile.close();

            if (indexJson.is_array()) {
                for (const auto& item : indexJson) {
                    existingKeys.push_back(item.get<std::string>());
                }
            }
        } catch (...) {
            // 忽略解析错误
        }
    }

    // 添加新键（如果不存在）
    if (std::find(existingKeys.begin(), existingKeys.end(), identifier) ==
        existingKeys.end()) {
        existingKeys.push_back(identifier);
    }

    // 写回索引
    std::ofstream newIndexFile(indexPath);
    if (newIndexFile.is_open()) {
        json newIndexJson = existingKeys;
        newIndexFile << newIndexJson.dump();
        newIndexFile.close();
    }

    LOG_F(INFO, "Data stored successfully in file for identifier: %s",
          identifier.c_str());
    return true;
}

std::string PasswordManager::retrieveFromEncryptedFile(
    const std::string& identifier) {
    // 获取存储目录
    std::string storageDir;

#if defined(_WIN32)
    char* appDataPath;
    size_t pathLen;
    _dupenv_s(&appDataPath, &pathLen, "APPDATA");
    if (appDataPath) {
        storageDir = std::string(appDataPath) + "\\AtomPasswordManager";
        free(appDataPath);
    } else {
        storageDir = ".\\AtomPasswordManager";
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        storageDir = std::string(homeDir) + "/.atom_pm";
    } else {
        storageDir = "./atom_pm";
    }
#endif

    // 构建文件路径
    std::string filePath = storageDir + "/" + identifier + ".dat";

    // 读取文件
    std::ifstream inFile(filePath, std::ios::binary | std::ios::ate);
    if (!inFile.is_open()) {
        return "";
    }

    std::streamsize size = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!inFile.read(buffer.data(), size)) {
        LOG_F(ERROR, "Failed to read from file: %s", filePath.c_str());
        return "";
    }

    LOG_F(INFO, "Data retrieved successfully from file for identifier: %s",
          identifier.c_str());
    return std::string(buffer.begin(), buffer.end());
}

bool PasswordManager::deleteFromEncryptedFile(const std::string& identifier) {
    // 获取存储目录
    std::string storageDir;

#if defined(_WIN32)
    char* appDataPath;
    size_t pathLen;
    _dupenv_s(&appDataPath, &pathLen, "APPDATA");
    if (appDataPath) {
        storageDir = std::string(appDataPath) + "\\AtomPasswordManager";
        free(appDataPath);
    } else {
        storageDir = ".\\AtomPasswordManager";
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        storageDir = std::string(homeDir) + "/.atom_pm";
    } else {
        storageDir = "./atom_pm";
    }
#endif

    // 构建文件路径
    std::string filePath = storageDir + "/" + identifier + ".dat";

    // 删除文件
#if defined(_WIN32)
    if (DeleteFileA(filePath.c_str()) ||
        GetLastError() == ERROR_FILE_NOT_FOUND) {
#else
    if (remove(filePath.c_str()) == 0 || errno == ENOENT) {
#endif
        // 更新索引
        std::string indexPath = storageDir + "/index.json";
        std::vector<std::string> existingKeys;

        // 读取现有索引
        std::ifstream indexFile(indexPath);
        if (indexFile.is_open()) {
            try {
                json indexJson;
                indexFile >> indexJson;
                indexFile.close();

                if (indexJson.is_array()) {
                    for (const auto& item : indexJson) {
                        std::string key = item.get<std::string>();
                        if (key != identifier) {
                            existingKeys.push_back(key);
                        }
                    }
                }
            } catch (...) {
                // 忽略解析错误
            }
        }

        // 写回索引
        std::ofstream newIndexFile(indexPath);
        if (newIndexFile.is_open()) {
            json newIndexJson = existingKeys;
            newIndexFile << newIndexJson.dump();
            newIndexFile.close();
        }

        LOG_F(INFO, "Data deleted successfully from file for identifier: %s",
              identifier.c_str());
        return true;
    } else {
        LOG_F(ERROR, "Failed to delete file: %s", filePath.c_str());
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllEncryptedFileItems() {
    // 获取存储目录
    std::string storageDir;

#if defined(_WIN32)
    char* appDataPath;
    size_t pathLen;
    _dupenv_s(&appDataPath, &pathLen, "APPDATA");
    if (appDataPath) {
        storageDir = std::string(appDataPath) + "\\AtomPasswordManager";
        free(appDataPath);
    } else {
        storageDir = ".\\AtomPasswordManager";
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        storageDir = std::string(homeDir) + "/.atom_pm";
    } else {
        storageDir = "./atom_pm";
    }
#endif

    // 读取索引文件
    std::string indexPath = storageDir + "/index.json";
    std::vector<std::string> keys;

    std::ifstream indexFile(indexPath);
    if (indexFile.is_open()) {
        try {
            json indexJson;
            indexFile >> indexJson;
            indexFile.close();

            if (indexJson.is_array()) {
                for (const auto& item : indexJson) {
                    keys.push_back(item.get<std::string>());
                }
            }
        } catch (...) {
            // 忽略解析错误
        }
    }

    return keys;
}
#endif

}  // namespace atom::secret