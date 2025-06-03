#include "storage.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "spdlog/spdlog.h"

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <wincred.h>
#include <shlobj.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#endif
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <sys/stat.h>
#elif defined(__linux__)
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#if __has_include(<libsecret/secret.h>)
#include <libsecret/secret.h>
#define USE_LIBSECRET 1
#else
#define USE_FILE_FALLBACK 1
#endif
#else
#define USE_FILE_FALLBACK 1
#endif

namespace {

/**
 * @brief Gets secure storage directory for file fallback
 */
std::filesystem::path getSecureStorageDirectory(std::string_view appName) {
    std::filesystem::path storageDir;

#if defined(_WIN32)
    PWSTR path = nullptr;
    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        storageDir = path;
        storageDir /= appName;
        CoTaskMemFree(path);
    } else {
        char* appDataPath = nullptr;
        size_t pathLen;
        _dupenv_s(&appDataPath, &pathLen, "LOCALAPPDATA");
        if (appDataPath) {
            storageDir = appDataPath;
            storageDir /= appName;
            free(appDataPath);
        } else {
            storageDir = std::string(".") + std::string(appName);
            spdlog::warn(
                "Could not determine LocalAppData path, using current "
                "directory");
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
        storageDir /= "Library/Application Support";
        storageDir /= appName;
#else
        storageDir /= ".local/share";
        storageDir /= appName;
#endif
    } else {
        storageDir = std::string(".") + std::string(appName);
        spdlog::warn(
            "Could not determine HOME directory, using current directory");
    }
#else
    storageDir = std::string(".") + std::string(appName);
    spdlog::warn("Unknown platform, using current directory for storage");
#endif

    std::error_code ec;
    if (!std::filesystem::exists(storageDir, ec) && !ec) {
        if (!std::filesystem::create_directories(storageDir, ec) && ec) {
            spdlog::error("Failed to create storage directory '{}': {}",
                          storageDir.string(), ec.message());
        }
#if !defined(_WIN32)
        chmod(storageDir.c_str(), 0700);
#endif
    } else if (ec) {
        spdlog::error("Failed to check existence of storage directory '{}': {}",
                      storageDir.string(), ec.message());
    }

    return storageDir;
}

/**
 * @brief Sanitizes identifier for use as filename
 */
std::string sanitizeIdentifier(std::string_view identifier) {
    std::string sanitized;
    sanitized.reserve(identifier.length());

    for (char c : identifier) {
        if (std::isalnum(c) || c == '-' || c == '_') {
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

#if defined(__APPLE__)
/**
 * @brief Helper function for macOS status codes
 */
std::string getMacOSStatusString(OSStatus status) {
    return "macOS Error: " + std::to_string(status);
}
#endif

}  // namespace

namespace atom::secret {

/**
 * @brief Base FileSecureStorage class implementation (used as fallback)
 */
class FileSecureStorage : public SecureStorage {
private:
    std::string appName_;
    std::filesystem::path storageDir_;

public:
    explicit FileSecureStorage(std::string_view appName)
        : appName_(appName), storageDir_(getSecureStorageDirectory(appName)) {
        spdlog::info("Using file-based secure storage at: {}",
                     storageDir_.string());
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for file storage");
            return false;
        }

        std::string sanitizedKey = sanitizeIdentifier(key);
        std::filesystem::path filePath = storageDir_ / (sanitizedKey + ".dat");

        try {
            std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
            if (!outFile) {
                throw std::runtime_error("Failed to open file for writing: " +
                                         filePath.string());
            }

            outFile.write(data.data(), data.size());
            outFile.close();

            if (!outFile) {
                throw std::runtime_error("Failed to write data to file: " +
                                         filePath.string());
            }

            updateIndex(sanitizedKey, true);
            spdlog::debug("Data stored successfully to file: {}", sanitizedKey);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to store data in file (key: {}): {}", key,
                          e.what());
            return false;
        }
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for file retrieval");
            return "";
        }

        std::string sanitizedKey = sanitizeIdentifier(key);
        std::filesystem::path filePath = storageDir_ / (sanitizedKey + ".dat");

        try {
            std::ifstream inFile(filePath, std::ios::binary);
            if (!inFile) {
                spdlog::debug("File not found for key: {}", key);
                return "";
            }

            std::string data((std::istreambuf_iterator<char>(inFile)),
                             std::istreambuf_iterator<char>());
            return data;
        } catch (const std::exception& e) {
            spdlog::error("Failed to retrieve data from file (key: {}): {}",
                          key, e.what());
            return "";
        }
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for file removal");
            return false;
        }

        std::string sanitizedKey = sanitizeIdentifier(key);
        std::filesystem::path filePath = storageDir_ / (sanitizedKey + ".dat");

        try {
            std::error_code ec;
            if (std::filesystem::remove(filePath, ec) || ec.value() == ENOENT) {
                updateIndex(sanitizedKey, false);
                spdlog::debug("Successfully removed file for key: {}", key);
                return true;
            } else {
                throw std::runtime_error(
                    "Failed to delete file: " + filePath.string() + " (" +
                    ec.message() + ")");
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to remove data file (key: {}): {}", key,
                          e.what());
            return false;
        }
    }

