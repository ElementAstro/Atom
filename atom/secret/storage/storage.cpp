#include "storage.hpp"

#include <unordered_map>

#include "file_storage.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Note: Do NOT define WIN32_LEAN_AND_MEAN here as wincred.h needs full
// windows.h
#include <wincred.h>
#include <windows.h>
#pragma comment(lib, "Advapi32.lib")
#elif defined(__APPLE__)
#include <Security/Security.h>
#elif defined(__linux__)
// libsecret is optional
#if __has_include(<libsecret/secret.h>)
#define HAS_LIBSECRET 1
#include <libsecret/secret.h>
#endif
#endif

#include "spdlog/spdlog.h"

namespace atom::secret {

#if defined(_WIN32)

/**
 * @brief Windows Credential Manager storage implementation.
 */
class WindowsSecureStorage : public SecureStorage {
public:
    explicit WindowsSecureStorage(std::string_view appName)
        : appName_(appName) {}

    Result<void> store(std::string_view key, std::string_view data) override {
        return store(key, std::vector<uint8_t>(data.begin(), data.end()));
    }

    Result<void> store(std::string_view key,
                       const std::vector<uint8_t>& data) override {
        std::string targetName = appName_ + "/" + std::string(key);

        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;

        std::wstring wTargetName(targetName.begin(), targetName.end());
        cred.TargetName = const_cast<LPWSTR>(wTargetName.c_str());
        cred.CredentialBlobSize = static_cast<DWORD>(data.size());
        cred.CredentialBlob = const_cast<LPBYTE>(data.data());
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

        std::wstring wAppName(appName_.begin(), appName_.end());
        cred.Comment = const_cast<LPWSTR>(wAppName.c_str());

        if (!CredWriteW(&cred, 0)) {
            return Result<void>::error(ErrorCode::StorageWriteFailed,
                                       "Failed to store credential: " +
                                           std::to_string(GetLastError()));
        }

        return Result<void>::success();
    }

    Result<std::string> retrieve(std::string_view key) override {
        auto result = retrieveBytes(key);
        if (result.isError()) {
            return Result<std::string>::error(result.errorCode(),
                                              result.errorMessage());
        }
        const auto& bytes = result.value();
        return Result<std::string>::success(
            std::string(bytes.begin(), bytes.end()));
    }

    Result<std::vector<uint8_t>> retrieveBytes(std::string_view key) override {
        std::string targetName = appName_ + "/" + std::string(key);
        std::wstring wTargetName(targetName.begin(), targetName.end());

        PCREDENTIALW pCred = nullptr;
        if (!CredReadW(wTargetName.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::StorageKeyNotFound, "Key not found");
        }

        std::vector<uint8_t> data(
            pCred->CredentialBlob,
            pCred->CredentialBlob + pCred->CredentialBlobSize);

        CredFree(pCred);
        return Result<std::vector<uint8_t>>::success(std::move(data));
    }

    Result<void> remove(std::string_view key) override {
        std::string targetName = appName_ + "/" + std::string(key);
        std::wstring wTargetName(targetName.begin(), targetName.end());

        if (!CredDeleteW(wTargetName.c_str(), CRED_TYPE_GENERIC, 0)) {
            DWORD error = GetLastError();
            if (error == ERROR_NOT_FOUND) {
                return Result<void>::success();  // Already deleted
            }
            return Result<void>::error(
                ErrorCode::StorageDeleteFailed,
                "Failed to delete credential: " + std::to_string(error));
        }

        return Result<void>::success();
    }

