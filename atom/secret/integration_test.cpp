#include "password_manager.hpp"
#include "storage.hpp"

#include <cassert>
#include <iostream>

namespace atom::secret {

/**
 * @brief Simple integration test to verify storage backend compatibility.
 */
class IntegrationTest {
public:
    static bool runBasicTests() {
        std::cout << "Running basic integration tests...\n";

        // Test 1: Storage creation
        auto storage = SecureStorage::create("atom-test");
        if (!storage) {
            std::cerr << "Failed to create storage backend\n";
            return false;
        }
        std::cout << "✓ Storage backend created successfully\n";

        // Test 2: Basic storage operations
        const std::string testKey = "test_key";
        const std::string testData = "test_data_12345";

        if (!storage->store(testKey, testData)) {
            std::cerr << "Failed to store test data\n";
            return false;
        }
        std::cout << "✓ Data stored successfully\n";

        std::string retrievedData = storage->retrieve(testKey);
        if (retrievedData != testData) {
            std::cerr << "Retrieved data doesn't match stored data\n";
            std::cerr << "Expected: " << testData << "\n";
            std::cerr << "Got: " << retrievedData << "\n";
            return false;
        }
        std::cout << "✓ Data retrieved successfully\n";

        // Test 3: getAllKeys functionality
        auto keys = storage->getAllKeys();
        bool foundKey = false;
        for (const auto& key : keys) {
            if (key == testKey) {
                foundKey = true;
                break;
            }
        }
        if (!foundKey) {
            std::cerr << "Test key not found in getAllKeys() result\n";
            return false;
        }
        std::cout << "✓ getAllKeys() working correctly\n";

        // Test 4: Data removal
        if (!storage->remove(testKey)) {
            std::cerr << "Failed to remove test data\n";
            return false;
        }
        std::cout << "✓ Data removed successfully\n";

        // Verify removal
        std::string removedData = storage->retrieve(testKey);
        if (!removedData.empty()) {
            std::cerr << "Data still exists after removal\n";
            return false;
        }
        std::cout << "✓ Data removal verified\n";

        return true;
    }

    static bool runPasswordManagerTests() {
        std::cout << "\nRunning PasswordManager integration tests...\n";

        // Test 1: PasswordManager initialization
        PasswordManager manager;
        PasswordManagerSettings settings;
        settings.minPasswordLength = 12;
        settings.autoLockTimeoutSeconds = 300;

        if (!manager.initialize("test_master_password_123", settings)) {
            std::cerr << "Failed to initialize PasswordManager\n";
            return false;
        }
        std::cout << "✓ PasswordManager initialized successfully\n";

        // Test 2: Password storage and retrieval
        PasswordEntry entry;
        entry.title = "Test Website";
        entry.username = "testuser";
        entry.password = "secure_password_123";
        entry.url = "https://example.com";
        entry.notes = "Test notes";
        entry.category = PasswordCategory::Personal;
        entry.created = std::chrono::system_clock::now();
        entry.modified = entry.created;

        if (!manager.storePassword("test_entry", entry)) {
            std::cerr << "Failed to store password entry\n";
            return false;
        }
        std::cout << "✓ Password entry stored successfully\n";

        PasswordEntry retrievedEntry = manager.retrievePassword("test_entry");
        if (retrievedEntry.password != entry.password ||
            retrievedEntry.username != entry.username ||
            retrievedEntry.title != entry.title) {
            std::cerr << "Retrieved entry doesn't match stored entry\n";
            return false;
        }
        std::cout << "✓ Password entry retrieved successfully\n";

        // Test 3: Search functionality
        auto searchResults = manager.searchPasswords("Test");
        if (searchResults.empty()) {
            std::cerr << "Search didn't find the test entry\n";
            return false;
        }
        std::cout << "✓ Search functionality working\n";

        // Test 4: Password generation
        std::string generatedPassword =
            manager.generatePassword(16, true, true, true);
        if (generatedPassword.empty() || generatedPassword.length() != 16) {
            std::cerr << "Password generation failed\n";
            return false;
        }
        std::cout << "✓ Password generation working\n";

        // Test 5: Lock/unlock functionality
        manager.lock();
        if (!manager.isLocked()) {
            std::cerr << "Manager should be locked\n";
            return false;
        }
        std::cout << "✓ Lock functionality working\n";

        if (!manager.unlock("test_master_password_123")) {
            std::cerr << "Failed to unlock manager\n";
            return false;
        }
        if (manager.isLocked()) {
            std::cerr << "Manager should be unlocked\n";
            return false;
        }
        std::cout << "✓ Unlock functionality working\n";

        // Test 6: Export/Import functionality
        auto exportResult = manager.exportToJson();
        if (exportResult.isError()) {
            std::cerr << "Failed to export data: " << exportResult.error()
                      << "\n";
            return false;
        }
        std::cout << "✓ Export functionality working\n";

        // Clean up
        manager.removePassword("test_entry");

        return true;
    }

    static bool runAllTests() {
        std::cout << "=== Atom Secret Module Integration Tests ===\n\n";

        bool basicTestsPass = runBasicTests();
        bool managerTestsPass = runPasswordManagerTests();

        std::cout << "\n=== Test Results ===\n";
        std::cout << "Basic Storage Tests: "
                  << (basicTestsPass ? "PASS" : "FAIL") << "\n";
        std::cout << "PasswordManager Tests: "
                  << (managerTestsPass ? "PASS" : "FAIL") << "\n";

        bool allTestsPass = basicTestsPass && managerTestsPass;
        std::cout << "Overall Result: " << (allTestsPass ? "PASS" : "FAIL")
                  << "\n";

        return allTestsPass;
    }
};

}  // namespace atom::secret

// Simple test runner
int main() {
    try {
        bool success = atom::secret::IntegrationTest::runAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