    std::vector<std::string> getAllKeys() const override {
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
                for (const auto& entry :
                     std::filesystem::directory_iterator(storageDir_)) {
                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".dat") {
                        std::string filename = entry.path().filename().string();
                        filename = filename.substr(0, filename.length() - 4);
                        keys.push_back(filename);
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to read index file: {}", e.what());
        }

        return keys;
    }

private:
    void updateIndex(const std::string& key, bool add) const {
        std::filesystem::path indexPath = storageDir_ / "index.txt";
        std::vector<std::string> keys = getAllKeys();

        if (add) {
            if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
                keys.push_back(key);
            }
        } else {
            keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
        }

        try {
            std::ofstream indexFile(indexPath,
                                    std::ios::binary | std::ios::trunc);
            if (indexFile) {
                for (const auto& k : keys) {
                    indexFile << k << '\n';
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to update index file: {}", e.what());
        }
    }
};

#if defined(_WIN32)
/**
 * @brief Windows implementation using Credential Manager
 */
class WindowsSecureStorage : public SecureStorage {
private:
    std::string appName_;

public:
    explicit WindowsSecureStorage(std::string_view appName)
        : appName_(appName) {
        spdlog::info("Using Windows Credential Manager for secure storage");
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for Windows Credential Manager");
            return false;
        }

        std::string targetName = appName_ + "/" + std::string(key);

        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;

        int wideLen = MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(),
                                          targetName.length(), nullptr, 0);
        if (wideLen <= 0) {
            spdlog::error(
                "Failed to convert target to wide string for CredWriteW. "
                "Error: {}",
                GetLastError());
            return false;
        }

        std::wstring wideTarget(wideLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(), targetName.length(),
                            &wideTarget[0], wideLen);

        cred.TargetName = &wideTarget[0];
        cred.CredentialBlobSize = static_cast<DWORD>(data.length());
        cred.CredentialBlob =
            reinterpret_cast<LPBYTE>(const_cast<char*>(data.data()));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

        static const std::wstring userName = L"AtomSecureStorageUser";
        cred.UserName = const_cast<LPWSTR>(userName.c_str());