    bool exists(std::string_view key) override {
        std::string targetName = appName_ + "/" + std::string(key);
        std::wstring wTargetName(targetName.begin(), targetName.end());

        PCREDENTIALW pCred = nullptr;
        if (CredReadW(wTargetName.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
            CredFree(pCred);
            return true;
        }
        return false;
    }

    Result<std::vector<std::string>> getAllKeys() override {
        std::vector<std::string> keys;
        std::string filter = appName_ + "/*";
        std::wstring wFilter(filter.begin(), filter.end());

        DWORD count = 0;
        PCREDENTIALW* pCredentials = nullptr;

        if (CredEnumerateW(wFilter.c_str(), 0, &count, &pCredentials)) {
            for (DWORD i = 0; i < count; ++i) {
                std::wstring wTarget = pCredentials[i]->TargetName;
                std::string target(wTarget.begin(), wTarget.end());

                size_t prefixLen = appName_.length() + 1;
                if (target.length() > prefixLen) {
                    keys.push_back(target.substr(prefixLen));
                }
            }
            CredFree(pCredentials);
        }

        return Result<std::vector<std::string>>::success(std::move(keys));
    }

    Result<void> clear() override {
        auto keysResult = getAllKeys();
        if (keysResult.isError()) {
            return Result<void>::error(keysResult.errorCode(),
                                       keysResult.errorMessage());
        }

        for (const auto& key : keysResult.value()) {
            remove(key);
        }

        return Result<void>::success();
    }

    StorageBackend getBackend() const noexcept override {
        return StorageBackend::System;
    }

    std::string getLocation() const override {
        return "Windows Credential Manager";
    }

private:
    std::string appName_;
};

#elif defined(__APPLE__)

/**
 * @brief macOS Keychain storage implementation.
 */
class MacSecureStorage : public SecureStorage {
public:
    explicit MacSecureStorage(std::string_view appName) : appName_(appName) {}

    Result<void> store(std::string_view key, std::string_view data) override {
        return store(key, std::vector<uint8_t>(data.begin(), data.end()));
    }

    Result<void> store(std::string_view key,
                       const std::vector<uint8_t>& data) override {
        std::string service = appName_;
        std::string account = std::string(key);

        // Delete existing item first
        SecKeychainItemRef itemRef = nullptr;
        OSStatus status = SecKeychainFindGenericPassword(
            nullptr, static_cast<UInt32>(service.length()), service.c_str(),
            static_cast<UInt32>(account.length()), account.c_str(), nullptr,
            nullptr, &itemRef);

        if (status == errSecSuccess && itemRef) {
            SecKeychainItemDelete(itemRef);
            CFRelease(itemRef);
        }

        // Add new item
        status = SecKeychainAddGenericPassword(
            nullptr, static_cast<UInt32>(service.length()), service.c_str(),
            static_cast<UInt32>(account.length()), account.c_str(),
            static_cast<UInt32>(data.size()), data.data(), nullptr);

        if (status != errSecSuccess) {
            return Result<void>::error(
                ErrorCode::StorageWriteFailed,
                "Failed to store in Keychain: " + std::to_string(status));
        }

        return Result<void>::success();
    }

    Result<std::string> retrieve(std::string_view key) override {
        auto result = retrieveBytes(key);
        if (result.isError()) {
            return Result<std::string>::error(result.errorCode(),
                                              result.errorMessage());
        }
        const auto& bytes = result.value();
        return Result<std::string>::success(
            std::string(bytes.begin(), bytes.end()));
    }

    Result<std::vector<uint8_t>> retrieveBytes(std::string_view key) override {
        std::string service = appName_;
        std::string account = std::string(key);

        UInt32 passwordLength = 0;
        void* passwordData = nullptr;

        OSStatus status = SecKeychainFindGenericPassword(
            nullptr, static_cast<UInt32>(service.length()), service.c_str(),
            static_cast<UInt32>(account.length()), account.c_str(),
            &passwordLength, &passwordData, nullptr);

        if (status != errSecSuccess) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::StorageKeyNotFound, "Key not found in Keychain");
        }

        std::vector<uint8_t> data(
            static_cast<uint8_t*>(passwordData),
            static_cast<uint8_t*>(passwordData) + passwordLength);

        SecKeychainItemFreeContent(nullptr, passwordData);
        return Result<std::vector<uint8_t>>::success(std::move(data));
    }

    Result<void> remove(std::string_view key) override {
        std::string service = appName_;
        std::string account = std::string(key);

        SecKeychainItemRef itemRef = nullptr;
        OSStatus status = SecKeychainFindGenericPassword(
            nullptr, static_cast<UInt32>(service.length()), service.c_str(),
            static_cast<UInt32>(account.length()), account.c_str(), nullptr,
            nullptr, &itemRef);

        if (status == errSecSuccess && itemRef) {
            SecKeychainItemDelete(itemRef);
            CFRelease(itemRef);
        }

        return Result<void>::success();
    }

