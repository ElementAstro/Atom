#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "atom/secret/password.hpp"

using namespace atom::secret;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StartsWith;

// Test fixture for PasswordManager tests
class PasswordManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test password manager
        manager = std::make_unique<PasswordManager>();

        // Setup test values
        masterPassword = "TestMasterPassword123!";
        testPlatformKey = "TestPlatform";
        testEntry = {
            /* password */ "TestPassword123!",
            /* username */ "testuser@example.com",
            /* url */ "https://example.com/login",
            /* notes */ "Test account notes",
            /* category */ PasswordCategory::Personal,
            /* created */ std::chrono::system_clock::now(),
            /* modified */ std::chrono::system_clock::now(),
            /* previousPasswords */ {"OldPassword1!", "OldPassword2@"}};

        // Define test export file path
        exportFilePath = "test_password_export.json";

        // Initialize the password manager with default settings
        ASSERT_TRUE(manager->initialize(masterPassword));
    }

    void TearDown() override {
        // Lock and clean up
        if (manager) {
            manager->lock();
        }
        manager.reset();

        // Clean up any created export file
        std::remove(exportFilePath.c_str());
    }

    // Helper function to create a test entry with a given password
    PasswordEntry createTestEntry(const std::string& password) {
        PasswordEntry entry = testEntry;
        entry.password = password;
        return entry;
    }

    std::unique_ptr<PasswordManager> manager;
    std::string masterPassword;
    std::string testPlatformKey;
    PasswordEntry testEntry;
    std::string exportFilePath;
};

// Test initialization of the password manager
TEST_F(PasswordManagerTest, Initialization) {
    // Create a new manager for this test
    auto newManager = std::make_unique<PasswordManager>();

    // Test with valid master password
    EXPECT_TRUE(newManager->initialize(masterPassword));

    // Test with empty master password (should fail)
    newManager = std::make_unique<PasswordManager>();
    EXPECT_FALSE(newManager->initialize(""));

    // Test with custom settings
    newManager = std::make_unique<PasswordManager>();
    PasswordManagerSettings settings;
    settings.autoLockTimeoutSeconds = 60;
    settings.minPasswordLength = 16;
    settings.encryptionOptions.keyIterations = 20000;
    EXPECT_TRUE(newManager->initialize(masterPassword, settings));

    // Verify settings were applied
    PasswordManagerSettings appliedSettings = newManager->getSettings();
    EXPECT_EQ(appliedSettings.autoLockTimeoutSeconds, 60);
    EXPECT_EQ(appliedSettings.minPasswordLength, 16);
    EXPECT_EQ(appliedSettings.encryptionOptions.keyIterations, 20000);
}

// Test locking and unlocking
TEST_F(PasswordManagerTest, LockAndUnlock) {
    // Check that the manager is initially unlocked after initialization

    // Lock the manager
    manager->lock();

    // Try to store a password while locked (should fail)
    EXPECT_FALSE(manager->storePassword(testPlatformKey, testEntry));

    // Unlock with incorrect password (should fail)
    EXPECT_FALSE(manager->unlock("WrongPassword"));

    // Unlock with correct password (should succeed)
    EXPECT_TRUE(manager->unlock(masterPassword));

    // Now storing should work
    EXPECT_TRUE(manager->storePassword(testPlatformKey, testEntry));
}

// Test auto-locking after timeout
TEST_F(PasswordManagerTest, AutoLock) {
    // Set a very short auto-lock timeout
    PasswordManagerSettings settings = manager->getSettings();
    settings.autoLockTimeoutSeconds = 1;  // 1 second
    manager->updateSettings(settings);

    // Store a password
    EXPECT_TRUE(manager->storePassword(testPlatformKey, testEntry));

    // Wait for auto-lock timeout
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Manager should be locked now, try to store another password (should fail)
    EXPECT_FALSE(manager->storePassword("AnotherKey", testEntry));

    // Unlock again
    EXPECT_TRUE(manager->unlock(masterPassword));
}