        if (CredWriteW(&cred, 0)) {
            spdlog::debug(
                "Data stored successfully in Windows Credential Manager for "
                "key: {}",
                key);
            return true;
        } else {
            DWORD error = GetLastError();
            spdlog::error(
                "Failed to store data in Windows Credential Manager for key: "
                "{}. Error: {}",
                key, error);
            return false;
        }
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error(
                "Empty key provided for Windows Credential Manager retrieval");
            return "";
        }

        std::string targetName = appName_ + "/" + std::string(key);
        PCREDENTIALW pCred = nullptr;
        std::string result = "";

        int wideLen = MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(),
                                          targetName.length(), nullptr, 0);
        if (wideLen <= 0) {
            spdlog::error(
                "Failed to convert target to wide string for CredReadW. Error: "
                "{}",
                GetLastError());
            return "";
        }

        std::wstring wideTarget(wideLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(), targetName.length(),
                            &wideTarget[0], wideLen);

        if (CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
            if (pCred->CredentialBlobSize > 0 &&
                pCred->CredentialBlob != nullptr) {
                result.assign(reinterpret_cast<char*>(pCred->CredentialBlob),
                              pCred->CredentialBlobSize);
            }
            CredFree(pCred);
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                spdlog::error(
                    "Failed to retrieve data from Windows Credential Manager "
                    "for key: {}. Error: {}",
                    key, error);
            }
        }
        return result;
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error(
                "Empty key provided for Windows Credential Manager removal");
            return false;
        }

        std::string targetName = appName_ + "/" + std::string(key);

        int wideLen = MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(),
                                          targetName.length(), nullptr, 0);
        if (wideLen <= 0) {
            spdlog::error(
                "Failed to convert target to wide string for CredDeleteW. "
                "Error: {}",
                GetLastError());
            return false;
        }

        std::wstring wideTarget(wideLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(), targetName.length(),
                            &wideTarget[0], wideLen);

        if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
            spdlog::debug(
                "Data deleted successfully from Windows Credential Manager for "
                "key: {}",
                key);
            return true;
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                spdlog::error(
                    "Failed to delete data from Windows Credential Manager for "
                    "key: {}. Error: {}",
                    key, error);
            }
            return error == ERROR_NOT_FOUND;
        }
    }

    std::vector<std::string> getAllKeys() const override {
        std::vector<std::string> results;
        DWORD count = 0;
        PCREDENTIALW* pCredentials = nullptr;

        std::wstring filter =
            std::wstring(appName_.begin(), appName_.end()) + L"/*";

        if (CredEnumerateW(filter.c_str(), 0, &count, &pCredentials)) {
            results.reserve(count);
            for (DWORD i = 0; i < count; ++i) {
                if (pCredentials[i]->TargetName) {
                    int utf8Len = WideCharToMultiByte(
                        CP_UTF8, 0, pCredentials[i]->TargetName, -1, nullptr, 0,
                        nullptr, nullptr);
                    if (utf8Len > 1) {
                        std::string target(utf8Len - 1, 0);
                        WideCharToMultiByte(
                            CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                            &target[0], utf8Len, nullptr, nullptr);

                        std::string prefix = appName_ + "/";
                        if (target.compare(0, prefix.length(), prefix) == 0) {
                            results.push_back(target.substr(prefix.length()));
                        }
                    }
                }
            }
            CredFree(pCredentials);
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                spdlog::error(
                    "Failed to enumerate Windows credentials with filter '{}'. "
                    "Error: {}",
                    std::string(filter.begin(), filter.end()), error);
            }
        }

        return results;
    }
};

#elif defined(__APPLE__)
/**
 * @brief macOS implementation using Keychain
 */