    bool exists(std::string_view key) override {
        std::string service = appName_;
        std::string account = std::string(key);

        SecKeychainItemRef itemRef = nullptr;
        OSStatus status = SecKeychainFindGenericPassword(
            nullptr, static_cast<UInt32>(service.length()), service.c_str(),
            static_cast<UInt32>(account.length()), account.c_str(), nullptr,
            nullptr, &itemRef);

        if (status == errSecSuccess && itemRef) {
            CFRelease(itemRef);
            return true;
        }
        return false;
    }

    Result<std::vector<std::string>> getAllKeys() override {
        // Keychain doesn't easily support enumeration
        // Return empty list - users should track keys separately
        return Result<std::vector<std::string>>::success(
            std::vector<std::string>{});
    }

    Result<void> clear() override {
        // Cannot easily enumerate, so this is a no-op
        return Result<void>::success();
    }

    StorageBackend getBackend() const noexcept override {
        return StorageBackend::System;
    }

    std::string getLocation() const override { return "macOS Keychain"; }

private:
    std::string appName_;
};

#elif defined(__linux__) && defined(HAS_LIBSECRET)

/**
 * @brief Linux Secret Service storage implementation.
 */
class LinuxSecureStorage : public SecureStorage {
public:
    explicit LinuxSecureStorage(std::string_view appName) : appName_(appName) {
        schema_ = secret_schema_new(appName_.c_str(), SECRET_SCHEMA_NONE, "key",
                                    SECRET_SCHEMA_ATTRIBUTE_STRING, nullptr);
    }

    ~LinuxSecureStorage() override {
        if (schema_) {
            secret_schema_unref(schema_);
        }
    }

    Result<void> store(std::string_view key, std::string_view data) override {
        return store(key, std::vector<uint8_t>(data.begin(), data.end()));
    }

    Result<void> store(std::string_view key,
                       const std::vector<uint8_t>& data) override {
        GError* error = nullptr;
        std::string keyStr(key);
        std::string dataStr(data.begin(), data.end());

        gboolean result = secret_password_store_sync(
            schema_, SECRET_COLLECTION_DEFAULT, keyStr.c_str(), dataStr.c_str(),
            nullptr, &error, "key", keyStr.c_str(), nullptr);

        if (!result || error) {
            std::string msg = error ? error->message : "Unknown error";
            if (error)
                g_error_free(error);
            return Result<void>::error(ErrorCode::StorageWriteFailed, msg);
        }

        return Result<void>::success();
    }

    Result<std::string> retrieve(std::string_view key) override {
        GError* error = nullptr;
        std::string keyStr(key);

        gchar* password = secret_password_lookup_sync(
            schema_, nullptr, &error, "key", keyStr.c_str(), nullptr);

        if (!password) {
            if (error)
                g_error_free(error);
            return Result<std::string>::error(ErrorCode::StorageKeyNotFound,
                                              "Key not found");
        }

        std::string result(password);
        secret_password_free(password);
        return Result<std::string>::success(std::move(result));
    }

    Result<std::vector<uint8_t>> retrieveBytes(std::string_view key) override {
        auto result = retrieve(key);
        if (result.isError()) {
            return Result<std::vector<uint8_t>>::error(result.errorCode(),
                                                       result.errorMessage());
        }
        const auto& str = result.value();
        return Result<std::vector<uint8_t>>::success(
            std::vector<uint8_t>(str.begin(), str.end()));
    }

    Result<void> remove(std::string_view key) override {
        GError* error = nullptr;
        std::string keyStr(key);

        secret_password_clear_sync(schema_, nullptr, &error, "key",
                                   keyStr.c_str(), nullptr);

        if (error) {
            g_error_free(error);
        }

        return Result<void>::success();
    }

    bool exists(std::string_view key) override {
        auto result = retrieve(key);
        return result.isSuccess();
    }

    Result<std::vector<std::string>> getAllKeys() override {
        return Result<std::vector<std::string>>::success(
            std::vector<std::string>{});
    }

    Result<void> clear() override { return Result<void>::success(); }

