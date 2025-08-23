/**
 * @file simple_secret_example.cpp
 * @brief Simple example demonstrating basic secure storage functionality
 *
 * This example shows how to:
 * - Create and use secure storage
 * - Store and retrieve encrypted data
 * - Manage keys and secrets securely
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>
#include <string>
#include <memory>

// Atom Secret module headers
#include "atom/secret/storage.hpp"

using namespace atom::secret;

/**
 * @brief Demonstrates basic secure storage operations
 */
void secureStorageExample() {
    std::cout << "\n=== Secure Storage Example ===\n";

    try {
        // Create a secure storage instance
        std::cout << "Creating secure storage instance...\n";
        auto storage = SecureStorage::create("AtomSecretExample");

        if (!storage) {
            std::cout << "Failed to create secure storage instance!\n";
            return;
        }

        std::cout << "Secure storage created successfully!\n";

        // Test data to store
        std::string key1 = "user_password";
        std::string value1 = "my_super_secret_password_123";
        std::string key2 = "api_key";
        std::string value2 = "sk-1234567890abcdef";
        std::string key3 = "database_url";
        std::string value3 = "postgresql://user:pass@localhost:5432/mydb";

        // Store some secrets
        std::cout << "\nStoring secrets...\n";

        if (storage->store(key1, value1)) {
            std::cout << "✓ Stored: " << key1 << "\n";
        } else {
            std::cout << "✗ Failed to store: " << key1 << "\n";
        }

        if (storage->store(key2, value2)) {
            std::cout << "✓ Stored: " << key2 << "\n";
        } else {
            std::cout << "✗ Failed to store: " << key2 << "\n";
        }

        if (storage->store(key3, value3)) {
            std::cout << "✓ Stored: " << key3 << "\n";
        } else {
            std::cout << "✗ Failed to store: " << key3 << "\n";
        }

        // List all stored keys
        std::cout << "\nListing all stored keys:\n";
        auto keys = storage->getAllKeys();
        for (const auto& key : keys) {
            std::cout << "  - " << key << "\n";
        }

        // Retrieve and verify stored data
        std::cout << "\nRetrieving stored secrets:\n";

        auto retrieved1 = storage->retrieve(key1);
        if (!retrieved1.empty()) {
            std::cout << "✓ Retrieved " << key1 << ": " << retrieved1 << "\n";
            if (retrieved1 == value1) {
                std::cout << "  ✓ Value matches original!\n";
            } else {
                std::cout << "  ✗ Value doesn't match original!\n";
            }
        } else {
            std::cout << "✗ Failed to retrieve: " << key1 << "\n";
        }

        auto retrieved2 = storage->retrieve(key2);
        if (!retrieved2.empty()) {
            std::cout << "✓ Retrieved " << key2 << ": " << retrieved2 << "\n";
        } else {
            std::cout << "✗ Failed to retrieve: " << key2 << "\n";
        }

        // Test removing a key
        std::cout << "\nRemoving a secret...\n";
        if (storage->remove(key2)) {
            std::cout << "✓ Removed: " << key2 << "\n";
        } else {
            std::cout << "✗ Failed to remove: " << key2 << "\n";
        }

        // Verify removal
        auto retrievedAfterRemoval = storage->retrieve(key2);
        if (retrievedAfterRemoval.empty()) {
            std::cout << "✓ Confirmed: " << key2 << " was removed\n";
        } else {
            std::cout << "✗ " << key2 << " still exists after removal\n";
        }

        // List keys again to show the change
        std::cout << "\nFinal list of stored keys:\n";
        keys = storage->getAllKeys();
        for (const auto& key : keys) {
            std::cout << "  - " << key << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in secure storage example: " << e.what() << "\n";
    }
}



/**
 * @brief Main function demonstrating secure storage capabilities
 */
int main() {
    std::cout << "=== Atom Secret Management Module Example ===\n";
    std::cout << "Demonstrating secure storage functionality...\n";

    try {
        // Run the secure storage example
        secureStorageExample();

        std::cout << "\n=== Example Completed Successfully ===\n";
        std::cout << "The secret module provides:\n";
        std::cout << "  ✓ Secure storage for sensitive data\n";
        std::cout << "  ✓ Platform-specific encryption\n";
        std::cout << "  ✓ Key-value storage interface\n";
        std::cout << "  ✓ Cross-platform compatibility\n";
        std::cout << "  ✓ Automatic encryption/decryption\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