// Test storing and retrieving passwords
TEST_F(PasswordManagerTest, StoreAndRetrievePassword) {
    // Store a password
    EXPECT_TRUE(manager->storePassword(testPlatformKey, testEntry));

    // Retrieve and verify
    PasswordEntry retrievedEntry = manager->retrievePassword(testPlatformKey);
    EXPECT_EQ(retrievedEntry.password, testEntry.password);
    EXPECT_EQ(retrievedEntry.username, testEntry.username);
    EXPECT_EQ(retrievedEntry.url, testEntry.url);
    EXPECT_EQ(retrievedEntry.notes, testEntry.notes);
    EXPECT_EQ(retrievedEntry.category, testEntry.category);
    EXPECT_EQ(retrievedEntry.previousPasswords.size(),
              testEntry.previousPasswords.size());

    // Test retrieval of a non-existent key
    PasswordEntry emptyEntry = manager->retrievePassword("NonExistentKey");
    EXPECT_TRUE(emptyEntry.password.empty());
}

// Test password deletion
TEST_F(PasswordManagerTest, DeletePassword) {
    // Store a password
    EXPECT_TRUE(manager->storePassword(testPlatformKey, testEntry));

    // Verify it exists
    EXPECT_FALSE(manager->retrievePassword(testPlatformKey).password.empty());

    // Delete it
    EXPECT_TRUE(manager->deletePassword(testPlatformKey));

    // Verify it's gone
    EXPECT_TRUE(manager->retrievePassword(testPlatformKey).password.empty());

    // Try deleting a non-existent key
    EXPECT_FALSE(manager->deletePassword("NonExistentKey"));
}

// Test getting all platform keys
TEST_F(PasswordManagerTest, GetAllPlatformKeys) {
    // Initially there should be no keys
    EXPECT_THAT(manager->getAllPlatformKeys(), IsEmpty());

    // Store a few passwords
    EXPECT_TRUE(manager->storePassword("Key1", testEntry));
    EXPECT_TRUE(manager->storePassword("Key2", testEntry));
    EXPECT_TRUE(manager->storePassword("Key3", testEntry));

    // Get all keys and verify
    std::vector<std::string> keys = manager->getAllPlatformKeys();
    EXPECT_THAT(keys, SizeIs(3));
    EXPECT_THAT(keys, Contains("Key1"));
    EXPECT_THAT(keys, Contains("Key2"));
    EXPECT_THAT(keys, Contains("Key3"));
}

// Test searching passwords
TEST_F(PasswordManagerTest, SearchPasswords) {
    // Create and store entries with different attributes
    PasswordEntry entry1 = testEntry;
    entry1.username = "user1@example.com";
    EXPECT_TRUE(manager->storePassword("Entry1", entry1));

    PasswordEntry entry2 = testEntry;
    entry2.username = "user2@gmail.com";
    entry2.url = "https://gmail.com";
    EXPECT_TRUE(manager->storePassword("Entry2", entry2));

    PasswordEntry entry3 = testEntry;
    entry3.username = "user3@yahoo.com";
    entry3.notes = "Important account";
    EXPECT_TRUE(manager->storePassword("Entry3", entry3));

    // Search by username
    std::vector<std::string> results = manager->searchPasswords("user1");
    EXPECT_THAT(results, ElementsAre("Entry1"));

    // Search by domain
    results = manager->searchPasswords("gmail");
    EXPECT_THAT(results, ElementsAre("Entry2"));

    // Search by notes
    results = manager->searchPasswords("Important");
    EXPECT_THAT(results, ElementsAre("Entry3"));

    // Search with no results
    results = manager->searchPasswords("nonexistent");
    EXPECT_THAT(results, IsEmpty());

    // Search with empty query (should return all)
    results = manager->searchPasswords("");
    EXPECT_THAT(results, SizeIs(3));
}

