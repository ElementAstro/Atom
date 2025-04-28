#include "storage.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>  // For std::istreambuf_iterator
#include <string>
#include <system_error>  // For filesystem errors

#include "atom/log/loguru.hpp"

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
#define USE_FILE_FALLBACK 1  // Explicitly define for clarity
#endif
#else
#define USE_FILE_FALLBACK 1  // Explicitly define for clarity
#endif

namespace {

// Helper to get secure storage directory for file fallback
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
        // Fallback if SHGetKnownFolderPath fails
        char* appDataPath = nullptr;
        size_t pathLen;
        _dupenv_s(&appDataPath, &pathLen, "LOCALAPPDATA");
        if (appDataPath) {
            storageDir = appDataPath;
            storageDir /= appName;
            free(appDataPath);
        } else {
            storageDir =
                std::string(".") +
                std::string(appName);  // Current directory as last resort
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
        storageDir /= "Library/Application Support";
        storageDir /= appName;
#else  // Linux
        storageDir /= ".local/share";
        storageDir /= appName;
#endif
    } else {
        storageDir = std::string(".") +
                     std::string(appName);  // Current directory as last resort
        LOG_F(WARNING,
              "Could not determine HOME directory, using current directory.");
    }
#else
    storageDir =
        std::string(".") +
        std::string(appName);  // Current directory for unknown platforms
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

#if defined(__APPLE__)
// Helper function for macOS status codes
std::string GetMacOSStatusString(OSStatus status) {
    // Consider using SecCopyErrorMessageString for more descriptive errors if
    // available
    return "macOS Error: " + std::to_string(status);
}
#endif

}  // anonymous namespace

namespace atom::secret {

// Base FileSecureStorage class implementation (used as fallback)
class FileSecureStorage : public SecureStorage {
private:
    std::string appName;
    std::filesystem::path storageDir;

public:
    explicit FileSecureStorage(std::string_view appName)
        : appName(appName), storageDir(getSecureStorageDirectory(appName)) {
        LOG_F(INFO, "Using file-based secure storage at: {}",
              storageDir.string().c_str());
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for file storage");
            return false;
        }

        std::string sanitizedKey = sanitizeIdentifier(key);
        std::filesystem::path filePath = storageDir / (sanitizedKey + ".dat");