class MacSecureStorage : public SecureStorage {
private:
    std::string serviceName_;

public:
    explicit MacSecureStorage(std::string_view appName)
        : serviceName_(appName) {
        spdlog::info("Using macOS Keychain for secure storage");
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for Mac Keychain");
            return false;
        }

        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName_.data()),
            serviceName_.length(), kCFStringEncodingUTF8, false);
        CFStringRef cfAccount = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(key.data()),
            key.length(), kCFStringEncodingUTF8, false);
        CFDataRef cfData = CFDataCreate(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data.data()),
            data.length());

        if (!cfService || !cfAccount || !cfData) {
            if (cfService)
                CFRelease(cfService);
            if (cfAccount)
                CFRelease(cfAccount);
            if (cfData)
                CFRelease(cfData);
            spdlog::error("Failed to create CF types for Keychain storage");
            return false;
        }

        CFMutableDictionaryRef query = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(query, kSecAttrService, cfService);
        CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

        OSStatus status = SecItemCopyMatching(query, nullptr);

        if (status == errSecSuccess) {
            CFMutableDictionaryRef update = CFDictionaryCreateMutable(
                kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks);
            CFDictionarySetValue(update, kSecValueData, cfData);
            status = SecItemUpdate(query, update);
            CFRelease(update);
            if (status != errSecSuccess) {
                spdlog::error(
                    "Failed to update item in macOS Keychain (Service: {}, "
                    "Account: {}): {}",
                    serviceName_, key, getMacOSStatusString(status));
            }
        } else if (status == errSecItemNotFound) {
            CFDictionarySetValue(query, kSecValueData, cfData);
            CFDictionarySetValue(query, kSecAttrAccessible,
                                 kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
            status = SecItemAdd(query, nullptr);
            if (status != errSecSuccess) {
                spdlog::error(
                    "Failed to add item to macOS Keychain (Service: {}, "
                    "Account: {}): {}",
                    serviceName_, key, getMacOSStatusString(status));
            }
        } else {
            spdlog::error(
                "Failed to query macOS Keychain (Service: {}, Account: {}): {}",
                serviceName_, key, getMacOSStatusString(status));
        }

        CFRelease(query);
        CFRelease(cfService);
        CFRelease(cfAccount);
        CFRelease(cfData);

        return status == errSecSuccess;
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for Mac Keychain retrieval");
            return "";
        }

        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName_.data()),
            serviceName_.length(), kCFStringEncodingUTF8, false);
        CFStringRef cfAccount = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(key.data()),
            key.length(), kCFStringEncodingUTF8, false);

        if (!cfService || !cfAccount) {
            if (cfService)
                CFRelease(cfService);
            if (cfAccount)
                CFRelease(cfAccount);
            spdlog::error("Failed to create CF types for Keychain retrieval");
            return "";
        }

        CFMutableDictionaryRef query = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(query, kSecAttrService, cfService);
        CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
        CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

        CFDataRef cfData = nullptr;
        OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfData);

        std::string result = "";
        if (status == errSecSuccess && cfData) {
            result.assign(
                reinterpret_cast<const char*>(CFDataGetBytePtr(cfData)),
                CFDataGetLength(cfData));
            CFRelease(cfData);
        } else if (status != errSecItemNotFound) {
            spdlog::error(
                "Failed to retrieve item from macOS Keychain (Service: {}, "
                "Account: {}): {}",
                serviceName_, key, getMacOSStatusString(status));
        }

        CFRelease(query);
        CFRelease(cfService);
        CFRelease(cfAccount);

        return result;
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for Mac Keychain removal");
            return false;
        }

        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName_.data()),
            serviceName_.length(), kCFStringEncodingUTF8, false);
        CFStringRef cfAccount = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(key.data()),
            key.length(), kCFStringEncodingUTF8, false);

        if (!cfService || !cfAccount) {
            if (cfService)
                CFRelease(cfService);
            if (cfAccount)
                CFRelease(cfAccount);
            spdlog::error("Failed to create CF types for Keychain deletion");
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
            spdlog::error(
                "Failed to delete item from macOS Keychain (Service: {}, "
                "Account: {}): {}",
                serviceName_, key, getMacOSStatusString(status));
        }

        CFRelease(query);
        CFRelease(cfService);
        CFRelease(cfAccount);

        return status == errSecSuccess || status == errSecItemNotFound;
    }

    std::vector<std::string> getAllKeys() const override {
        std::vector<std::string> results;
        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName_.data()),
            serviceName_.length(), kCFStringEncodingUTF8, false);

        if (!cfService) {
            spdlog::error(
                "Failed to create CFString for Keychain service name");
            return results;
        }

        CFMutableDictionaryRef query = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(query, kSecAttrService, cfService);
        CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
        CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);

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
                        accountStr.resize(strlen(accountStr.c_str()));
                        results.push_back(std::move(accountStr));
                    }
                }
            }
            CFRelease(cfResults);
        } else if (status != errSecItemNotFound) {
            spdlog::error(
                "Failed to list macOS Keychain items (Service: {}): {}",
                serviceName_, getMacOSStatusString(status));
        }

        CFRelease(query);
        CFRelease(cfService);

        return results;
    }
};