// Test filtering by category
TEST_F(PasswordManagerTest, FilterByCategory) {
    // Create and store entries with different categories
    PasswordEntry personalEntry = testEntry;
    personalEntry.category = PasswordCategory::Personal;
    EXPECT_TRUE(manager->storePassword("Personal", personalEntry));

    PasswordEntry workEntry = testEntry;
    workEntry.category = PasswordCategory::Work;
    EXPECT_TRUE(manager->storePassword("Work", workEntry));

    PasswordEntry financeEntry = testEntry;
    financeEntry.category = PasswordCategory::Finance;
    EXPECT_TRUE(manager->storePassword("Finance", financeEntry));

    // Filter by Personal category
    std::vector<std::string> results =
        manager->filterByCategory(PasswordCategory::Personal);
    EXPECT_THAT(results, ElementsAre("Personal"));

    // Filter by Work category
    results = manager->filterByCategory(PasswordCategory::Work);
    EXPECT_THAT(results, ElementsAre("Work"));

    // Filter by Finance category
    results = manager->filterByCategory(PasswordCategory::Finance);
    EXPECT_THAT(results, ElementsAre("Finance"));

    // Filter by category with no entries
    results = manager->filterByCategory(PasswordCategory::Entertainment);
    EXPECT_THAT(results, IsEmpty());
}

// Test password generation
TEST_F(PasswordManagerTest, GeneratePassword) {
    // Generate a default password
    std::string password = manager->generatePassword();
    EXPECT_EQ(password.length(), 16);  // Default length
    EXPECT_TRUE(std::any_of(password.begin(), password.end(), ::isupper));
    EXPECT_TRUE(std::any_of(password.begin(), password.end(), ::islower));
    EXPECT_TRUE(std::any_of(password.begin(), password.end(), ::isdigit));
    EXPECT_TRUE(std::any_of(password.begin(), password.end(),
                            [](char c) { return !::isalnum(c); }));

    // Generate a custom length password
    password = manager->generatePassword(20);
    EXPECT_EQ(password.length(), 20);

    // Generate a password with only lowercase
    password = manager->generatePassword(12, false, false, false);
    EXPECT_EQ(password.length(), 12);
    EXPECT_FALSE(std::any_of(password.begin(), password.end(), ::isupper));
    EXPECT_FALSE(std::any_of(password.begin(), password.end(), ::isdigit));
    EXPECT_FALSE(std::any_of(password.begin(), password.end(),
                             [](char c) { return !::isalnum(c); }));

    // Generate a password with numbers but no special chars
    password = manager->generatePassword(12, false, true, false);
    EXPECT_EQ(password.length(), 12);
    EXPECT_FALSE(std::any_of(password.begin(), password.end(), ::isupper));
    EXPECT_TRUE(std::any_of(password.begin(), password.end(), ::isdigit));
    EXPECT_FALSE(std::any_of(password.begin(), password.end(),
                             [](char c) { return !::isalnum(c); }));
}

// Test password strength evaluation
TEST_F(PasswordManagerTest, EvaluatePasswordStrength) {
    // Very weak password (too short)
    EXPECT_EQ(manager->evaluatePasswordStrength("abc123"),
              PasswordStrength::VeryWeak);

    // Weak password (only lowercase and numbers)
    EXPECT_EQ(manager->evaluatePasswordStrength("abcdefgh123456"),
              PasswordStrength::Weak);

    // Medium password (mixed case and numbers)
    EXPECT_EQ(manager->evaluatePasswordStrength("Abcdefgh123456"),
              PasswordStrength::Medium);

    // Strong password (mixed case, numbers, and special)
    EXPECT_EQ(manager->evaluatePasswordStrength("Abcdefgh123456!"),
              PasswordStrength::Strong);

    // Very strong password (long, mixed case, numbers, special)
    EXPECT_EQ(manager->evaluatePasswordStrength("Abcdefgh123456!@#$%^&*()"),
              PasswordStrength::VeryStrong);

    // Password with repeating patterns (should be downgraded)
    EXPECT_EQ(manager->evaluatePasswordStrength("AAAbbbCCC123!!!"),
              PasswordStrength::Medium);

    // Password with common sequences (should be downgraded)
    EXPECT_EQ(manager->evaluatePasswordStrength("Abcdefg123456!"),
              PasswordStrength::Medium);
}