        try {
            std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
            if (!outFile) {
                throw std::runtime_error("Failed to open file for writing: " +
                                         filePath.string());
            }
            outFile.write(data.data(), data.size());
            outFile.close();
            if (!outFile) {  // Check for write errors after closing
                throw std::runtime_error("Failed to write data to file: " +
                                         filePath.string());
            }

            // Update index to track stored keys
            std::filesystem::path indexPath = storageDir / "index.json";
            std::vector<std::string> existingKeys;

            // Read existing index if it exists
            std::ifstream indexFile(indexPath);
            if (indexFile) {
                std::string indexStr(
                    (std::istreambuf_iterator<char>(indexFile)),
                    std::istreambuf_iterator<char>());
                indexFile.close();

                // Parse index file - simple format, one key per line
                if (!indexStr.empty()) {
                    size_t pos = 0;
                    while (pos < indexStr.size()) {
                        size_t endPos = indexStr.find('\n', pos);
                        if (endPos == std::string::npos) {
                            existingKeys.push_back(indexStr.substr(pos));
                            break;
                        }
                        existingKeys.push_back(
                            indexStr.substr(pos, endPos - pos));
                        pos = endPos + 1;
                    }
                }
            }

            // Add new key if not already present
            if (std::find(existingKeys.begin(), existingKeys.end(),
                          sanitizedKey) == existingKeys.end()) {
                existingKeys.push_back(sanitizedKey);
            }

            // Write updated index
            std::ofstream newIndexFile(indexPath,
                                       std::ios::binary | std::ios::trunc);
            if (newIndexFile) {
                for (const auto& k : existingKeys) {
                    newIndexFile << k << '\n';
                }
            }

            LOG_F(INFO, "Data stored successfully to file: {}",
                  sanitizedKey.c_str());
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to store data in file (key: {}): {}",
                  key.data(), e.what());
            return false;
        }
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for file retrieval");
            return "";
        }

        std::string sanitizedKey = sanitizeIdentifier(key);
        std::filesystem::path filePath = storageDir / (sanitizedKey + ".dat");

        try {
            std::ifstream inFile(filePath, std::ios::binary);
            if (!inFile) {
                LOG_F(WARNING, "File not found for key: {}", key.data());
                return "";  // Not found
            }
            std::string data((std::istreambuf_iterator<char>(inFile)),
                             std::istreambuf_iterator<char>());
            return data;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to retrieve data from file (key: {}): {}",
                  key.data(), e.what());
            return "";
        }
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for file removal");
            return false;
        }

        std::string sanitizedKey = sanitizeIdentifier(key);
        std::filesystem::path filePath = storageDir / (sanitizedKey + ".dat");

        try {
            std::error_code ec;
            if (std::filesystem::remove(filePath, ec) || ec.value() == ENOENT) {
                // Update index to remove the key
                std::filesystem::path indexPath = storageDir / "index.json";
                std::vector<std::string> existingKeys;

                // Read existing index
                std::ifstream indexFile(indexPath);
                if (indexFile) {
                    std::string indexStr(
                        (std::istreambuf_iterator<char>(indexFile)),
                        std::istreambuf_iterator<char>());
                    indexFile.close();

                    // Parse index file - simple format, one key per line
                    if (!indexStr.empty()) {
                        size_t pos = 0;
                        while (pos < indexStr.size()) {
                            size_t endPos = indexStr.find('\n', pos);
                            std::string curKey;
                            if (endPos == std::string::npos) {
                                curKey = indexStr.substr(pos);
                                if (curKey != sanitizedKey) {
                                    existingKeys.push_back(curKey);
                                }
                                break;
                            }
                            curKey = indexStr.substr(pos, endPos - pos);
                            if (curKey != sanitizedKey) {
                                existingKeys.push_back(curKey);
                            }
                            pos = endPos + 1;
                        }
                    }
                }

                // Write updated index
                std::ofstream newIndexFile(indexPath,
                                           std::ios::binary | std::ios::trunc);
                if (newIndexFile) {
                    for (const auto& k : existingKeys) {
                        newIndexFile << k << '\n';
                    }
                }

                LOG_F(INFO, "Successfully removed file for key: {}",
                      key.data());
                return true;
            } else {
                throw std::runtime_error(
                    "Failed to delete file: " + filePath.string() + " (" +
                    ec.message() + ")");
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to remove data file (key: {}): {}", key.data(),
                  e.what());
            return false;
        }
    }

    std::vector<std::string> getAllKeys() const override {
        std::vector<std::string> keys;
        std::filesystem::path indexPath = storageDir / "index.json";

        try {
            std::ifstream indexFile(indexPath);
            if (indexFile) {
                std::string indexStr(
                    (std::istreambuf_iterator<char>(indexFile)),
                    std::istreambuf_iterator<char>());

                // Parse index file - simple format, one key per line
                if (!indexStr.empty()) {
                    size_t pos = 0;
                    while (pos < indexStr.size()) {
                        size_t endPos = indexStr.find('\n', pos);
                        if (endPos == std::string::npos) {
                            keys.push_back(indexStr.substr(pos));
                            break;
                        }
                        keys.push_back(indexStr.substr(pos, endPos - pos));
                        pos = endPos + 1;
                    }
                }
            } else {
                // If index file doesn't exist, try to enumerate .dat files in
                // the directory
                for (const auto& entry :
                     std::filesystem::directory_iterator(storageDir)) {
                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".dat") {
                        std::string filename = entry.path().filename().string();
                        // Remove .dat extension
                        filename = filename.substr(0, filename.length() - 4);
                        keys.push_back(filename);
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to read index file: {}", e.what());
        }

        return keys;
    }
};

#if defined(_WIN32)
// Windows implementation
class WindowsSecureStorage : public SecureStorage {
private:
    std::string appName;

public:
    explicit WindowsSecureStorage(std::string_view appName) : appName(appName) {
        LOG_F(INFO, "Using Windows Credential Manager for secure storage");
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for Windows Credential Manager");
            return false;
        }

        // Prefix the key with app name to avoid collisions
        std::string targetName = appName + "/" + std::string(key);

        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;

        // Convert target to wide string
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(),
                                          targetName.length(), nullptr, 0);
        if (wideLen <= 0) {
            LOG_F(ERROR,
                  "Failed to convert target to wide string for CredWriteW. "
                  "Error: %lu",
                  GetLastError());
            return false;
        }
        std::wstring wideTarget(wideLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(), targetName.length(),
                            &wideTarget[0], wideLen);

        cred.TargetName = &wideTarget[0];
        cred.CredentialBlobSize = static_cast<DWORD>(data.length());
        // CredentialBlob needs non-const pointer
        cred.CredentialBlob =
            reinterpret_cast<LPBYTE>(const_cast<char*>(data.data()));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

        // Use a fixed, non-sensitive username
        static const std::wstring userName = L"AtomSecureStorageUser";
        cred.UserName = const_cast<LPWSTR>(userName.c_str());

        if (CredWriteW(&cred, 0)) {
            LOG_F(INFO,
                  "Data stored successfully in Windows Credential Manager for "
                  "key: {}",
                  key.data());
            return true;
        } else {
            DWORD error = GetLastError();
            LOG_F(ERROR,
                  "Failed to store data in Windows Credential Manager for key: "
                  "{}. Error: {}",
                  key.data(), error);
            return false;
        }
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(
                ERROR,
                "Empty key provided for Windows Credential Manager retrieval");
            return "";
        }

        // Prefix the key with app name
        std::string targetName = appName + "/" + std::string(key);

        PCREDENTIALW pCred = nullptr;
        std::string result = "";

        // Convert target to wide string
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(),
                                          targetName.length(), nullptr, 0);
        if (wideLen <= 0) {
            LOG_F(ERROR,
                  "Failed to convert target to wide string for CredReadW. "
                  "Error: %lu",
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
                LOG_F(ERROR,
                      "Failed to retrieve data from Windows Credential Manager "
                      "for "
                      "key: {}. Error: {}",
                      key.data(), error);
            }
            // Return empty string if not found or error
        }
        return result;
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(ERROR,
                  "Empty key provided for Windows Credential Manager removal");
            return false;
        }

        // Prefix the key with app name
        std::string targetName = appName + "/" + std::string(key);

        // Convert target to wide string
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(),
                                          targetName.length(), nullptr, 0);
        if (wideLen <= 0) {
            LOG_F(ERROR,
                  "Failed to convert target to wide string for CredDeleteW. "
                  "Error: "
                  "%lu",
                  GetLastError());
            return false;
        }
        std::wstring wideTarget(wideLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, targetName.c_str(), targetName.length(),
                            &wideTarget[0], wideLen);

        if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
            LOG_F(INFO,
                  "Data deleted successfully from Windows Credential Manager "
                  "for key: {}",
                  key.data());
            return true;
        } else {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                LOG_F(
                    ERROR,
                    "Failed to delete data from Windows Credential Manager for "
                    "key: {}. Error: {}",
                    key.data(), error);
            }
            // Return true if not found (idempotent delete)
            return error == ERROR_NOT_FOUND;
        }
    }

    std::vector<std::string> getAllKeys() const override {
        std::vector<std::string> results;
        DWORD count = 0;
        PCREDENTIALW* pCredentials = nullptr;

        // Create filter for our app's credentials
        std::wstring filter =
            std::wstring(appName.begin(), appName.end()) + L"/*";

        if (CredEnumerateW(filter.c_str(), 0, &count, &pCredentials)) {
            results.reserve(count);
            for (DWORD i = 0; i < count; ++i) {
                if (pCredentials[i]->TargetName) {
                    // Convert wide string target back to UTF-8 std::string
                    int utf8Len = WideCharToMultiByte(
                        CP_UTF8, 0, pCredentials[i]->TargetName, -1, nullptr, 0,
                        nullptr, nullptr);
                    if (utf8Len >
                        1) {  // Greater than 1 to account for null terminator
                        std::string target(utf8Len - 1, 0);
                        WideCharToMultiByte(
                            CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                            &target[0], utf8Len, nullptr, nullptr);

                        // Extract the key part (remove the app name prefix)
                        std::string prefix = appName + "/";
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
                LOG_F(
                    ERROR,
                    "Failed to enumerate Windows credentials with filter '{}'. "
                    "Error: {}",
                    std::string(filter.begin(), filter.end()).c_str(), error);
            }
        }

        return results;
    }
};
#elif defined(__APPLE__)
// macOS implementation
class MacSecureStorage : public SecureStorage {
private:
    std::string serviceName;

public:
    explicit MacSecureStorage(std::string_view appName) : serviceName(appName) {
        LOG_F(INFO, "Using macOS Keychain for secure storage");
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for Mac Keychain");
            return false;
        }

        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName.data()),
            serviceName.length(), kCFStringEncodingUTF8, false);
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
                      serviceName.c_str(), key.data(),
                      GetMacOSStatusString(status).c_str());
            }
        } else if (status ==
                   errSecItemNotFound) {  // Item doesn't exist, add it
            CFDictionarySetValue(query, kSecValueData, cfData);
            // Set accessibility - kSecAttrAccessibleWhenUnlockedThisDeviceOnly
            // is a reasonable default
            CFDictionarySetValue(query, kSecAttrAccessible,
                                 kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
            status = SecItemAdd(query, nullptr);
            if (status != errSecSuccess) {
                LOG_F(ERROR,
                      "Failed to add item to macOS Keychain (Service: {}, "
                      "Account: "
                      "{}): {}",
                      serviceName.c_str(), key.data(),
                      GetMacOSStatusString(status).c_str());
            }
        } else {  // Other error during lookup
            LOG_F(
                ERROR,
                "Failed to query macOS Keychain (Service: {}, Account: {}): {}",
                serviceName.c_str(), key.data(),
                GetMacOSStatusString(status).c_str());
        }

        CFRelease(query);
        CFRelease(cfService);
        CFRelease(cfAccount);
        CFRelease(cfData);

        return status == errSecSuccess;
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for Mac Keychain retrieval");
            return "";
        }

        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName.data()),
            serviceName.length(), kCFStringEncodingUTF8, false);
        CFStringRef cfAccount = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(key.data()),
            key.length(), kCFStringEncodingUTF8, false);

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
            result.assign(
                reinterpret_cast<const char*>(CFDataGetBytePtr(cfData)),
                CFDataGetLength(cfData));
            CFRelease(cfData);
        } else if (status != errSecItemNotFound) {
            LOG_F(ERROR,
                  "Failed to retrieve item from macOS Keychain (Service: {}, "
                  "Account: {}): {}",
                  serviceName.c_str(), key.data(),
                  GetMacOSStatusString(status).c_str());
        }

        CFRelease(query);
        CFRelease(cfService);
        CFRelease(cfAccount);

        return result;
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for Mac Keychain removal");
            return false;
        }

        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName.data()),
            serviceName.length(), kCFStringEncodingUTF8, false);
        CFStringRef cfAccount = CFStringCreateWithBytes(
            kCFAllocatorDefault, reinterpret_cast<const UInt8*>(key.data()),
            key.length(), kCFStringEncodingUTF8, false);

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
                  serviceName.c_str(), key.data(),
                  GetMacOSStatusString(status).c_str());
        }

        CFRelease(query);
        CFRelease(cfService);
        CFRelease(cfAccount);

        // Return true if deleted or not found (idempotent)
        return status == errSecSuccess || status == errSecItemNotFound;
    }

    std::vector<std::string> getAllKeys() const override {
        std::vector<std::string> results;
        CFStringRef cfService = CFStringCreateWithBytes(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(serviceName.data()),
            serviceName.length(), kCFStringEncodingUTF8, false);
        if (!cfService) {
            LOG_F(ERROR,
                  "Failed to create CFString for Keychain service name.");
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
            LOG_F(ERROR,
                  "Failed to list macOS Keychain items (Service: {}): {}",
                  serviceName.c_str(), GetMacOSStatusString(status).c_str());
        }

        CFRelease(query);
        CFRelease(cfService);

        return results;
    }
};
#elif defined(__linux__) && defined(USE_LIBSECRET)
// Linux implementation with libsecret
class LinuxSecureStorage : public SecureStorage {
private:
    std::string schemaName;

public:
    explicit LinuxSecureStorage(std::string_view appName)
        : schemaName(appName) {
        LOG_F(INFO, "Using Linux Secret Service for secure storage");
    }