    StorageBackend getBackend() const noexcept override {
        return StorageBackend::System;
    }

    std::string getLocation() const override { return "Linux Secret Service"; }

private:
    std::string appName_;
    SecretSchema* schema_ = nullptr;
};

#endif

/**
 * @brief In-memory storage for testing.
 */
class MemoryStorage : public SecureStorage {
public:
    Result<void> store(std::string_view key, std::string_view data) override {
        data_[std::string(key)] =
            std::vector<uint8_t>(data.begin(), data.end());
        return Result<void>::success();
    }

    Result<void> store(std::string_view key,
                       const std::vector<uint8_t>& data) override {
        data_[std::string(key)] = data;
        return Result<void>::success();
    }

    Result<std::string> retrieve(std::string_view key) override {
        auto it = data_.find(std::string(key));
        if (it == data_.end()) {
            return Result<std::string>::error(ErrorCode::StorageKeyNotFound,
                                              "Key not found");
        }
        return Result<std::string>::success(
            std::string(it->second.begin(), it->second.end()));
    }

    Result<std::vector<uint8_t>> retrieveBytes(std::string_view key) override {
        auto it = data_.find(std::string(key));
        if (it == data_.end()) {
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::StorageKeyNotFound, "Key not found");
        }
        return Result<std::vector<uint8_t>>::success(it->second);
    }

    Result<void> remove(std::string_view key) override {
        data_.erase(std::string(key));
        return Result<void>::success();
    }

    bool exists(std::string_view key) override {
        return data_.find(std::string(key)) != data_.end();
    }

    Result<std::vector<std::string>> getAllKeys() override {
        std::vector<std::string> keys;
        for (const auto& pair : data_) {
            keys.push_back(pair.first);
        }
        return Result<std::vector<std::string>>::success(std::move(keys));
    }

    Result<void> clear() override {
        data_.clear();
        return Result<void>::success();
    }

    StorageBackend getBackend() const noexcept override {
        return StorageBackend::Memory;
    }

    std::string getLocation() const override { return "In-Memory"; }

private:
    std::unordered_map<std::string, std::vector<uint8_t>> data_;
};

std::unique_ptr<SecureStorage> SecureStorage::create(
    const StorageOptions& options) {
    StorageBackend backend = options.backend;

    if (backend == StorageBackend::Auto) {
        // Try system storage first, fall back to file
        if (isBackendAvailable(StorageBackend::System)) {
            backend = StorageBackend::System;
        } else {
            backend = StorageBackend::File;
        }
    }

    switch (backend) {
        case StorageBackend::System:
#if defined(_WIN32)
            spdlog::info("Using Windows Credential Manager for secure storage");
            return std::make_unique<WindowsSecureStorage>(options.appName);
#elif defined(__APPLE__)
            spdlog::info("Using macOS Keychain for secure storage");
            return std::make_unique<MacSecureStorage>(options.appName);
#elif defined(__linux__) && defined(HAS_LIBSECRET)
            spdlog::info("Using Linux Secret Service for secure storage");
            return std::make_unique<LinuxSecureStorage>(options.appName);
#else
            spdlog::warn(
                "System keychain not available, falling back to file storage");
            return std::make_unique<FileStorage>(options.appName,
                                                 options.storagePath);
#endif

        case StorageBackend::File:
            spdlog::info("Using file-based secure storage");
            return std::make_unique<FileStorage>(options.appName,
                                                 options.storagePath);

        case StorageBackend::Memory:
            spdlog::info("Using in-memory storage (not persistent)");
            return std::make_unique<MemoryStorage>();

        default:
            return std::make_unique<FileStorage>(options.appName,
                                                 options.storagePath);
    }
}

bool SecureStorage::isBackendAvailable(StorageBackend backend) noexcept {
    switch (backend) {
        case StorageBackend::System:
#if defined(_WIN32) || defined(__APPLE__)
            return true;
#elif defined(__linux__) && defined(HAS_LIBSECRET)
            return true;
#else
            return false;
#endif

        case StorageBackend::File:
            return true;

        case StorageBackend::Memory:
            return true;

        default:
            return false;
    }
}

}  // namespace atom::secret