// Test exporting and importing passwords
TEST_F(PasswordManagerTest, ExportAndImportPasswords) {
    // Store some test passwords
    EXPECT_TRUE(manager->storePassword("Key1", createTestEntry("Password1")));
    EXPECT_TRUE(manager->storePassword("Key2", createTestEntry("Password2")));
    EXPECT_TRUE(manager->storePassword("Key3", createTestEntry("Password3")));

    // Export passwords
    std::string exportPassword = "ExportPassword123!";
    EXPECT_TRUE(manager->exportPasswords(exportFilePath, exportPassword));

    // Verify export file exists
    std::ifstream exportFile(exportFilePath);
    EXPECT_TRUE(exportFile.good());
    exportFile.close();

    // Create a new manager for import testing
    auto importManager = std::make_unique<PasswordManager>();
    EXPECT_TRUE(importManager->initialize(masterPassword));

    // Import passwords
    EXPECT_TRUE(importManager->importPasswords(exportFilePath, exportPassword));

    // Verify imported passwords
    std::vector<std::string> keys = importManager->getAllPlatformKeys();
    EXPECT_THAT(keys, SizeIs(3));
    EXPECT_THAT(keys, Contains("Key1"));
    EXPECT_THAT(keys, Contains("Key2"));
    EXPECT_THAT(keys, Contains("Key3"));

    // Check imported password content
    PasswordEntry entry = importManager->retrievePassword("Key1");
    EXPECT_EQ(entry.password, "Password1");
}

// Test changing the master password
TEST_F(PasswordManagerTest, ChangeMasterPassword) {
    // Store a password
    EXPECT_TRUE(manager->storePassword(testPlatformKey, testEntry));

    // Change master password
    std::string newMasterPassword = "NewMasterPassword456!";
    EXPECT_TRUE(
        manager->changeMasterPassword(masterPassword, newMasterPassword));

    // Lock and try to unlock with old password (should fail)
    manager->lock();
    EXPECT_FALSE(manager->unlock(masterPassword));

    // Unlock with new password
    EXPECT_TRUE(manager->unlock(newMasterPassword));

    // Verify passwords are still accessible
    PasswordEntry retrievedEntry = manager->retrievePassword(testPlatformKey);
    EXPECT_EQ(retrievedEntry.password, testEntry.password);
}

// Test password expiry checking
TEST_F(PasswordManagerTest, CheckExpiredPasswords) {
    // Create test entries with different modification times
    PasswordEntry recentEntry = testEntry;
    recentEntry.modified = std::chrono::system_clock::now();
    EXPECT_TRUE(manager->storePassword("Recent", recentEntry));

    PasswordEntry oldEntry = testEntry;
    // Set modified date to 100 days ago (older than default 90 days expiry)
    oldEntry.modified =
        std::chrono::system_clock::now() - std::chrono::hours(24 * 100);
    EXPECT_TRUE(manager->storePassword("Old", oldEntry));

    // Check for expired passwords
    std::vector<std::string> expired = manager->checkExpiredPasswords();
    EXPECT_THAT(expired, ElementsAre("Old"));

    // Disable expiry notifications and check again
    PasswordManagerSettings settings = manager->getSettings();
    settings.notifyOnPasswordExpiry = false;
    manager->updateSettings(settings);

    expired = manager->checkExpiredPasswords();
    EXPECT_THAT(expired, IsEmpty());
}

// Test activity callback
TEST_F(PasswordManagerTest, ActivityCallback) {
    bool callbackCalled = false;

    // Set activity callback
    manager->setActivityCallback(
        [&callbackCalled]() { callbackCalled = true; });

    // Perform an action that should trigger activity
    manager->generatePassword();

    // Check callback was called
    EXPECT_TRUE(callbackCalled);
}

// Test updating settings
TEST_F(PasswordManagerTest, UpdateSettings) {
    // Update settings
    PasswordManagerSettings newSettings;
    newSettings.autoLockTimeoutSeconds = 600;
    newSettings.minPasswordLength = 16;
    newSettings.requireSpecialChars = false;
    newSettings.encryptionOptions.keyIterations = 20000;

    manager->updateSettings(newSettings);

    // Verify settings were updated
    PasswordManagerSettings updatedSettings = manager->getSettings();
    EXPECT_EQ(updatedSettings.autoLockTimeoutSeconds, 600);
    EXPECT_EQ(updatedSettings.minPasswordLength, 16);
    EXPECT_FALSE(updatedSettings.requireSpecialChars);
    EXPECT_EQ(updatedSettings.encryptionOptions.keyIterations, 20000);
}