    bool store(std::string_view key, std::string_view data) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for Linux keyring");
            return false;
        }

        const SecretSchema schema = {
            schemaName.c_str(),  // Use c_str() for C API
            SECRET_SCHEMA_NONE,
            {
                // Define attributes used for lookup
                {"app_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
                {nullptr, SecretSchemaAttributeType(0)}  // Null terminate
            }};

        GError* error = nullptr;
        // Store password with label and attributes
        gboolean success = secret_password_store_sync(
            &schema,
            SECRET_COLLECTION_DEFAULT,  // Or specific collection
            key.data(),                 // Use key as the display label
            data.data(),
            nullptr,                        // Cancellable
            &error, "app_key", key.data(),  // Attribute for lookup
            nullptr);                       // Null terminate attributes

        if (!success) {
            if (error) {
                LOG_F(ERROR,
                      "Failed to store data in Linux keyring (Schema: {}, Key: "
                      "{}): {}",
                      schemaName.c_str(), key.data(), error->message);
                g_error_free(error);
            } else {
                LOG_F(ERROR,
                      "Failed to store data in Linux keyring (Schema: {}, Key: "
                      "{}) "
                      "(Unknown error)",
                      schemaName.c_str(), key.data());
            }
            return false;
        }

        LOG_F(INFO, "Data stored successfully in Linux keyring for key: {}",
              key.data());
        return true;
    }

    std::string retrieve(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for Linux keyring retrieval");
            return "";
        }

        const SecretSchema schema = {
            schemaName.c_str(),
            SECRET_SCHEMA_NONE,
            {{"app_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
             {nullptr, SecretSchemaAttributeType(0)}}};

        GError* error = nullptr;
        // Lookup password based on attributes
        gchar* secret =
            secret_password_lookup_sync(&schema,
                                        nullptr,  // Cancellable
                                        &error, "app_key",
                                        key.data(),  // Attribute value to match
                                        nullptr);  // Null terminate attributes

        std::string result = "";
        if (secret) {
            result.assign(secret);
            secret_password_free(secret);
        } else if (error) {
            LOG_F(
                ERROR,
                "Failed to retrieve data from Linux keyring (Schema: {}, Key: "
                "{}): {}",
                schemaName.c_str(), key.data(), error->message);
            g_error_free(error);
        }
        // Return empty string if not found or error
        return result;
    }

    bool remove(std::string_view key) const override {
        if (key.empty()) {
            LOG_F(ERROR, "Empty key provided for Linux keyring removal");
            return false;
        }

        const SecretSchema schema = {
            schemaName.c_str(),
            SECRET_SCHEMA_NONE,
            {{"app_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
             {nullptr, SecretSchemaAttributeType(0)}}};

        GError* error = nullptr;
        gboolean success =
            secret_password_clear_sync(&schema,
                                       nullptr,  // Cancellable
                                       &error, "app_key",
                                       key.data(),  // Attribute value to match
                                       nullptr);    // Null terminate attributes

        if (!success && error) {
            LOG_F(ERROR,
                  "Failed to delete data from Linux keyring (Schema: {}, Key: "
                  "{}): "
                  "{}",
                  schemaName.c_str(), key.data(), error->message);
            g_error_free(error);
        }
        // Return true if deleted or not found (idempotent)
        return success || !error;
    }

    std::vector<std::string> getAllKeys() const override {
        // Note: libsecret doesn't provide a simple way to enumerate all items
        // with a given schema This implementation uses an index file stored in
        // the keyring itself
        std::vector<std::string> results;

        // Try to retrieve the index
        std::string indexKey = std::string(schemaName) + "_INDEX";
        std::string indexData = retrieve(indexKey);

        if (!indexData.empty()) {
            // Parse the index data (simple format: one key per line)
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

    // Helper method to update the index when adding/removing keys
    bool updateIndex(const std::vector<std::string>& keys) const {
        std::string indexKey = std::string(schemaName) + "_INDEX";
        std::string indexData;

        for (const auto& key : keys) {
            indexData += key + "\n";
        }

        return store(indexKey, indexData);
    }
};
#endif

// Factory method to create appropriate SecureStorage instance
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