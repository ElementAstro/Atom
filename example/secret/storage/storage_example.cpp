/*
 * storage_example.cpp
 *
 * Demonstrates secure storage usage in the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <filesystem>
#include <iostream>

#include "atom/secret/core/types.hpp"
#include "atom/secret/storage/file_storage.hpp"
#include "atom/secret/storage/storage.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret Storage Example ===" << std::endl
              << std::endl;

    // ========================================================================
    // Memory Storage
    // ========================================================================
    std::cout << "--- Memory Storage ---" << std::endl;

    auto memStorage = SecureStorage::create(StorageBackend::Memory);
    if (!memStorage) {
        std::cerr << "Failed to create memory storage" << std::endl;
        return 1;
    }

    // Store string data
    auto storeResult =
        memStorage->storeString("api_key", "sk-1234567890abcdef");
    if (storeResult.isSuccess()) {
        std::cout << "Stored API key successfully" << std::endl;
    }

    // Retrieve string data
    auto retrieveResult = memStorage->retrieveString("api_key");
    if (retrieveResult.isSuccess()) {
        std::cout << "Retrieved API key: " << retrieveResult.value()
                  << std::endl;
    }

    // Store binary data
    std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04, 0x05};
    memStorage->store("binary_key", binaryData);

    auto binaryResult = memStorage->retrieve("binary_key");
    if (binaryResult.isSuccess()) {
        std::cout << "Retrieved binary data: "
                  << bytesToHex(binaryResult.value()) << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // File Storage
    // ========================================================================
    std::cout << "--- File Storage ---" << std::endl;

    auto tempDir =
        std::filesystem::temp_directory_path() / "atom_secret_example";
    std::filesystem::create_directories(tempDir);
    auto storagePath = tempDir / "secrets.dat";

    auto fileStorage =
        SecureStorage::create(StorageBackend::File, storagePath.string());
    if (!fileStorage) {
        std::cerr << "Failed to create file storage" << std::endl;
        return 1;
    }

    std::cout << "Storage location: " << fileStorage->getLocation()
              << std::endl;

    // Store multiple items
    fileStorage->storeString("db_password", "super_secret_db_pass");
    fileStorage->storeString("jwt_secret", "jwt_signing_key_12345");
    fileStorage->storeString("encryption_key", "aes256_master_key");

    std::cout << "Stored 3 secrets" << std::endl;

    // List keys
    auto keysResult = fileStorage->listKeys();
    if (keysResult.isSuccess()) {
        std::cout << "Stored keys:" << std::endl;
        for (const auto& key : keysResult.value()) {
            std::cout << "  - " << key << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Check Existence
    // ========================================================================
    std::cout << "--- Check Existence ---" << std::endl;

    std::cout << "db_password exists: "
              << (fileStorage->exists("db_password") ? "Yes" : "No")
              << std::endl;
    std::cout << "nonexistent exists: "
              << (fileStorage->exists("nonexistent") ? "Yes" : "No")
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Update and Remove
    // ========================================================================
    std::cout << "--- Update and Remove ---" << std::endl;

    // Update existing
    fileStorage->storeString("db_password", "new_super_secret_pass");
    auto updated = fileStorage->retrieveString("db_password");
    if (updated.isSuccess()) {
        std::cout << "Updated db_password: " << updated.value() << std::endl;
    }

    // Remove
    auto removeResult = fileStorage->remove("jwt_secret");
    if (removeResult.isSuccess()) {
        std::cout << "Removed jwt_secret" << std::endl;
    }

    std::cout << "jwt_secret exists after removal: "
              << (fileStorage->exists("jwt_secret") ? "Yes" : "No")
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Persistence Test
    // ========================================================================
    std::cout << "--- Persistence Test ---" << std::endl;

    // Close and reopen
    fileStorage.reset();

    auto reopenedStorage =
        SecureStorage::create(StorageBackend::File, storagePath.string());
    if (reopenedStorage) {
        auto persistedResult = reopenedStorage->retrieveString("db_password");
        if (persistedResult.isSuccess()) {
            std::cout << "Persisted db_password: " << persistedResult.value()
                      << std::endl;
        }

        auto persistedKeys = reopenedStorage->listKeys();
        if (persistedKeys.isSuccess()) {
            std::cout << "Persisted keys count: "
                      << persistedKeys.value().size() << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Clear Storage
    // ========================================================================
    std::cout << "--- Clear Storage ---" << std::endl;

    if (reopenedStorage) {
        auto clearResult = reopenedStorage->clear();
        if (clearResult.isSuccess()) {
            std::cout << "Storage cleared" << std::endl;
        }

        auto afterClear = reopenedStorage->listKeys();
        if (afterClear.isSuccess()) {
            std::cout << "Keys after clear: " << afterClear.value().size()
                      << std::endl;
        }
    }

    // Cleanup
    std::filesystem::remove_all(tempDir);

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