// Test error handling for file operations
TEST_F(PasswordManagerTest, FileOperationErrors) {
    // Try to export to an invalid file path
    EXPECT_FALSE(
        manager->exportPasswords("/invalid/path/file.json", "password"));

    // Try to import from a non-existent file
    EXPECT_FALSE(manager->importPasswords("nonexistent_file.json", "password"));

    // Try to import with wrong password
    // First, create a valid export
    EXPECT_TRUE(manager->storePassword(testPlatformKey, testEntry));
    EXPECT_TRUE(manager->exportPasswords(exportFilePath, "correctPassword"));

    // Try to import with wrong password
    EXPECT_FALSE(manager->importPasswords(exportFilePath, "wrongPassword"));
}

// Test handling of empty and special platform keys
TEST_F(PasswordManagerTest, SpecialPlatformKeys) {
    // Empty key
    EXPECT_FALSE(manager->storePassword("", testEntry));

    // Very long key
    std::string longKey(1024, 'A');
    EXPECT_TRUE(manager->storePassword(longKey, testEntry));
    EXPECT_FALSE(manager->retrievePassword(longKey).password.empty());

    // Key with special characters
    std::string specialKey = "Key!@#$%^&*()_+";
    EXPECT_TRUE(manager->storePassword(specialKey, testEntry));
    EXPECT_FALSE(manager->retrievePassword(specialKey).password.empty());
}

// Platform-specific storage tests
#if defined(_WIN32)
TEST_F(PasswordManagerTest, WindowsCredentialManager) {
    // This test only runs on Windows
    // Test Windows-specific credential storage
    // (Basic functionality already tested in general tests)
}
#elif defined(__APPLE__)
TEST_F(PasswordManagerTest, MacOSKeychain) {
    // This test only runs on macOS
    // Test macOS-specific keychain storage
    // (Basic functionality already tested in general tests)
}
#elif defined(__linux__)
TEST_F(PasswordManagerTest, LinuxKeyring) {
    // This test only runs on Linux
    // Test Linux-specific keyring storage
    // (Basic functionality already tested in general tests)
}
#endif

// Test thread safety
TEST_F(PasswordManagerTest, ThreadSafety) {
    // Create multiple threads that access the manager simultaneously
    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i]() {
            std::string key = "ThreadKey" + std::to_string(i);
            PasswordEntry entry = testEntry;
            entry.username = "thread" + std::to_string(i) + "@example.com";

            // Store a password
            EXPECT_TRUE(manager->storePassword(key, entry));

            // Retrieve it
            PasswordEntry retrievedEntry = manager->retrievePassword(key);
            EXPECT_EQ(retrievedEntry.username, entry.username);
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all entries were stored correctly
    std::vector<std::string> keys = manager->getAllPlatformKeys();
    EXPECT_EQ(keys.size(), numThreads);

    for (int i = 0; i < numThreads; ++i) {
        std::string key = "ThreadKey" + std::to_string(i);
        PasswordEntry entry = manager->retrievePassword(key);
        EXPECT_EQ(entry.username,
                  "thread" + std::to_string(i) + "@example.com");
    }
}

// Test border cases and error handling
TEST_F(PasswordManagerTest, BorderCasesAndErrorHandling) {
    // Test with extremely large entry
    PasswordEntry largeEntry = testEntry;
    largeEntry.notes = std::string(1024 * 1024, 'A');  // 1MB of notes
    EXPECT_TRUE(manager->storePassword("LargeEntry", largeEntry));

    // Test with empty entry
    PasswordEntry emptyEntry;
    EXPECT_TRUE(manager->storePassword("EmptyEntry", emptyEntry));

    // Test with entry containing special characters
    PasswordEntry specialEntry = testEntry;
    specialEntry.username = "user@例子.测试";  // Unicode username
    specialEntry.password = "パスワード123!";  // Unicode password
    EXPECT_TRUE(manager->storePassword("SpecialEntry", specialEntry));

    // Retrieve and verify
    PasswordEntry retrievedSpecialEntry =
        manager->retrievePassword("SpecialEntry");
    EXPECT_EQ(retrievedSpecialEntry.username, specialEntry.username);
    EXPECT_EQ(retrievedSpecialEntry.password, specialEntry.password);
}
