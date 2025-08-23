/**
 * @file secure_storage_example.cpp
 * @brief Comprehensive example demonstrating the Atom Secret module's secure storage capabilities
 * 
 * This example shows how to:
 * - Create platform-specific secure storage instances
 * - Store and retrieve encrypted data securely
 * - Handle different platforms (Windows Credential Manager, macOS Keychain, Linux Secret Service)
 * - Manage password entries with metadata
 * - Use encryption options and settings
 * 
 * @author Max Qian
 * @date 2024-12-19
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

// Atom Secret module headers
#include "atom/secret/storage.hpp"
#include "atom/secret/password_entry.hpp"
#include "atom/secret/common.hpp"

using namespace atom::secret;

/**
 * @brief Demonstrates basic secure storage operations
 */
void basicSecureStorageExample() {
    std::cout << "\n=== Basic Secure Storage Example ===\n";
    
    try {
        // Create a secure storage instance for our application
        auto storage = SecureStorage::create("AtomExampleApp");
        
        if (!storage) {
            std::cerr << "Failed to create secure storage instance\n";
            return;
        }
        
        // Store some sensitive data
        std::string apiKey = "sk-1234567890abcdef";
        std::string databasePassword = "MySecureDBPassword123!";
        std::string userToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...";
        
        std::cout << "Storing sensitive data...\n";
        
        bool success = true;
        success &= storage->store("api_key", apiKey);
        success &= storage->store("db_password", databasePassword);
        success &= storage->store("user_token", userToken);
        
        if (success) {
            std::cout << "✓ All data stored successfully\n";
        } else {
            std::cout << "✗ Some data failed to store\n";
            return;
        }
        
        // Retrieve the stored data
        std::cout << "\nRetrieving stored data...\n";
        
        std::string retrievedApiKey = storage->retrieve("api_key");
        std::string retrievedDbPassword = storage->retrieve("db_password");
        std::string retrievedUserToken = storage->retrieve("user_token");
        
        // Verify the data matches
        if (retrievedApiKey == apiKey) {
            std::cout << "✓ API key retrieved correctly\n";
        } else {
            std::cout << "✗ API key mismatch\n";
        }
        
        if (retrievedDbPassword == databasePassword) {
            std::cout << "✓ Database password retrieved correctly\n";
        } else {
            std::cout << "✗ Database password mismatch\n";
        }
        
        if (retrievedUserToken == userToken) {
            std::cout << "✓ User token retrieved correctly\n";
        } else {
            std::cout << "✗ User token mismatch\n";
        }
        
        // List all stored keys
        std::cout << "\nListing all stored keys:\n";
        auto keys = storage->getAllKeys();
        for (const auto& key : keys) {
            std::cout << "  - " << key << "\n";
        }
        
        // Clean up - remove the test data
        std::cout << "\nCleaning up test data...\n";
        storage->remove("api_key");
        storage->remove("db_password");
        storage->remove("user_token");
        std::cout << "✓ Test data removed\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in basic secure storage example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates password entry management
 */
void passwordEntryExample() {
    std::cout << "\n=== Password Entry Management Example ===\n";
    
    try {
        auto storage = SecureStorage::create("AtomPasswordManager");
        
        if (!storage) {
            std::cerr << "Failed to create secure storage instance\n";
            return;
        }
        
        // Create a password entry with metadata
        PasswordEntry entry;
        entry.password = "MySecurePassword123!";
        entry.username = "john.doe@example.com";
        entry.url = "https://example.com";
        entry.title = "Example Website Account";
        entry.notes = "Main account for example.com services";
        entry.category = PasswordCategory::Work;
        entry.tags = {"work", "important", "main-account"};
        entry.created = std::chrono::system_clock::now();
        entry.modified = entry.created;
        
        // Set expiration to 90 days from now
        entry.expires = entry.created + std::chrono::hours(24 * 90);
        
        std::cout << "Created password entry:\n";
        std::cout << "  Title: " << entry.title << "\n";
        std::cout << "  Username: " << entry.username << "\n";
        std::cout << "  URL: " << entry.url << "\n";
        std::cout << "  Category: " << static_cast<int>(entry.category) << "\n";
        std::cout << "  Tags: ";
        for (const auto& tag : entry.tags) {
            std::cout << tag << " ";
        }
        std::cout << "\n";
        
        // In a real application, you would serialize the PasswordEntry
        // and store it securely. For this example, we'll just store the password.
        std::string entryKey = "password_entry_" + entry.title;
        bool stored = storage->store(entryKey, entry.password);
        
        if (stored) {
            std::cout << "✓ Password entry stored successfully\n";
        } else {
            std::cout << "✗ Failed to store password entry\n";
            return;
        }
        
        // Retrieve and verify
        std::string retrievedPassword = storage->retrieve(entryKey);
        if (retrievedPassword == entry.password) {
            std::cout << "✓ Password entry retrieved successfully\n";
        } else {
            std::cout << "✗ Password entry retrieval failed\n";
        }
        
        // Clean up
        storage->remove(entryKey);
        std::cout << "✓ Password entry cleaned up\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in password entry example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates encryption options and settings
 */
void encryptionOptionsExample() {
    std::cout << "\n=== Encryption Options Example ===\n";
    
    // Create encryption options with different settings
    EncryptionOptions options;
    options.useHardwareAcceleration = true;
    options.keyIterations = 150000;  // Higher iteration count for better security
    options.encryptionMethod = EncryptionOptions::Method::AES_GCM;
    
    std::cout << "Encryption Options:\n";
    std::cout << "  Hardware Acceleration: " << (options.useHardwareAcceleration ? "Enabled" : "Disabled") << "\n";
    std::cout << "  Key Iterations: " << options.keyIterations << "\n";
    std::cout << "  Encryption Method: ";
    
    switch (options.encryptionMethod) {
        case EncryptionOptions::Method::AES_GCM:
            std::cout << "AES-GCM (AEAD)\n";
            break;
        case EncryptionOptions::Method::AES_CBC:
            std::cout << "AES-CBC\n";
            break;
        case EncryptionOptions::Method::CHACHA20_POLY1305:
            std::cout << "ChaCha20-Poly1305\n";
            break;
    }
    
    // Create password manager settings
    PasswordManagerSettings settings;
    settings.autoLockTimeoutSeconds = 600;  // 10 minutes
    settings.notifyOnPasswordExpiry = true;
    settings.passwordExpiryDays = 120;  // 4 months
    settings.minPasswordLength = 16;
    settings.requireSpecialChars = true;
    settings.requireNumbers = true;
    settings.requireMixedCase = true;
    settings.encryptionOptions = options;
    
    std::cout << "\nPassword Manager Settings:\n";
    std::cout << "  Auto-lock timeout: " << settings.autoLockTimeoutSeconds << " seconds\n";
    std::cout << "  Password expiry: " << settings.passwordExpiryDays << " days\n";
    std::cout << "  Minimum password length: " << settings.minPasswordLength << "\n";
    std::cout << "  Require special chars: " << (settings.requireSpecialChars ? "Yes" : "No") << "\n";
    std::cout << "  Require numbers: " << (settings.requireNumbers ? "Yes" : "No") << "\n";
    std::cout << "  Require mixed case: " << (settings.requireMixedCase ? "Yes" : "No") << "\n";
}

/**
 * @brief Demonstrates error handling and edge cases
 */
void errorHandlingExample() {
    std::cout << "\n=== Error Handling Example ===\n";
    
    try {
        auto storage = SecureStorage::create("AtomErrorTest");
        
        if (!storage) {
            std::cerr << "Failed to create secure storage instance\n";
            return;
        }
        
        // Test storing with empty key
        std::cout << "Testing empty key storage...\n";
        bool result = storage->store("", "some_data");
        std::cout << "Empty key storage result: " << (result ? "Success" : "Failed (expected)") << "\n";
        
        // Test retrieving non-existent key
        std::cout << "\nTesting non-existent key retrieval...\n";
        std::string nonExistent = storage->retrieve("non_existent_key_12345");
        std::cout << "Non-existent key retrieval: " << (nonExistent.empty() ? "Empty (expected)" : "Got data") << "\n";
        
        // Test removing non-existent key
        std::cout << "\nTesting non-existent key removal...\n";
        bool removeResult = storage->remove("non_existent_key_12345");
        std::cout << "Non-existent key removal: " << (removeResult ? "Success" : "Failed") << "\n";
        
        // Test with very long key
        std::cout << "\nTesting very long key...\n";
        std::string longKey(1000, 'a');  // 1000 character key
        bool longKeyResult = storage->store(longKey, "test_data");
        std::cout << "Long key storage: " << (longKeyResult ? "Success" : "Failed") << "\n";
        
        if (longKeyResult) {
            std::string retrieved = storage->retrieve(longKey);
            std::cout << "Long key retrieval: " << (retrieved == "test_data" ? "Success" : "Failed") << "\n";
            storage->remove(longKey);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in error handling example: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating all secret module capabilities
 */
int main() {
    std::cout << "=== Atom Secret Module Example ===\n";
    std::cout << "Demonstrating secure storage capabilities across platforms\n";
    
    try {
        // Run all examples
        basicSecureStorageExample();
        passwordEntryExample();
        encryptionOptionsExample();
        errorHandlingExample();
        
        std::cout << "\n=== All Examples Completed Successfully ===\n";
        std::cout << "The secret module provides:\n";
        std::cout << "  ✓ Cross-platform secure storage\n";
        std::cout << "  ✓ Windows Credential Manager integration\n";
        std::cout << "  ✓ macOS Keychain integration\n";
        std::cout << "  ✓ Linux Secret Service integration\n";
        std::cout << "  ✓ File-based fallback storage\n";
        std::cout << "  ✓ Password entry management\n";
        std::cout << "  ✓ Configurable encryption options\n";
        std::cout << "  ✓ Robust error handling\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