#elif defined(__linux__) && defined(USE_LIBSECRET)
/**
 * @brief Linux implementation with libsecret
 */
class LinuxSecureStorage : public SecureStorage {
private:
    std::string schemaName_;

public:
    explicit LinuxSecureStorage(std::string_view appName)
        : schemaName_(appName) {
        spdlog::info("Using Linux Secret Service for secure storage");
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for Linux keyring");
            return false;
        }

        const SecretSchema schema = {
            schemaName_.c_str(),
            SECRET_SCHEMA_NONE,
            {{"app_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
             {nullptr, SecretSchemaAttributeType(0)}}};

        GError* error = nullptr;
        gboolean success = secret_password_store_sync(
            &schema, SECRET_COLLECTION_DEFAULT, key.data(), data.data(),
            nullptr, &error, "app_key", key.data(), nullptr);

        if (!success) {
            if (error) {
                spdlog::error(
                    "Failed to store data in Linux keyring (Schema: {}, Key: "
                    "{}): {}",
                    schemaName_, key, error->message);
                g_error_free(error);
            } else {
                spdlog::error(
                    "Failed to store data in Linux keyring (Schema: {}, Key: "
                    "{}) (Unknown error)",
                    schemaName_, key);
            }
            return false;
        }

        spdlog::debug("Data stored successfully in Linux keyring for key: {}",
                      key);
        return true;
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for Linux keyring retrieval");
            return "";
        }

        const SecretSchema schema = {
            schemaName_.c_str(),
            SECRET_SCHEMA_NONE,
            {{"app_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
             {nullptr, SecretSchemaAttributeType(0)}}};

        GError* error = nullptr;
        gchar* secret = secret_password_lookup_sync(
            &schema, nullptr, &error, "app_key", key.data(), nullptr);

        std::string result = "";
        if (secret) {
            result.assign(secret);
            secret_password_free(secret);
        } else if (error) {
            spdlog::error(
                "Failed to retrieve data from Linux keyring (Schema: {}, Key: "
                "{}): {}",
                schemaName_, key, error->message);
            g_error_free(error);
        }
        return result;
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            spdlog::error("Empty key provided for Linux keyring removal");
            return false;
        }

        const SecretSchema schema = {
            schemaName_.c_str(),
            SECRET_SCHEMA_NONE,
            {{"app_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
             {nullptr, SecretSchemaAttributeType(0)}}};

        GError* error = nullptr;
        gboolean success = secret_password_clear_sync(
            &schema, nullptr, &error, "app_key", key.data(), nullptr);

        if (!success && error) {
            spdlog::error(
                "Failed to delete data from Linux keyring (Schema: {}, Key: "
                "{}): {}",
                schemaName_, key, error->message);
            g_error_free(error);
        }
        return success || !error;
    }

    std::vector<std::string> getAllKeys() const override {
        std::vector<std::string> results;
        std::string indexKey = std::string(schemaName_) + "_INDEX";
        std::string indexData = retrieve(indexKey);

        if (!indexData.empty()) {
            size_t pos = 0;
            while (pos < indexData.size()) {
                size_t endPos = indexData.find('\n', pos);
                if (endPos == std::string::npos) {
                    results.push_back(indexData.substr(pos));
                    break;
                }
                results.push_back(indexData.substr(pos, endPos - pos));
                pos = endPos + 1;
            }
        }

        return results;
    }
};
#endif

std::unique_ptr<SecureStorage> SecureStorage::create(std::string_view appName) {
#if defined(_WIN32)
    return std::make_unique<WindowsSecureStorage>(appName);
#elif defined(__APPLE__)
    return std::make_unique<MacSecureStorage>(appName);
#elif defined(__linux__) && defined(USE_LIBSECRET)
    return std::make_unique<LinuxSecureStorage>(appName);
#else
    return std::make_unique<FileSecureStorage>(appName);
#endif
}

}  // namespace atom::secret